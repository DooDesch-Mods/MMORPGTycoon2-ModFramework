---
title: Native app entry and New Game handoff
---

# Native app entry and New Game handoff

Requires Framework Runtime 0.1.6 or newer. Lua API 1.0.0 and native C ABI 1 remain unchanged.

These MT2-only JSON operations are experimental. They use the existing native host request bridge, require the `ui` capability and belong in a render callback on the bound game thread. The stable Lua API and native plugin C ABI are unchanged.

## Optional Desktop entry

Register the same app before drawing its native window:

```json
{
  "id": "main",
  "title": "Multiplayer",
  "desktop": true,
  "icon": "assets/multiplayer.png"
}
```

Send this object to `ui.app`. The host supplies the caller's mod identity. `desktop` defaults to false, so existing registrations keep their central Mods launcher behavior. One app is registered per mod. The session supports at most 64 mod Desktop entries.

`icon` is optional. It names a PNG under the caller's own package `assets/` directory, or uses the existing data-directory `cache/<sha256>.png` contract. Cache file contents must match the filename hash. Both forms fit within 1 MiB and have dimensions no larger than 2048 x 2048. The Framework reads and caches the image once, without file operations per frame. Changing image contents needs a restart. Without an icon, the entry uses the Framework's Mods artwork.

The existing native Desktop owns the icon and its dragging. A double-click queues only that app's `open_requested` flag. Repeated input before the next callback coalesces into one request. `ui.app` consumes that flag once, just as the central Mods launcher does. A mod must not treat registration as an instruction to open its window every frame.

The response adds a `desktop` object:

| Field | Meaning |
| --- | --- |
| `requested` | The caller requested Desktop exposure. |
| `registered` | An icon for this app is confirmed on the current native Desktop. |
| `visible` | Observed visibility, or null when it is unknown. |
| `visibility_known` | Whether `visible` has a confirmed value. |
| `error` | Original registration or presentation failure, otherwise an empty string. |

Registration may complete on the next Framework frame. Entries are shown only on the Title Desktop. The in-game `Mods...` menu and the central launcher remain available. An inactive owner loses pending open requests and its icon is hidden; its native icon storage remains owned by the Desktop until native destruction. A failed icon does not disable another app or the central Mods window. Native icon creation with an unknown result is not automatically repeated.

## Open the original New Game wizard

After registering the app, send an empty object to `ui.new_world` in response to the user's New World action. This opens the original MT2 wizard. It does not create a world or select any settings.

Only `confirmed: true` with `status` of `opened` or `already_open` permits the caller to hide or suspend its own app window:

```json
{
  "status": "already_open",
  "confirmed": true,
  "attempted": false,
  "visible": true,
  "mode": 2,
  "window": "NewGame",
  "route": "mmoModeTitle::ShowCreateWindow"
}
```

The successful states are `already_open` and `opened`. An already visible wizard is acknowledged without calling the launcher, preserving its edited name, seed and page. A hidden wizard uses the original launcher once, then verifies the same owner, registry entry, class and visible state.

Failure returns `confirmed: false`, `status` of `rejected` or `unknown`, an `attempted` flag and the original `cause`. A rejected request made no native launch call. An unknown result must not trigger another launch: a later observation can acknowledge the now-visible wizard, otherwise the operation remains blocked for that Title epoch. A native mode change retires the old attempt.

The caller retains its lobby state. If the player cancels the native wizard, it can reopen its registered app. If the player uses the original Create control, Multiplayer's existing world-readiness and Host-adoption path remains responsible for shared preparation. This API does not acknowledge that later world creation or multiplayer admission.

To detect cancellation, send `{"observe":true}` to the same operation. This performs only the ownership and visibility reads, never the launcher. Its success receipt has `status: "observed"`, `confirmed: true`, `attempted: false` and the actual `visible` value. A cancelled hidden wizard stays hidden. `confirmed` here confirms only that observation. After Title is left, this read mode rejects with `attempted: false`; the caller's existing world-readiness path handles the new world.
