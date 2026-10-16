# Runtime third-party notices

Copyright (c) 2026 DooDesch. Original framework code is MIT licensed; see `LICENSE`.

| Component | Origin | License notice |
| --- | --- | --- |
| Lua 5.4.9 | https://www.lua.org/ftp/lua-5.4.9.tar.gz | [MIT](licenses/lua-README.txt) |
| Dear ImGui | https://github.com/ocornut/imgui | [MIT](licenses/imgui-LICENSE.txt) |
| MinHook | https://github.com/TsudaKageyu/minhook | [BSD 2-Clause](licenses/minhook-LICENSE.txt) |
| nlohmann/json | https://github.com/nlohmann/json | [MIT](licenses/nlohmann-LICENSE.MIT) |
| SDL3 import declarations and ImGui backend | https://github.com/libsdl-org/SDL | [zlib](licenses/SDL3-LICENSE.txt) |

The game provides its own SDL3 library; it is not redistributed. Lua's build-time C++ wrapper propagates framework instruction-budget failures through nested protected calls. The original Lua license is retained above.

DooDesch licenses the extracted authored adapter, logger, ABI constants and proxy-forwarding code under MIT for this framework. This does not relicense the separate multiplayer project. Source provenance and the complete dependency inventory are available in the repository and SDK. Game binaries, game archives and private reference material are excluded.

This runtime contains no Python, Tcl/Tk, Pillow, PyInstaller or documentation tooling. Those components and their applicable notices belong to the separate developer SDK.
