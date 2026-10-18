# Native create observer

Requires MT2 Mod Framework 0.2.0 or later with experimental patch protocol 1. Use stable Lua API 1.0.0 together with the bundled experimental patch contract. Read `.mt2fw/api.json`, `.mt2fw/mt2.lua` and the SDK's `api/experimental/patches.json` before changing the project. Experimental patches are a separate versioned surface.

- Register `objects.create` observations once with `mt2.patches.add`. Callback tables contain copied values and no raw addresses. Return no value from an observer.
- Keep counts in memory and draw only inside the registered window. This example counts native calls, not successful world actions. Only `original_returned` establishes normal return; it does not establish action success.
- Do not issue world writes from an observer. Do not load native Lua modules or add another hook.
- Reloading Lua replaces the complete owner registration set. A rejected replacement keeps the previous instance and order. Counts restart after a successful reload.
- Run `mt2fw check <mod-directory>`, then `mt2fw pack <mod-directory> <zip-outside-source>`. A synthetic-host pass does not prove a native game callback occurred.
- Preserve the included MIT license for redistributed template code.
