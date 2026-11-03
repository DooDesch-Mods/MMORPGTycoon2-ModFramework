# Nexus releases

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/modframework](https://support.doodesch.de/modframework).

The **Publish Nexus files** workflow updates the existing Nexus files using the official [Nexus upload action](https://github.com/Nexus-Mods/upload-action). Framework and Vortex releases run when their GitHub release is published, including a prerelease. Starting with Multiplayer Beta 29, the successful Multiplayer release job calls this same workflow directly. Scheduled polling is retained only for legacy Beta 27/28 recovery; new releases do not depend on it.

| Product | Nexus page | Unique Mod ID for the API | Persistent File ID |
| --- | --- | --- | --- |
| Framework | [MMORPG Tycoon 2 / 25](https://www.nexusmods.com/mmorpgtycoon2/mods/25) | `19121194401817` | `8170305` |
| Vortex extension | [Modding Tools / 2465](https://www.nexusmods.com/site/mods/2465) | `9856949946785` | `8170257` |
| Multiplayer | [MMORPG Tycoon 2 / 28](https://www.nexusmods.com/mmorpgtycoon2/mods/28) | `19121194401820` | `8177136` |

`targets.json` records these destinations. The API uses the Unique Mod ID, not the page ID. File IDs identify the existing update chains and stay fixed when a new version is uploaded. Short Unique File IDs from the website are not action inputs.

## GitHub configuration

The organization secrets `NEXUSMODS_API_KEY` and `DISCORD_RELEASE_WEBHOOK` are available to this repository. The repository variables are:

- `NEXUS_FRAMEWORK_MOD_ID=19121194401817`
- `NEXUS_FRAMEWORK_FILE_ID=8170305`
- `NEXUS_VORTEX_MOD_ID=9856949946785`
- `NEXUS_VORTEX_FILE_ID=8170257`
- `NEXUS_MULTIPLAYER_MOD_ID=19121194401820`
- `NEXUS_MULTIPLAYER_FILE_ID=8177136`

Multiplayer remains excluded from `all`, so Framework release events cannot select it accidentally. The direct caller must be the Multiplayer repository, use a main-branch push, matching tag push or explicit dispatch, select only Multiplayer, and pin the reusable workflow and tooling checkout to the same full Framework commit. It passes only the two existing organization secrets and grants `contents: write` for its own release receipts. No cross-repository dispatch token is required. The tracked Nexus IDs supply defaults when caller repository variables are absent; conflicting variables still fail validation.

Every Multiplayer path validates the exact native plugin ZIP, per-ZIP checksum, tagged source manifest and published GitHub release notes before Nexus upload and Discord delivery. The file description refers to that release's Runtime requirement instead of pinning an older Runtime. Prepared listing updates remain an explicit operation with exact cohort guards; future betas never apply Beta 27's listing metadata.

Beta 28 and earlier keep their Discord intent and acknowledgement assets on the configured Framework receipt release (`v0.2.0`). Beta 29 and later store them on the exact Multiplayer release being delivered. The central Framework workflow refuses delivery of those newer versions, and its legacy poll excludes them. This keeps one delivery owner per version. A confirmed receipt retains product, tag, version, source commit and package digest; an uncertain or different receipt fails for inspection instead of replaying a send. The announcement always links to the Multiplayer release.

The workflow rejects variables that differ from the tracked destinations. Keys are only passed to API/action steps, never stored in source or receipts.

## Release flow

1. Build and verify the product, then publish the accepted ZIPs in a GitHub release with `SHA256SUMS.txt`.
2. The workflow selects framework/Vortex archives that are present and verifies their checksums, ZIP structure and embedded versions independently.
3. It checks each Nexus page, verifies that its file ID belongs to that mod, and reads existing versions. An existing version is skipped; an older candidate is rejected. Skipping does not claim remote bytes were compared.
4. New versions upload through the pinned official action. Their file description contains the matching product/version section of `CHANGELOG.md` rendered as BBCode. Missing or duplicate sections fail before upload. The prior version is archived, the new one becomes the main download, and the mod version is updated. The listing description, summary and gallery are not changed by this workflow.
5. The workflow reads back the created version, completes its plain-text Nexus changelog and sends a Discord release embed with changes, artwork and download links. Each destination retains an acknowledgement.

Framework and Vortex versions come from their archives, not from the shared release tag. A release can contain either or both. Missing targets are skipped only when `all` is selected; an explicitly selected missing archive fails.

Starting with 0.1.1, Nexus accepts only the allowlisted player runtime. The separate `MT2Framework-SDK` archive is a GitHub-only download. The release selector excludes SDK filenames, and preflight checks the runtime package kind, exact file inventory and all hashes before upload. The runtime includes the native mod checker needed by the existing Vortex extension; Python and authoring tools remain in the SDK.

For a manual check, open **Actions > Publish Nexus files > Run workflow**, enter the existing release tag, select a target, and leave **publish** off. This validates artifacts and authenticated Nexus reads without writes. Turn **publish** on to upload a newer version or complete a missing changelog for an existing version. Enable **announce** to send a missing Discord announcement. Published-release events enable both automatically.

That central manual entry remains for Framework, Vortex and legacy Multiplayer releases. For Multiplayer Beta 29 and later, rerun the matching Multiplayer delivery job. Its caller passes the published tag through the release job output, uses `needs: release`, pins both `uses` and `tooling_revision` to the accepted Framework commit, and sets `publish` and `announce` explicitly. An unsuccessful delivery fails that job; it is not converted into release success. A GitHub release may already exist when Nexus or Discord fails, so inspect its retained delivery state before recovery.

Runs for the same product are serialized. After a timeout or failed acknowledgement, inspect the retained plan and Nexus versions before retrying. A successful version read prevents the same version from being uploaded again; upload failures are not silently retried.

Nexus changelogs use an additive API. The workflow reads the existing version's entries through the official v1 API before writing through v3, then reads them back. Matching text is skipped; different text fails instead of appending duplicates. This runs separately from the upload action so an interrupted run can complete metadata without re-uploading its ZIP. Existing file descriptions cannot be edited through the public v3 API; use Nexus file management for that repair.

Discord delivery uses `wait=true` and records the acknowledged message ID in a small `release-delivery-<product>-<version>.json` asset on the existing GitHub release. A send intent is saved there before the POST, and the returned message ID is saved before content validation. A rerun reads back a confirmed message instead of posting again. An unconfirmed intent stops the run: inspect the release channel and provide its existing message ID in **discord_message_id** to reconcile it without sending. Receipts contain no webhook URL or API key. Only the delivery job has permission to write release assets.

The release embed follows the Schedule I format: a small product icon, bold change headings, short bullet points, download links and a game/repository footer. It is generated automatically from the same product/version changelog used for Nexus. Write that changelog once in English, using headings such as `Added`, `Fixed` and `Changed`. Describe what players can do and what improves in the game. Keep API/ABI details in developer documentation and build logs in acceptance evidence. There is no separate Discord announcement file or additional authoring step.

Multiplayer `0.1.0-beta.N` announcements use the title `MMORPG Tycoon 2 Multiplayer - Early Beta N`. The download and release URL retain the exact SemVer version. To correct an existing announcement, provide its **discord_message_id** and enable **update_announcement**. The workflow verifies its exact current or legacy product title and exact release URL before editing it, then reads back the same message. It never sends a replacement post during this operation.

If the tagged changelog itself needs a wording correction, first edit that version's existing Nexus changelog in Nexus file management. Then supply the corrected Markdown in **corrected_notes** with the existing message ID and **update_announcement** enabled. This input is accepted only for a version already present on Nexus. The workflow requires its Nexus changelog to match before editing Discord. Package identity and upload checks stay unchanged; no new file or post is created.

For an already delivered Multiplayer version whose GitHub notes changed afterward, **use_delivered_changelog** with **publish** and **announce** verifies its actual existing Nexus changelog without replacing or appending text. Discord uses the current published GitHub notes from the same verified package, tag and source commit. A retained `changelog-drift.json` records the existing Nexus text, both hashes and origins, and whether they match. It does not claim that Nexus contains the corrected GitHub notes. This explicit recovery requires an existing Multiplayer Nexus version and preserves the normal Discord intent, acknowledgement and duplicate guards. It cannot be combined with corrected notes or an announcement edit. The default changelog mismatch guard remains unchanged.
