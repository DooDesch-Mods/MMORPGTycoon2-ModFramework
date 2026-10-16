# MT2 mod authoring

Use the bundled `.mt2fw/mt2.lua` types and `.mt2fw/api.json` contract for API 1.0.0. Create Lua mods with a text editor; no compiler is required. Read the installed documentation's Agents section for complete examples.

- Declare every required capability in `mod.json`. Use a namespaced lowercase ID. Local mods cannot change world state.
- Register UI once with `mt2.ui.window`; draw widgets only inside its callback. Call game APIs after activation, usually in `world_ready`, `update` or a user-triggered callback. The `start` event still belongs to preparation.
- Keep handles intact. Their string UID is exact; their epoch expires on world change. Use persisted references and world storage for confirmed saves.
- An accepted action is queued, not successful. Wait for confirmed. Preserve and investigate unknown outcomes; never repeat them automatically.
- Lua code can reload. Content registrations and native plugins need restart. Do not use raw addresses, process launches or native Lua modules.
- Run `mt2fw check <mod-directory>`, then `mt2fw pack <mod-directory> <zip-outside-source>`. Report source checks separately from real game acceptance.
- This framework template is MIT licensed. Preserve the included notice for redistributed template code; choose a license for your own original additions.
