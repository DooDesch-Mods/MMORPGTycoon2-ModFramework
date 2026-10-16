---
title: Create your first mod
---

# Create your first mod

Put a greeting in the game. Change it. Reload it.

Use the portable helper and **Hello MT2**. You need the supported Windows Steam game and a text editor. Lua needs no compiler. Use the local product package, or follow the [contributor build guide](contributing/build.md) to build it. This candidate has no public download.

## Install into your profile

Open `mt2fw/MT2FW-Helper.exe` in the extracted package. Select your Steam game and a mod profile, then choose **Install framework**. Keep the helper package outside the game directory so it remains available for removal. Unknown existing loaders are preserved and reported.

![The portable helper with installation and profile controls](/media/helper-install.png)

The capture shows the real helper. The profile gives your mods their own settings and game data. Existing multiplayer test profiles are not moved. Import a save only through the explicit copy action.

## Create Hello MT2

Select **Create a mod**, choose **Hello MT2**, and enter `yourname.hello`. Select your profile's `mods` folder as the parent. The helper creates the manifest, script, editor types and agent guide.

![The helper's mod creation controls](/media/helper-create.png)

## Open the game

Start through Steam. The **Hello MT2** window appears. Load a world to see its name. F10 opens **Mods**.

![The first Lua mod visible in the actual game](/media/first-mod.png)

## Make it yours

Open the generated `main.lua` in your text editor. Replace its first line with:

```lua
local message = "My first MT2 mod!"
```

Save the file. Keep the rest of the generated script.

## Reload and see the change

In **Mods**, choose **Reload** beside your mod. The new greeting appears without restarting the game.

![A changed greeting after reloading in the same running game](/media/edited-mod.png)

If replacement code has a syntax or activation error, the previous instance stays active. Read the displayed file, line and original cause, fix the script, then reload again. Other mods keep running. Content packages and native DLLs need restart.

## Check and share

Choose **Check mod**, then **Pack mod ZIP**. Save the ZIP outside the mod source folder. Another player installs it with **Install mod ZIP**.

```powershell
mt2fw check C:/Mods/yourname.hello
mt2fw pack C:/Mods/yourname.hello C:/Mods/hello-1.0.0.zip
```

Include your own source, assets and license. Generated template code is MIT licensed; retain its notice. Choose a license for your original additions. Continue with [settings and storage](guides/config-storage.md), [API reference](reference/index.md) or [agent instructions](agents/start.md).

🛟 **Need help or found a bug?** Get support at [support.doodesch.de/modframework](https://support.doodesch.de/modframework).
