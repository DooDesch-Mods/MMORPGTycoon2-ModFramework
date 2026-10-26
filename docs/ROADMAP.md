# MT2 Mod Framework roadmap

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/modframework](https://support.doodesch.de/modframework).

Included in [Framework 0.4.0](https://github.com/DooDesch-Mods/MMORPGTycoon2-ModFramework/releases/tag/v0.4.0). New gameplay tools are experimental. World-changing Lua gameplay operations currently run in singleplayer.

## Available now: Start creating

- Lua mods and C++ plugins
- Lua reload and mod apps
- Static GLB scenery

## Next priorities: Connect more mods

- Shared Lua world actions
- Native UI improvements
- Better content workflows

## Longer term: More ways to create

- Animated models
- C# mod support
- Broader multiplayer APIs

Priorities can change. No release dates are promised.

## Feature coverage

| Feature | 0.4.0 | What is covered |
| --- | --- | --- |
| Lua mods and reload | ✅ Available | Use a text editor; reload scripts without restarting. A rejected replacement keeps the previous mod running. [Start here](quickstart.md). |
| Native C++ plugins | ✅ Available | Build with the native SDK. Plugin updates require a game restart. [SDK guide](reference/native.md). |
| In-game mod apps | ✅ Available | Open Lua and native mods through the central Mods window, with optional game-desktop icons. Lua apps can use native windows, PNGs and document layouts. [Lua app guide](recipes/lua-apps.md). |
| Static GLB scenery | ✅ Available | Import static models and textures into the scenery catalog. Keep required packs installed for your saves. [Scenery guide](guides/scenery.md). |
| Camera, objects and world actions | ✅ Available | Use the documented camera, time, object and checked world-operation APIs. [API reference](reference/index.md). |
| Character design | ◇ Experimental | Create character definitions and variants; edit stats, appearance, weapons and abilities from Lua. [Gameplay guide](experimental/gameplay.md). |
| Actors, quests and Monster Zones | ◇ Experimental | Place, move and remove supported NPCs, Monsters and Bosses. Edit quests, Boss phases and Monster Zones, including native paint and erase tools. |
| Dungeon tools | ◇ Experimental | Edit Dungeon cells, themes, lighting, paint, gizmos and instances. Building operations use the active editable Dungeon template. |
| Modular building tools | ◇ Experimental | Place modular prefabs, read layouts, add or remove blocks and entrances, and use the supported native Undo and Redo routes. |
| Terrain, paths and scenery tools | ◇ Experimental | Edit Regions and terrain, build paths and curved walls, and paint, group or transform scenery through native game operations. |
| Management and development tools | ◇ Experimental | Manage fees, loans, staff, marketing, game releases, research projects and server networks from Lua. |
| Native patches | ◇ Experimental | Observe named game calls with ordered callbacks and conflict diagnostics. [Patch guide](experimental/patches.md). |
| Vortex installation | ✅ Available | Manage the player Runtime and mod ZIPs using the separate Vortex Extension. [Install guide](https://github.com/DooDesch-Mods/MMORPGTycoon2-ModFramework/blob/main/integrations/vortex/README.md). |
| Editor types and agent docs | ✅ Available | Generated Lua types, complete examples and a versioned machine contract. [Agent entry point](agents/start.md). |
| Multiplayer mods | ◐ Partial | Run the separate Multiplayer mod with its matching requirements. Other mods need their own synchronization; the new Lua gameplay mutations currently run in singleplayer. |
| Animated models and C# | ○ Planned | Future extensions. The current release uses static scenery, Lua and native C++. |

## How coverage is counted

Each listed feature area counts once. The categories have equal weight. Partial and experimental areas do not count as fully available.

- Available: 7/16 (43.8%)
- Experimental: 7/16 (43.8%)
- Partial: 1/16 (6.2%)
- Planned: 1/16 (6.2%)

This measures the listed feature areas, not overall project completion or stability. The denominator changes when the inventory changes.
