# API time policy

Requires MT2 Mod Framework 0.2.0 or later with experimental patch protocol 1. Use stable Lua API 1.0.0 together with the bundled experimental patch contract. Read `.mt2fw/api.json`, `.mt2fw/mt2.lua` and the SDK's `api/experimental/patches.json` before changing the project. Experimental patches are a separate versioned surface.

- `api.time.set` mediates Framework time requests. It does not intercept all native clock calls or the game's time-control UI.
- A modify Before callback may return partial typed replacements for `paused` and `speed`. One writer owns their combined slot. Do not call a world-write API from a patch callback.
- Register the policy once; request time changes only from the visible buttons after a world loads. Show the original error when authority is unknown or the process is a client. Never retry automatically.
- A shared manifest is metadata, not host authority. The adapter or trusted bridge must acknowledge the actual session. Reload does not undo clock changes.
- Run `mt2fw check <mod-directory>`, then `mt2fw pack <mod-directory> <zip-outside-source>`. A synthetic-host pass does not prove native game effects.
- Preserve the included MIT license for redistributed template code.
