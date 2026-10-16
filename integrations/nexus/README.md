# Nexus releases

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/modframework](https://support.doodesch.de/modframework).

The **Publish Nexus files** workflow uploads versions of the two existing Nexus files using the official [Nexus upload action](https://github.com/Nexus-Mods/upload-action). It runs when a GitHub release is published, including a prerelease, and supports a manual validation run.

| Product | Nexus page | Unique Mod ID for the API | Persistent File ID |
| --- | --- | --- | --- |
| Framework | [MMORPG Tycoon 2 / 25](https://www.nexusmods.com/mmorpgtycoon2/mods/25) | `19121194401817` | `8170305` |
| Vortex extension | [Modding Tools / 2465](https://www.nexusmods.com/site/mods/2465) | `9856949946785` | `8170257` |

`targets.json` records these destinations. The API uses the Unique Mod ID, not the page ID. File IDs identify the existing update chains and stay fixed when a new version is uploaded. Short Unique File IDs from the website are not action inputs.

## GitHub configuration

The organization secret `NEXUSMODS_API_KEY` is available to this repository. The repository variables are:

- `NEXUS_FRAMEWORK_MOD_ID=19121194401817`
- `NEXUS_FRAMEWORK_FILE_ID=8170305`
- `NEXUS_VORTEX_MOD_ID=9856949946785`
- `NEXUS_VORTEX_FILE_ID=8170257`

The workflow rejects variables that differ from the tracked destinations. Keys are only passed to API/action steps, never stored in source or receipts.

## Release flow

1. Build and verify the product, then publish the accepted ZIPs in a GitHub release with `SHA256SUMS.txt`.
2. The workflow selects framework/Vortex archives that are present and verifies their checksums, ZIP structure and embedded versions independently.
3. It checks each Nexus page, verifies that its file ID belongs to that mod, and reads existing versions. An existing version is skipped; an older candidate is rejected. Skipping does not claim remote bytes were compared.
4. New versions upload through the pinned official action. The prior version is archived, the new one becomes the main download, and the mod version is updated. The listing description, summary and gallery are not changed by this workflow.
5. The workflow reads back the created version and retains a receipt with the source release, checksum, target and acknowledged version ID.

Framework and Vortex versions come from their archives, not from the shared release tag. A release can contain either or both. Missing targets are skipped only when `all` is selected; an explicitly selected missing archive fails.

Starting with 0.1.1, Nexus accepts only the allowlisted player runtime. The separate `MT2Framework-SDK` archive is a GitHub-only download. The release selector excludes SDK filenames, and preflight checks the runtime package kind, exact file inventory and all hashes before upload. The runtime includes the native mod checker needed by the existing Vortex extension; Python and authoring tools remain in the SDK.

For a manual check, open **Actions > Publish Nexus files > Run workflow**, enter the existing release tag, select a target, and leave **publish** off. This validates artifacts and authenticated Nexus reads without an upload. Turn **publish** on to upload a newer version from that release.

Runs for the same product are serialized. After a timeout or failed acknowledgement, inspect the retained plan and Nexus versions before retrying. A successful version read prevents the same version from being uploaded again; upload failures are not silently retried.
