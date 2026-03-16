interface Env {
	DB: D1Database;
}

interface PresenceRow {
	clientId: number;
	dummyId: number | null;
	lastSeenAt: number;
}

interface PresenceRowWithServer extends PresenceRow {
	server: string;
}

interface RateLimitRow {
	windowStartedAt: number;
	requestCount: number;
}

interface UserPayload {
	clientId: number;
	dummyId: number | null;
}

const ACTIVE_TTL_SECONDS = 10;
const CLEANUP_TTL_SECONDS = 40;
const RATE_LIMIT_WINDOW_SECONDS = 60;
const RATE_LIMIT_ENTRY_TTL_SECONDS = RATE_LIMIT_WINDOW_SECONDS * 5;
const READ_RATE_LIMIT = 180;
const WRITE_RATE_LIMIT = 90;
const MAX_SERVER_LENGTH = 128;
const MAX_USERS_PER_POST = 2;
const MAX_CLIENT_ID = 64;

function createHeaders(contentType: string): Headers {
	const HeadersOut = new Headers();
	HeadersOut.set("content-type", contentType);
	HeadersOut.set("access-control-allow-origin", "*");
	HeadersOut.set("access-control-allow-methods", "GET,POST,OPTIONS");
	HeadersOut.set("access-control-allow-headers", "content-type");
	HeadersOut.set("cache-control", "no-store");
	return HeadersOut;
}

function mergeHeaders(contentType: string, init: ResponseInit): Headers {
	const HeadersOut = createHeaders(contentType);
	if(init.headers) {
		const Incoming = new Headers(init.headers);
		Incoming.forEach((Value, Key) => HeadersOut.set(Key, Value));
	}
	return HeadersOut;
}

function json(Data: unknown, init: ResponseInit = {}): Response {
	return new Response(JSON.stringify(Data, null, 2), {
		...init,
		headers: mergeHeaders("application/json; charset=utf-8", init),
	});
}

function html(Body: string, init: ResponseInit = {}): Response {
	return new Response(Body, {
		...init,
		headers: mergeHeaders("text/html; charset=utf-8", init),
	});
}

function errorResponse(status: number, message: string, details?: unknown, init: ResponseInit = {}): Response {
	return json(
		{
			ok: false,
			error: message,
			details: details ?? null,
		},
		{ ...init, status },
	);
}

function normalizeString(Value: unknown, MaxLength: number): string {
	if(typeof Value !== "string") {
		return "";
	}
	return Value.trim().slice(0, MaxLength);
}

function isObject(Value: unknown): Value is Record<string, unknown> {
	return typeof Value === "object" && Value !== null;
}

function isValidClientId(Value: unknown): Value is number {
	return typeof Value === "number" && Number.isInteger(Value) && Value >= 0 && Value <= MAX_CLIENT_ID;
}

function normalizeDummyId(Value: unknown, ClientId: number): number | null {
	if(Value === null || Value === undefined) {
		return null;
	}
	if(!isValidClientId(Value) || Value === ClientId) {
		return null;
	}
	return Value;
}

function getClientIp(request: Request): string {
	const CfIp = normalizeString(request.headers.get("cf-connecting-ip"), 64);
	if(CfIp.length > 0) {
		return CfIp;
	}

	const ForwardedFor = normalizeString(request.headers.get("x-forwarded-for"), 256);
	if(ForwardedFor.length > 0) {
		const FirstAddress = ForwardedFor.split(",")[0]?.trim() ?? "";
		if(FirstAddress.length > 0) {
			return FirstAddress.slice(0, 64);
		}
	}

	return "unknown";
}

async function sha256Hex(Value: string): Promise<string> {
	const Encoded = new TextEncoder().encode(Value);
	const Digest = await crypto.subtle.digest("SHA-256", Encoded);
	return Array.from(new Uint8Array(Digest), (Byte) => Byte.toString(16).padStart(2, "0")).join("");
}

async function cleanupExpiredRows(env: Env, nowSeconds: number): Promise<void> {
	await env.DB.prepare("DELETE FROM cat_presence WHERE last_seen_at < ?")
		.bind(nowSeconds - CLEANUP_TTL_SECONDS)
		.run();
}

async function cleanupRateLimits(env: Env, nowSeconds: number): Promise<void> {
	await env.DB.prepare("DELETE FROM request_rate_limits WHERE last_seen_at < ?")
		.bind(nowSeconds - RATE_LIMIT_ENTRY_TTL_SECONDS)
		.run();
}

async function enforceRateLimit(request: Request, env: Env): Promise<Response | null> {
	const Url = new URL(request.url);
	const Bucket = `${Url.pathname}:${request.method === "GET" ? "read" : "write"}`;
	const Limit = request.method === "GET" ? READ_RATE_LIMIT : WRITE_RATE_LIMIT;
	const NowSeconds = Math.floor(Date.now() / 1000);

	await cleanupRateLimits(env, NowSeconds);

	const BucketKey = await sha256Hex(`${Bucket}:${getClientIp(request)}`);
	await env.DB.prepare(
		`INSERT INTO request_rate_limits (
			bucket_key,
			window_started_at,
			request_count,
			last_seen_at
		) VALUES (?, ?, 1, ?)
		ON CONFLICT(bucket_key) DO UPDATE SET
			request_count = CASE
				WHEN excluded.window_started_at - request_rate_limits.window_started_at >= ? THEN 1
				ELSE request_rate_limits.request_count + 1
			END,
			window_started_at = CASE
				WHEN excluded.window_started_at - request_rate_limits.window_started_at >= ? THEN excluded.window_started_at
				ELSE request_rate_limits.window_started_at
			END,
			last_seen_at = excluded.last_seen_at`,
	)
		.bind(BucketKey, NowSeconds, NowSeconds, RATE_LIMIT_WINDOW_SECONDS, RATE_LIMIT_WINDOW_SECONDS)
		.run();

	const Result = await env.DB.prepare(
		`SELECT
			window_started_at AS windowStartedAt,
			request_count AS requestCount
		FROM request_rate_limits
		WHERE bucket_key = ?`,
	)
		.bind(BucketKey)
		.all<RateLimitRow>();

	const Row = Result.results?.[0];
	if(!Row || Row.requestCount <= Limit) {
		return null;
	}

	const RetryAfter = Math.max(1, RATE_LIMIT_WINDOW_SECONDS - Math.max(0, NowSeconds - Row.windowStartedAt));
	return errorResponse(
		429,
		"too many requests from this IP",
		{
			limit: Limit,
			retryAfter: RetryAfter,
			windowSeconds: RATE_LIMIT_WINDOW_SECONDS,
		},
		{
			headers: {
				"retry-after": String(RetryAfter),
			},
		},
	);
}

function parseUsers(Value: unknown): UserPayload[] {
	if(!Array.isArray(Value)) {
		return [];
	}

	const Users: UserPayload[] = [];
	const SeenClientIds = new Set<number>();

	for(const Entry of Value.slice(0, MAX_USERS_PER_POST)) {
		if(!isObject(Entry) || !isValidClientId(Entry.clientId) || SeenClientIds.has(Entry.clientId)) {
			continue;
		}

		const DummyId = normalizeDummyId(Entry.dummyId, Entry.clientId);
		SeenClientIds.add(Entry.clientId);
		Users.push({
			clientId: Entry.clientId,
			dummyId: DummyId,
		});
	}

	return Users;
}

function renderUser(Row: PresenceRow) {
	return {
		clientId: Row.clientId,
		dummyId: Row.dummyId,
		lastSeenAt: Row.lastSeenAt,
	};
}

async function handleGetUsers(request: Request, env: Env): Promise<Response> {
	const Url = new URL(request.url);
	const Server = normalizeString(Url.searchParams.get("server"), MAX_SERVER_LENGTH);

	const NowSeconds = Math.floor(Date.now() / 1000);
	await cleanupExpiredRows(env, NowSeconds);

	if(Server.length === 0) {
		const Result = await env.DB.prepare(
			`SELECT
				server_address AS server,
				client_id AS clientId,
				dummy_id AS dummyId,
				last_seen_at AS lastSeenAt
			FROM cat_presence
			WHERE last_seen_at >= ?
			ORDER BY server_address ASC, client_id ASC`,
		)
			.bind(NowSeconds - ACTIVE_TTL_SECONDS)
			.all<PresenceRowWithServer>();

		const Rows = Result.results ?? [];
		const Servers = new Map<string, PresenceRow[]>();
		for(const Row of Rows) {
			const NormalizedServer = normalizeString(Row.server, MAX_SERVER_LENGTH);
			if(NormalizedServer.length === 0) {
				continue;
			}

			if(!Servers.has(NormalizedServer)) {
				Servers.set(NormalizedServer, []);
			}
			Servers.get(NormalizedServer)?.push({
				clientId: Row.clientId,
				dummyId: Row.dummyId,
				lastSeenAt: Row.lastSeenAt,
			});
		}

		return json({
			ok: true,
			ttlSeconds: ACTIVE_TTL_SECONDS,
			generatedAt: new Date(NowSeconds * 1000).toISOString(),
			totalUsers: Rows.length,
			servers: Array.from(Servers.entries()).map(([ServerAddress, Users]) => ({
				server: ServerAddress,
				users: Users.map(renderUser),
			})),
		});
	}

	const Result = await env.DB.prepare(
		`SELECT
			client_id AS clientId,
			dummy_id AS dummyId,
			last_seen_at AS lastSeenAt
		FROM cat_presence
		WHERE server_address = ?
			AND last_seen_at >= ?
		ORDER BY client_id ASC`,
	)
		.bind(Server, NowSeconds - ACTIVE_TTL_SECONDS)
		.all<PresenceRow>();

	return json({
		ok: true,
		server: Server,
		ttlSeconds: ACTIVE_TTL_SECONDS,
		generatedAt: new Date(NowSeconds * 1000).toISOString(),
		users: (Result.results ?? []).map(renderUser),
	});
}

async function handlePostUsers(request: Request, env: Env): Promise<Response> {
	let Payload: unknown;
	try {
		Payload = await request.json();
	} catch {
		return errorResponse(400, "not valid");
	}

	if(!isObject(Payload)) {
		return errorResponse(400, "not valid");
	}

	const Server = normalizeString(Payload.server, MAX_SERVER_LENGTH);
	if(Server.length === 0) {
		return errorResponse(400, "not valid");
	}

	const Users = parseUsers(Array.isArray(Payload.users) ? Payload.users : Payload.players);
	if(Users.length === 0) {
		return errorResponse(400, "not valid");
	}

	const RemoveRequested = Payload.remove === true;
	const NowSeconds = Math.floor(Date.now() / 1000);
	const Statements = [
		env.DB.prepare("DELETE FROM cat_presence WHERE last_seen_at < ?")
			.bind(NowSeconds - CLEANUP_TTL_SECONDS),
	];

	if(RemoveRequested) {
		const RemoveIds = new Set<number>();
		for(const User of Users) {
			RemoveIds.add(User.clientId);
			if(User.dummyId !== null) {
				RemoveIds.add(User.dummyId);
			}
		}

		for(const RemoveId of RemoveIds) {
			Statements.push(
				env.DB.prepare(
					`DELETE FROM cat_presence
					WHERE server_address = ?
						AND (client_id = ? OR dummy_id = ?)`,
				)
					.bind(Server, RemoveId, RemoveId),
			);
		}

		await env.DB.batch(Statements);

		return json({
			ok: true,
			server: Server,
			removed: Users.length,
			ttlSeconds: ACTIVE_TTL_SECONDS,
		});
	}

	for(const User of Users) {
		if(User.dummyId !== null) {
			Statements.push(
				env.DB.prepare("DELETE FROM cat_presence WHERE server_address = ? AND client_id = ?")
					.bind(Server, User.dummyId),
			);
		}

		Statements.push(
			env.DB.prepare(
				`INSERT INTO cat_presence (
					server_address,
					client_id,
					dummy_id,
					player_name,
					created_at,
					last_seen_at
				) VALUES (?, ?, ?, '', ?, ?)
				ON CONFLICT(server_address, client_id) DO UPDATE SET
					dummy_id = excluded.dummy_id,
					player_name = '',
					last_seen_at = excluded.last_seen_at`,
			)
				.bind(Server, User.clientId, User.dummyId, NowSeconds, NowSeconds),
		);
	}

	await env.DB.batch(Statements);

	return json({
		ok: true,
		server: Server,
		upserted: Users.length,
		ttlSeconds: ACTIVE_TTL_SECONDS,
	});
}

export default {
	async fetch(request: Request, env: Env): Promise<Response> {
		const Url = new URL(request.url);

		if(request.method === "OPTIONS") {
			return new Response(null, {
				status: 204,
				headers: createHeaders("text/plain; charset=utf-8"),
			});
		}

		const RateLimitResponse = await enforceRateLimit(request, env);
		if(RateLimitResponse !== null) {
			return RateLimitResponse;
		}

		if(Url.pathname === "/") {
			return errorResponse(405, "not allowed");
		}

		if(Url.pathname === "/users.json") {
			if(request.method === "GET") {
				return handleGetUsers(request, env);
			}
			if(request.method === "POST") {
				return handlePostUsers(request, env);
			}
			return errorResponse(405, "not allowed");
		}

		return errorResponse(404, "not found");
	},
};
