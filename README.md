# <img src="https://tags.quomy.win/catproject.png" width="40" style="vertical-align: middle;"> CatClient

A custom [DDNet](https://ddnet.org/) client with quality-of-life features, visual enhancements, and community integration. Available at [teeworlds.xyz/client](https://teeworlds.xyz/client).

> Built on top of the TClient source code. Inspired by BestClient and RushieClient.  
> Developers: [quomy](https://github.com/quomy), [rXelelo](https://github.com/rXelelo)

---

## Features

### 🎨 Visuals
- Custom background images and animated backgrounds (shown in menu and/or in-game)
- Custom aspect ratio / stretch with per-element HUD exclusion bitmask
- Enhanced laser rendering
- Tiny tees, cursor scaling, white feet rendering
- Custom cursor and arrow textures (loaded from CatData)
- Chat animations, modern UI widgets, horizontal settings tabs
- Configurable effect and sound muting (bitmask-based)
- Moving tiles in entities view

### 🛍️ Texture Shop (CatData integration)
An in-client browser for [CatData](https://teeworlds.xyz/data) — the community texture library.
- Browse and search Tee Skins, Gun Packs, Particles, Entities, Emoticons, HUD, Cursors, Arrows
- Preview textures before downloading
- Auto-apply assets after download (`cc_shop_auto_set`)

### 🎙️ Voice Chat
Integrated proximity/team voice chat powered by the Opus audio codec.
- Push-to-talk or automatic voice activation
- Per-player volume control and muting
- Configurable mic gain, bitrate (up to 96 kbps), input/output device selection
- Microphone loopback check
- Nameplate microphone icon for talking players
- Custom voice server address

### ⚡ Performance & Input
- Fast input mode (CatClient or Saiko scheme) — uses input for prediction before next tick
- Improved antiping algorithm with configurable uncertainty scale
- Prediction margin adjustment while frozen
- Anti-latency tools: remove antiping in freeze, smooth prediction margin
- Mouse distance scaling for improved precision
- Mouse boundary limiting

### 🧩 Gameplay & Binds
- Auto team lock with configurable delay
- Auto weapon switch bind (two-weapon cycle)
- Auto lag message (triggered by ping or frame time threshold)
- Auto vote "no" when far on a map
- Auto name change near finish
- Auto reply when minimized or to muted players
- Bind wheel with configurable open animation
- Show player hit boxes (predicted / unpredicted)

### 👤 Profiles & Nametags
- Quick-switch player profiles (skin, name, clan, flag, colors, emote)
- CatClient nametags with extended player info
- Player ignore list (persisted across sessions)

### 🎬 Streamer Mode
- Hides sensitive UI elements based on a configurable flag bitmask
- Word blocklist for obscuring player names and chat
- Separate streamer settings tab

### 🌐 Misc
- Custom Discord RPC
- Background draw tool (freehand strokes, auto save/load)
- Custom communities support (configurable JSON URL)
- Anti-quit confirmation popup
- Server browser auto-refresh with configurable interval
- Filter server browser to show only servers with CatClient users online
- Custom font face selection
- UI scale (50–100%)
- First-run setup wizard

---

## Supported Platforms

|Platform ||
|-|-|
|Windows|✅|
|Linux|✅|
|macOS|✅|

## Links

- **Website** — [teeworlds.xyz/client](https://teeworlds.xyz/client)
- **Texture Library** — [teeworlds.xyz/data](https://teeworlds.xyz/data)
- **Discord** — [teeworlds.xyz/discord](https://teeworlds.xyz/discord)
---
- **BestClient** — [bestclient.fun](https://bestclient.fun)
- **RushieClient** — [rushie-client.ru](https://rushie-client.ru)