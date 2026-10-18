# Native patch observer

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/modframework](https://support.doodesch.de/modframework).

Observe one native object create through experimental patch protocol 1. Requires MT2 Mod Framework 0.2.0 or later, Windows x64, CMake 3.24 or later and Visual Studio 2022 C++ tools. Stable Lua API 1.0.0 and native plugin ABI 1 are unchanged.

The project includes its matching public headers. It resolves `MT2FW_MT2_QueryPatches` on the loaded framework from an active update callback and passes the exact borrowed Host pointer from Load. The typed create callbacks keep only an atomic count. No game addresses or additional hook library are needed.

1. Copy this project to `C:/Mods/native-patches`.
2. Run `cmake -S C:/Mods/native-patches -B C:/Mods/native-patches/build -A x64`.
3. Run `cmake --build C:/Mods/native-patches/build --config Release`.
4. Run `mt2fw check C:/Mods/native-patches`.
5. Run `mt2fw pack C:/Mods/native-patches C:/Mods/native-patches.zip`.

Install the ZIP with the portable helper, restart the game and load a world. Native plugins require restart. On the first completed create, a later update requests retirement. Windows debugger output contains registration and retirement results with original receipt causes. `BUSY` retains callbacks and state until a later `OK`; other failures stop automatic retries and retain state. Native callback memory remains valid for the process lifetime in this example.

This read-only local mod declares `world.read`. Two observers can coexist through the shared Lua/native patch order. It does not change world state, establish multiplayer authority or confirm a world action.
