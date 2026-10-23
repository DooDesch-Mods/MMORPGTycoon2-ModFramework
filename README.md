# MT2 Mod Framework

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/modframework](https://support.doodesch.de/modframework).

![MT2 Mod Framework - Make room for your ideas](assets/brand/readme-banner.svg)

Build mods for **MMORPG Tycoon 2**. Start with Lua and a text editor, extend with C++, or bring your own static GLB scenery into the game.

**[Create your first mod](https://doodesch-mods.github.io/MMORPGTycoon2-ModFramework/quickstart.html)** · [Explore the API](https://doodesch-mods.github.io/MMORPGTycoon2-ModFramework/reference/index.html) · [Use an AI agent](https://doodesch-mods.github.io/MMORPGTycoon2-ModFramework/agents/start.html)

[Feature coverage](#what-can-i-build-today) | [Roadmap](#roadmap) | [Build and contribute](#for-contributors)

<!-- roadmap:start -->
## What can I build today?

Included in [Framework 0.3.2](https://github.com/DooDesch-Mods/MMORPGTycoon2-ModFramework/releases/tag/v0.3.2). New gameplay tools are experimental. World-changing Lua gameplay operations currently run in singleplayer.

<picture>
  <source media="(max-width: 600px)" srcset="https://raw.githubusercontent.com/DooDesch-Mods/MMORPGTycoon2-ModFramework/main/assets/brand/coverage-mobile.svg">
  <img src="assets/brand/coverage.svg" alt="Feature coverage: 7 available, 1 partial, 7 experimental, 1 planned, across 16 tracked areas" width="1200">
</picture>

Percentages count the listed areas equally. They describe feature coverage, not overall project completion or stability.

<details>
<summary>Feature details - what is included</summary>

| Feature | 0.3.2 | What is covered |
| --- | --- | --- |
| Lua mods and reload | ✅ Available | Use a text editor; reload scripts without restarting. A rejected replacement keeps the previous mod running. [Start here](docs/quickstart.md). |
| Native C++ plugins | ✅ Available | Build with the native SDK. Plugin updates require a game restart. [SDK guide](docs/reference/native.md). |
| In-game mod apps | ✅ Available | Open Lua and native mods through the central Mods window, with optional game-desktop icons. Lua apps can use native windows, PNGs and document layouts. [Lua app guide](docs/recipes/lua-apps.md). |
| Static GLB scenery | ✅ Available | Import static models and textures into the scenery catalog. Keep required packs installed for your saves. [Scenery guide](docs/guides/scenery.md). |
| Camera, objects and world actions | ✅ Available | Use the documented camera, time, object and checked world-operation APIs. [API reference](docs/reference/index.md). |
| Character design | ◇ Experimental | Create character definitions and variants; edit stats, appearance, weapons and abilities from Lua. [Gameplay guide](docs/experimental/gameplay.md). |
| Actors, quests and Monster Zones | ◇ Experimental | Place, move and remove supported NPCs, Monsters and Bosses. Edit quests, Boss phases and Monster Zones, including native paint and erase tools. |
| Dungeon tools | ◇ Experimental | Edit Dungeon cells, themes, lighting, paint, gizmos and instances. Building operations use the active editable Dungeon template. |
| Modular building tools | ◇ Experimental | Place modular prefabs, read layouts, add or remove blocks and entrances, and use the supported native Undo and Redo routes. |
| Terrain, paths and scenery tools | ◇ Experimental | Edit Regions and terrain, build paths and curved walls, and paint, group or transform scenery through native game operations. |
| Management and development tools | ◇ Experimental | Manage fees, loans, staff, marketing, game releases, research projects and server networks from Lua. |
| Native patches | ◇ Experimental | Observe named game calls with ordered callbacks and conflict diagnostics. [Patch guide](docs/experimental/patches.md). |
| Vortex installation | ✅ Available | Manage the player Runtime and mod ZIPs using the separate Vortex Extension. [Install guide](integrations/vortex/README.md). |
| Editor types and agent docs | ✅ Available | Generated Lua types, complete examples and a versioned machine contract. [Agent entry point](docs/agents/start.md). |
| Multiplayer mods | ◐ Partial | Run the separate Multiplayer mod with its matching requirements. Other mods need their own synchronization; the new Lua gameplay mutations currently run in singleplayer. |
| Animated models and C# | ○ Planned | Future extensions. The current release uses static scenery, Lua and native C++. |

</details>

## Roadmap

<picture>
  <source media="(max-width: 600px)" srcset="https://raw.githubusercontent.com/DooDesch-Mods/MMORPGTycoon2-ModFramework/main/assets/brand/roadmap-mobile.svg">
  <img src="assets/brand/roadmap.svg" alt="Roadmap: Available now - Start creating. Next priorities - Connect more mods. Longer term - More ways to create" width="1200">
</picture>

[Read the roadmap in text](docs/ROADMAP.md).
<!-- roadmap:end -->

## Install mods

Players need the **Runtime** from [Nexus](https://www.nexusmods.com/mmorpgtycoon2/mods/25) or [GitHub Releases](https://github.com/DooDesch-Mods/MMORPGTycoon2-ModFramework/releases). This is a development preview for Windows x64 and the [supported Steam game build](docs/compatibility.md).

1. Close the game. Install the [MMORPG Tycoon 2 Vortex Extension](https://www.nexusmods.com/site/mods/2465) and restart Vortex.
2. Manage MMORPG Tycoon 2 in Vortex. Install the Runtime ZIP and your mods, enable them and deploy.
3. Start the game through Steam. Open **Mods...** from its menu, **Mods** on the title-screen desktop, or press **F10**.

Prefer to install by hand? Follow the [runtime installation guide](packaging/runtime/README.md). For co-op, follow the Multiplayer mod's requirements; other mods need their own multiplayer support.

In Mods, change settings, check dependencies and errors, or reload a Lua mod. Select a mod app and choose **Open** to use its window. Developer tools are available from the same central menu.

![The native Mods app with mod selection and reload controls](docs/public/media/mods-menu.png)

## Make your own mod

Download the separate **MT2Framework-SDK** from [GitHub Releases](https://github.com/DooDesch-Mods/MMORPGTycoon2-ModFramework/releases). It includes the matching Runtime, portable helper, CLI, GLB importer, templates, editor types and offline documentation. Start with **Hello MT2** and the [first mod guide](docs/quickstart.md). Lua needs only a text editor - no compiler or Python installation.

Lua API **1.2.0** opens more of the game to modders: build character editors, Dungeon tools, quest utilities, terrain brushes and management mods. The new gameplay operations are experimental, and their world-changing actions currently run in singleplayer. Start with the [gameplay guide](docs/experimental/gameplay.md) and the [Gameplay API Explorer](examples/gameplay).

Give your mod a central Mods entry, native game windows, searchable lists and world dashboards. Use the [complete Lua app examples](docs/recipes/lua-apps.md), or the same [native windows and previews](docs/experimental/native-document-ui.md) from C++. The UI protocol remains experimental.

## For contributors

Framework **0.3.2** provides Lua API **1.2.0** and the experimental gameplay contract. Use the matching Runtime and SDK. The [gameplay guide](docs/experimental/gameplay.md) covers action results and lifetime rules; the [machine contract](api/experimental/gameplay.json) describes each operation and its result type.

All required native source dependencies are inside this repository. Build with Visual Studio 2022 C++, CMake and Python:

```powershell
python tools/build.py
python -m unittest discover -s tests -p "test_*.py"
python tools/check_contract.py
npm ci --ignore-scripts
npm run docs:build
```

Generated API reference, editor definitions and machine contracts share [one versioned source](api/contract.json). [CONTRIBUTING.md](CONTRIBUTING.md) explains the checks and generated-file boundaries.

## License

Original framework code, SDK, templates, documentation and branding are [MIT licensed](LICENSE). Modders choose the license for their own original work and retain notices for redistributed template code. Dependencies keep their [original licenses](THIRD_PARTY_NOTICES.md). Game imagery and names retain their respective rights; no game binaries or original archives are included. This is an independent community project, not an official VectorStorm product. The separate multiplayer project keeps its existing license.
