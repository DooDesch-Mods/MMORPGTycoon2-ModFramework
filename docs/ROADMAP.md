# MT2 Mod Framework roadmap

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/modframework](https://support.doodesch.de/modframework).

Included in [Framework 0.2.6](https://github.com/DooDesch-Mods/MMORPGTycoon2-ModFramework/releases/tag/v0.2.6). Experimental features can change between releases.

## Available now: Start creating

- Lua mods and C++ plugins
- Lua reload and mod apps
- Static GLB scenery

## Next priorities: Reach more of MT2

- More game operations
- Native UI and diagnostics
- Better content workflows

## Longer term: More ways to create

- Animated models
- C# mod support
- Broader multiplayer APIs

Priorities can change. No release dates are promised.

## Feature coverage

| Feature | 0.2.6 | What is covered |
| --- | --- | --- |
| Lua mods and reload | ✅ Available | Use a text editor; reload scripts without restarting. A rejected replacement keeps the previous mod running. [Start here](quickstart.md). |
| Native C++ plugins | ✅ Available | Build with the native SDK. Plugin updates require a game restart. [SDK guide](reference/native.md). |
| In-game mod apps | ✅ Available | Open Lua and native mods through the central Mods window, with optional game-desktop icons. Lua apps can use native windows, PNGs and document layouts. [Lua app guide](recipes/lua-apps.md). |
| Static GLB scenery | ✅ Available | Import static models and textures into the scenery catalog. Keep required packs installed for your saves. [Scenery guide](guides/scenery.md). |
| Camera, objects and world actions | ✅ Available | Use the documented camera, time, object and checked world-operation APIs. [API reference](reference/index.md). |
| Native patches | ◇ Experimental | Observe named game calls with ordered callbacks and conflict diagnostics. [Patch guide](experimental/patches.md). |
| Vortex installation | ✅ Available | Manage the player Runtime and mod ZIPs using the separate Vortex Extension. [Install guide](https://github.com/DooDesch-Mods/MMORPGTycoon2-ModFramework/blob/main/integrations/vortex/README.md). |
| Editor types and agent docs | ✅ Available | Generated Lua types, complete examples and a versioned machine contract. [Agent entry point](agents/start.md). |
| Multiplayer mods | ◐ Partial | Runtime for Multiplayer Early Beta 18, including the native right-click route. Other mods need explicit multiplayer integration; installing the Framework alone does not synchronize them. |
| Animated models and C# | ○ Planned | Future extensions. The current release uses static scenery, Lua and native C++. |

## How coverage is counted

Each listed feature area counts once. The categories have equal weight. Partial and experimental areas do not count as fully available.

- Available: 7/10 (70.0%)
- Experimental: 1/10 (10.0%)
- Partial: 1/10 (10.0%)
- Planned: 1/10 (10.0%)

This measures the listed feature areas, not overall project completion or stability. The denominator changes when the inventory changes.
