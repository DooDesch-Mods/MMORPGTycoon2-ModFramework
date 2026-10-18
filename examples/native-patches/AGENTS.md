# Native patch observer authoring

Requires MT2 Mod Framework 0.2.0 or later with experimental patch protocol 1 and native plugin ABI 1. Use the matching headers bundled under `.mt2fw`; the SDK's `api/experimental/patches.json` describes targets and ordering. The guide includes the actual `plugin.cpp` source from this project.

- Preserve manifest identity, entry DLL and exported metadata together. Declare `world.read` for observations.
- Load is preparation. Query the extension from an active Framework callback on the bound game thread with the exact borrowed Host pointer from Load.
- Read copied scalar fields only during the callback. Never retain the borrowed event pointer. Create callbacks can run on worker threads; keep shared state thread safe.
- No exception or STL object may cross the C ABI. Inspect every query, registration and retirement result and preserve its receipt cause.
- `BUSY` prevents future selection but retains current callback leases. Keep callback code and user data until confirmed `OK`. Failed retirement stops automatic retries and also preserves state. The example's state lasts for the process lifetime.
- Native plugins require restart. Source compilation and package validation do not prove native game callbacks or multiplayer compatibility.
- Preserve the included MIT license for redistributed template code.
