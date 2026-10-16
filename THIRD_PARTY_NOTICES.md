# Third-party and provenance notices

| Component | Origin | License |
|---|---|---|
| Lua 5.4.9 | https://www.lua.org/ftp/lua-5.4.9.tar.gz | MIT; retain the Lua README notice from the verified source archive |
| MinHook | https://github.com/TsudaKageyu/minhook | BSD 2-Clause; retain the repository's vendor license |
| Dear ImGui | https://github.com/ocornut/imgui | MIT; retain LICENSE.txt |
| nlohmann/json | https://github.com/nlohmann/json | MIT; retain the vendor header notice |
| SDL3 import declarations and ImGui backend | https://github.com/libsdl-org/SDL | zlib; original game SDL3.dll is not redistributed |

The framework is authored by DooDesch. The retained native adapter, native ABI constants and proxy-forwarding technique originate in DooDesch's MMORPG Tycoon 2 Multiplayer work. These extracted authored copies are explicitly MIT licensed under the root LICENSE. `licenses/extracted-authored.json` records the copied input hashes and original paths. This permission does not relicense the original multiplayer product.

The standalone tree contains its own extracted logger and native declarations. DooDesch permits these authored copies under MIT as part of this framework. The internal retained mt2mp namespace is not a public SDK. No parent repository is needed to build. Third-party dependencies keep their own notices.

The public VectorStorm source is consulted to understand model and material contracts. Game-specific implementations, game archives, decompilation outputs and disassembly dumps are not distributed. Reading a file format does not grant rights to redistribute a game's assets.

An optional portable helper is built with PyInstaller. Its build retains the applicable PyInstaller distribution notices and bootloader exception. No helper package may silently omit those notices.

Lua is compiled as C++ with C linkage for its public API, so errors unwind host C++ values. The generated `ldo` wrapper adds framework instruction-budget status 7 and propagates it through nested protected calls to the host boundary. This prevents Lua `pcall` from swallowing the resource limit. The pinned upstream source remains unchanged; the build checks the exact insertion anchor. The Lua license is retained in the product package.

The portable helper also bundles Python, Tcl/Tk and Pillow. Their original license notices, plus PyInstaller and build dependency notices, are copied into `mt2fw/licenses` by the package builder. The helper software is separate from the game's original SDL3 library.

`licenses/dependencies.json` pins the hashes of every vendored native input. `package-lock.json` pins documentation tooling and integrity. `tools/license_inventory.py` verifies native hashes and retains installed documentation dependency notices in `licenses/DOCS_DEPENDENCY_NOTICES.md`. Generated web/offline packages include those notices.

Original framework documentation and branding are MIT licensed. Game screenshots retain separate rights, listed in `docs/media.json` and the media notice. The original game icon used as a private design reference is excluded. System fonts are referenced by name and are not redistributed.
