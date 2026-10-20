---
title: Lua apps and world dashboards
---

# Lua apps and world dashboards

Lua API **1.1.0** adds a central launcher, native windows, document layouts and richer read-only world snapshots. Set `api_version` to `1.1.0` in your manifest so older runtimes reject the package with a version error. API 1.0 mods keep working. These additions require Framework 0.2.4 or newer.

## Start from a complete app

1. Copy `examples/native-app` from the source or SDK examples into your profile's `mods` folder.
2. Open **Mods...**, select **Native App**, then choose **Open Hello native Lua**.
3. Enter a name and select **Say hello**.
4. Change the greeting in `main.lua` and select **Reload** in Mods.

The [complete native app](../reference/functions/ui-native.md) uses the game's own blue window, text field and button. It needs only the `ui` capability. No extra hotkey, compiler or native plugin is needed.

Call `mt2.ui.app` from every `render` callback, including while closed. It returns `open_requested` only to this mod. Keep your open state and input values in Lua. Call `mt2.ui.native` when open, adopt `result.open`, and process `clicked` and `values` once per frame. `ready=false` means the game mode is not ready yet. Register these calls directly in `render`, outside `mt2.ui.window`.

Each mod owns one app. `desktop=true` adds an optional game-desktop icon; `icon="assets/icon.png"` supplies your own package PNG. The central Mods launcher remains the default entry point.

Native widgets support text, inputs, buttons, tabs, list rows, panels and PNG images. Positions, widget IDs, styles, sizes and input budgets stay fixed for one Lua instance. Reload Lua to change the layout. Native plugins still need a restart. A replacement that fails during preparation leaves the old Lua instance active; errors in a later render callback disable the replacement and retain their cause.

For a native image widget, set `path="assets/preview.png"`. Assets are confined to this mod's package. Images are limited to 1 MiB and 2048 pixels per dimension, with 128 unique previews per session. Loaded PNGs stay immutable for the session; restart after changing bytes at the same path. No general filesystem access is exposed to Lua.

## Lists and flexible layouts

`examples/document-app` provides a searchable list and detail panel using [mt2.ui.document](../reference/functions/ui-document.md). The document renderer handles columns, scrollable panels, selectable rows, inputs, buttons and balanced rendering scopes. Use stable IDs that are unique throughout a document so events stay unambiguous. Lua document images are not exposed; use a native image widget for packaged PNGs.

UI operations are experimental. Their fields and limits are described in the generated Lua types and the [native UI protocol](../experimental/native-document-ui.md). UI results report clicks, values and visibility, not confirmation of a world mutation.

## Read the world without scanning it every frame

`examples/world-dashboard` registers a native app and reads snapshots once a second while open:

| Function | Data |
| --- | --- |
| [`mt2.objects.building`](../reference/functions/objects-building.md) | Type, variant, position, quaternion rotation, scale and dungeon membership |
| [`mt2.world.region`](../reference/functions/world-region.md) | Ownership, native type and the current configured region purchase price at one position |
| [`mt2.world.simulation`](../reference/functions/world-simulation.md) | Cash, logged-in subscribers, clock and an optional current ledger with 20 categories |
| [`mt2.world.software`](../reference/functions/world-software.md) | Native rating and fun values |

Read these in a game-thread `update` or `world_ready` callback with the required `world.read` or `objects.read` capability. Check `mt2.world.status().loaded` first. Handles expire on world changes and deleted objects are rejected. Keep UIDs as strings; rotation is `[x,y,z,w]`, scale is `[x,y,z]`. Native type and variant names are identifiers, not localized labels. Ledger category IDs remain the game's numeric IDs 0-19; the API does not invent names or history.

These reads work without a Multiplayer controller. A local mod can show them, but it cannot change the simulation. Existing action requests keep their capability and Multiplayer authority guards; only a confirmed action result counts as success.
