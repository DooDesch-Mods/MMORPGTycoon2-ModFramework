# Nexus releases

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/modframework](https://support.doodesch.de/modframework).

The **Release** workflow (`.github/workflows/release.yml`) builds a tagged version, creates its GitHub release and uploads the ZIP to Nexus through the official [Nexus upload action](https://github.com/Nexus-Mods/upload-action).

| Tag | Product | Nexus page | Unique Mod ID | File ID |
| --- | --- | --- | --- | --- |
| `v<version>` | Framework runtime | [MMORPG Tycoon 2 / 25](https://www.nexusmods.com/mmorpgtycoon2/mods/25) | `19121194401817` | `8170305` |
| `vortex-v<version>` | Vortex extension | [Modding Tools / 2465](https://www.nexusmods.com/site/mods/2465) | `9856949946785` | `8170257` |

`targets.json` records these destinations. The API uses the Unique Mod ID, not the page ID. The file ID names the existing update chain; a new version is added to it. The workflow needs the organization secret `NEXUSMODS_API_KEY`.

## Release a version

1. Set the version (`framework_version` in `api/contract.json`, or `version` in `integrations/vortex/extension/info.json`) and add its section to `CHANGELOG.md`.
2. Merge to `main`, then push the tag: `git tag v0.5.9 && git push origin v0.5.9`.

The workflow then:

1. Runs the full **Build and verify framework** checks and packages the downloads.
2. Checks that the ZIP's embedded version equals the tag, its checksum, inventory and file hashes, and that the Nexus page and file ID belong together. A version already on Nexus is skipped; an older one fails.
3. Creates the GitHub release with the changelog section as notes. Framework releases carry the runtime ZIP, the SDK ZIP and `SHA256SUMS.txt`; Vortex releases carry the extension ZIP and `SHA256SUMS.txt` and are marked as prerelease.
4. Uploads the runtime or extension ZIP to Nexus as the new main file, archives the previous version, reads the upload back and writes the same changelog section as the Nexus version changelog.

Only the player runtime goes to Nexus; the SDK stays on GitHub. A rerun reuses an existing GitHub release and its assets, and a version already on Nexus is not uploaded again. Nexus changelogs are additive: matching text is skipped, different text fails instead of adding a second entry.

## Manual runs

Open **Actions > Release > Run workflow** and enter an existing release tag.

- With **publish** off, the run validates the release assets and the Nexus destination and writes nothing.
- With **publish** on, it uploads a newer version or completes a missing changelog.
- With **publish** and **update_listing** on, it also replaces the Nexus summary and description with `listings/<product>/summary.txt` and `description.bbcode.txt`. `listings/<product>/release.json` must name that exact tag, version and SHA-256, and the run fails if Nexus does not read back the new text or changed any other field.

File descriptions of existing versions cannot be edited through the public API; use Nexus file management for that.
