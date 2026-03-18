#include "os.h"

#include "dbg.h"
#include "fs.h"
#include "mem.h"
#include "str.h"
#include "windows.h"

#include <cstdlib>
#include <cstring>
#include <iterator> // std::size

#if defined(CONF_FAMILY_UNIX)
#include "io.h"

#include <sys/utsname.h> // uname, utsname
#include <unistd.h> // execlp, fork

#if defined(CONF_PLATFORM_MACOS)
#include <CoreFoundation/CoreFoundation.h>
#endif
#elif defined(CONF_FAMILY_WINDOWS)
#include "mem.h"

#include <objbase.h> // required for shellapi.h
#include <shellapi.h> // ShellExecuteExW
#include <windows.h>

#include <cfenv> // fesetenv
#include <cstring> // std::wstring
#else
#error NOT IMPLEMENTED
#endif

#if defined(CONF_FAMILY_UNIX)
static bool GetAbsoluteOpenPath(const char *pPath, char *pBuffer, size_t BufferSize)
{
	if(!fs_is_relative_path(pPath))
	{
		str_copy(pBuffer, pPath, BufferSize);
		return true;
	}

	if(!fs_getcwd(pBuffer, BufferSize))
	{
		return false;
	}

	str_append(pBuffer, "/", BufferSize);
	str_append(pBuffer, pPath, BufferSize);
	return true;
}

static bool IsExecutableAvailable(const char *pExecutable)
{
	if(std::strchr(pExecutable, '/') != nullptr)
	{
		return access(pExecutable, X_OK) == 0;
	}

	const char *pPathEnv = std::getenv("PATH");
	if(pPathEnv == nullptr || pPathEnv[0] == '\0')
	{
		return false;
	}

	const char *pSegmentStart = pPathEnv;
	while(true)
	{
		const char *pSegmentEnd = std::strchr(pSegmentStart, ':');
		const size_t SegmentLength = pSegmentEnd != nullptr ? (size_t)(pSegmentEnd - pSegmentStart) : std::strlen(pSegmentStart);
		if(SegmentLength == 0)
		{
			if(access(pExecutable, X_OK) == 0)
			{
				return true;
			}
		}
		else if(SegmentLength + 1 + std::strlen(pExecutable) < IO_MAX_PATH_LENGTH)
		{
			char aExecutablePath[IO_MAX_PATH_LENGTH];
			std::memcpy(aExecutablePath, pSegmentStart, SegmentLength);
			aExecutablePath[SegmentLength] = '/';
			str_copy(aExecutablePath + SegmentLength + 1, pExecutable, sizeof(aExecutablePath) - SegmentLength - 1);
			if(access(aExecutablePath, X_OK) == 0)
			{
				return true;
			}
		}

		if(pSegmentEnd == nullptr)
		{
			break;
		}
		pSegmentStart = pSegmentEnd + 1;
	}

	return false;
}

static int ForkAndExecCommand(const char *pExecutable, const char *const *ppArguments)
{
	const int Pid = fork();
	if(Pid < 0)
	{
		return 0;
	}
	if(Pid == 0)
	{
		execvp(pExecutable, const_cast<char *const *>(ppArguments));
		_exit(1);
	}
	return 1;
}

static int TryOpenWithExecutable(const char *pExecutable, const char *pArgument1 = nullptr, const char *pArgument2 = nullptr, const char *pArgument3 = nullptr)
{
	if(!IsExecutableAvailable(pExecutable))
	{
		return 0;
	}

	const char *apArguments[5];
	int NumArguments = 0;
	apArguments[NumArguments++] = pExecutable;
	if(pArgument1 != nullptr)
	{
		apArguments[NumArguments++] = pArgument1;
	}
	if(pArgument2 != nullptr)
	{
		apArguments[NumArguments++] = pArgument2;
	}
	if(pArgument3 != nullptr)
	{
		apArguments[NumArguments++] = pArgument3;
	}
	apArguments[NumArguments] = nullptr;

	return ForkAndExecCommand(pExecutable, apArguments);
}
#endif

void cmdline_fix(int *argc, const char ***argv)
{
#if defined(CONF_FAMILY_WINDOWS)
	int wide_argc = 0;
	WCHAR **wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
	dbg_assert(wide_argv != NULL, "CommandLineToArgvW failure");
	dbg_assert(wide_argc > 0, "Invalid argc value");

	int total_size = 0;

	for(int i = 0; i < wide_argc; i++)
	{
		int size = WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, nullptr, 0, nullptr, nullptr);
		dbg_assert(size != 0, "WideCharToMultiByte failure");
		total_size += size;
	}

	char **new_argv = (char **)malloc((wide_argc + 1) * sizeof(*new_argv));
	new_argv[0] = (char *)malloc(total_size);
	mem_zero(new_argv[0], total_size);

	int remaining_size = total_size;
	for(int i = 0; i < wide_argc; i++)
	{
		int size = WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, new_argv[i], remaining_size, nullptr, nullptr);
		dbg_assert(size != 0, "WideCharToMultiByte failure");

		remaining_size -= size;
		new_argv[i + 1] = new_argv[i] + size;
	}

	LocalFree(wide_argv);
	new_argv[wide_argc] = nullptr;
	*argc = wide_argc;
	*argv = (const char **)new_argv;
#endif
}

void cmdline_free(int argc, const char **argv)
{
#if defined(CONF_FAMILY_WINDOWS)
	free((void *)*argv);
	free((char **)argv);
#endif
}

#if !defined(CONF_PLATFORM_ANDROID)
int os_open_link(const char *link)
{
#if defined(CONF_FAMILY_WINDOWS)
	const std::wstring wide_link = windows_utf8_to_wide(link);

	SHELLEXECUTEINFOW info;
	mem_zero(&info, sizeof(SHELLEXECUTEINFOW));
	info.cbSize = sizeof(SHELLEXECUTEINFOW);
	info.lpVerb = nullptr; // NULL to use the default verb, as "open" may not be available
	info.lpFile = wide_link.c_str();
	info.nShow = SW_SHOWNORMAL;
	// The SEE_MASK_NOASYNC flag ensures that the ShellExecuteEx function
	// finishes its DDE conversation before it returns, so it's not necessary
	// to pump messages in the calling thread.
	// The SEE_MASK_FLAG_NO_UI flag suppresses error messages that would pop up
	// when the link cannot be opened, e.g. when a folder does not exist.
	// The SEE_MASK_ASYNCOK flag is not used. It would allow the call to
	// ShellExecuteEx to return earlier, but it also prevents us from doing
	// our own error handling, as the function would always return TRUE.
	info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
	// Save and restore the FPU control word because ShellExecute might change it
	fenv_t floating_point_environment;
	int fegetenv_result = fegetenv(&floating_point_environment);
	BOOL success = ShellExecuteExW(&info);
	if(fegetenv_result == 0)
		fesetenv(&floating_point_environment);
	return success;
#elif defined(CONF_PLATFORM_LINUX)
	const int pid = fork();
	if(pid == 0)
	{
		execlp("xdg-open", "xdg-open", link, nullptr);
		_exit(1);
	}
	return pid > 0;
#elif defined(CONF_FAMILY_UNIX)
	const int pid = fork();
	if(pid == 0)
	{
		execlp("open", "open", link, nullptr);
		_exit(1);
	}
	return pid > 0;
#endif
}

int os_open_file(const char *path)
{
#if defined(CONF_PLATFORM_MACOS)
	return os_open_link(path);
#elif defined(CONF_PLATFORM_LINUX)
	char aAbsolutePath[IO_MAX_PATH_LENGTH];
	if(!GetAbsoluteOpenPath(path, aAbsolutePath, sizeof(aAbsolutePath)))
	{
		return 0;
	}

	if(fs_is_dir(aAbsolutePath))
	{
		if(TryOpenWithExecutable("nautilus", aAbsolutePath) ||
			TryOpenWithExecutable("nemo", aAbsolutePath) ||
			TryOpenWithExecutable("thunar", aAbsolutePath) ||
			TryOpenWithExecutable("dolphin", aAbsolutePath) ||
			TryOpenWithExecutable("pcmanfm", aAbsolutePath) ||
			TryOpenWithExecutable("pcmanfm-qt", aAbsolutePath) ||
			TryOpenWithExecutable("caja", aAbsolutePath) ||
			TryOpenWithExecutable("konqueror", aAbsolutePath) ||
			TryOpenWithExecutable("exo-open", "--launch", "FileManager", aAbsolutePath) ||
			TryOpenWithExecutable("kioclient5", "exec", aAbsolutePath) ||
			TryOpenWithExecutable("kioclient", "exec", aAbsolutePath) ||
			TryOpenWithExecutable("gio", "open", aAbsolutePath) ||
			TryOpenWithExecutable("gvfs-open", aAbsolutePath))
		{
			return 1;
		}
	}

	return os_open_link(aAbsolutePath);
#elif defined(CONF_FAMILY_WINDOWS)
	return os_open_link(path);
#else
	char aAbsolutePath[IO_MAX_PATH_LENGTH];
	if(!GetAbsoluteOpenPath(path, aAbsolutePath, sizeof(aAbsolutePath)))
	{
		return 0;
	}
	return os_open_link(aAbsolutePath);
#endif
}
#endif // !defined(CONF_PLATFORM_ANDROID)

bool os_version_str(char *version, size_t length)
{
#if defined(CONF_FAMILY_WINDOWS)
	const WCHAR *module_path = L"kernel32.dll";
	DWORD handle;
	DWORD size = GetFileVersionInfoSizeW(module_path, &handle);
	if(!size)
	{
		return false;
	}
	void *data = malloc(size);
	if(!GetFileVersionInfoW(module_path, handle, size, data))
	{
		free(data);
		return false;
	}
	VS_FIXEDFILEINFO *fileinfo;
	UINT unused;
	if(!VerQueryValueW(data, L"\\", (void **)&fileinfo, &unused))
	{
		free(data);
		return false;
	}
	str_format(version, length, "Windows %hu.%hu.%hu.%hu",
		HIWORD(fileinfo->dwProductVersionMS),
		LOWORD(fileinfo->dwProductVersionMS),
		HIWORD(fileinfo->dwProductVersionLS),
		LOWORD(fileinfo->dwProductVersionLS));
	free(data);
	return true;
#else
	struct utsname u;
	if(uname(&u))
	{
		return false;
	}
	char extra[128];
	extra[0] = 0;

	do
	{
		IOHANDLE os_release = io_open("/etc/os-release", IOFLAG_READ);
		char buf[4096];
		int read;
		int offset;
		char *newline;
		if(!os_release)
		{
			break;
		}
		read = io_read(os_release, buf, sizeof(buf) - 1);
		io_close(os_release);
		buf[read] = 0;
		if(str_startswith(buf, "PRETTY_NAME="))
		{
			offset = 0;
		}
		else
		{
			const char *found = str_find(buf, "\nPRETTY_NAME=");
			if(!found)
			{
				break;
			}
			offset = found - buf + 1;
		}
		newline = (char *)str_find(buf + offset, "\n");
		if(newline)
		{
			*newline = 0;
		}
		str_format(extra, sizeof(extra), "; %s", buf + offset + 12);
	} while(false);

	str_format(version, length, "%s %s (%s, %s)%s", u.sysname, u.release, u.machine, u.version, extra);
	return true;
#endif
}

void os_locale_str(char *locale, size_t length)
{
#if defined(CONF_FAMILY_WINDOWS)
	wchar_t wide_buffer[LOCALE_NAME_MAX_LENGTH];
	dbg_assert(GetUserDefaultLocaleName(wide_buffer, std::size(wide_buffer)) > 0, "GetUserDefaultLocaleName failure");

	const std::optional<std::string> buffer = windows_wide_to_utf8(wide_buffer);
	dbg_assert(buffer.has_value(), "GetUserDefaultLocaleName returned invalid UTF-16");
	str_copy(locale, buffer.value().c_str(), length);
#elif defined(CONF_PLATFORM_MACOS)
	CFLocaleRef locale_ref = CFLocaleCopyCurrent();
	CFStringRef locale_identifier_ref = static_cast<CFStringRef>(CFLocaleGetValue(locale_ref, kCFLocaleIdentifier));

	// Count number of UTF16 codepoints, +1 for zero-termination.
	// Assume maximum possible length for encoding as UTF-8.
	CFIndex locale_identifier_size = (UTF8_BYTE_LENGTH * CFStringGetLength(locale_identifier_ref) + 1) * sizeof(char);
	char *locale_identifier = (char *)malloc(locale_identifier_size);
	dbg_assert(CFStringGetCString(locale_identifier_ref, locale_identifier, locale_identifier_size, kCFStringEncodingUTF8), "CFStringGetCString failure");

	str_copy(locale, locale_identifier, length);

	free(locale_identifier);
	CFRelease(locale_ref);
#else
	static const char *ENV_VARIABLES[] = {
		"LC_ALL",
		"LC_MESSAGES",
		"LANG",
	};

	locale[0] = '\0';
	for(const char *env_variable : ENV_VARIABLES)
	{
		const char *env_value = getenv(env_variable);
		if(env_value)
		{
			str_copy(locale, env_value, length);
			break;
		}
	}
#endif

	// Ensure RFC 3066 format:
	// - use hyphens instead of underscores
	// - truncate locale string after first non-standard letter
	for(int i = 0; i < str_length(locale); ++i)
	{
		if(locale[i] == '_')
		{
			locale[i] = '-';
		}
		else if(locale[i] != '-' && !(locale[i] >= 'a' && locale[i] <= 'z') && !(locale[i] >= 'A' && locale[i] <= 'Z') && !(str_isnum(locale[i])))
		{
			locale[i] = '\0';
			break;
		}
	}

	// Use default if we could not determine the locale,
	// i.e. if only the C or POSIX locale is available.
	if(locale[0] == '\0' || str_comp(locale, "C") == 0 || str_comp(locale, "POSIX") == 0)
		str_copy(locale, "en-US", length);
}
