---
title: Profile a mod
---

# Profile a mod

[MT2 Profiler](https://github.com/DooDesch-Mods/MMORPGTycoon2-Profiler) is an optional native mod. Install it separately from the Framework runtime. It opens from the central **Mods...** window and records locally.

1. Install MT2 Profiler and restart the game.
2. Open **Mods...**, select **MT2 Profiler**, then choose **Open**.
3. Choose **Start capture**, reproduce the scene, then choose **Stop capture**.
4. Choose **Export capture** and wait for the confirmed file path.

Captures are stored under `data/doodesch.profiler/captures` in the active Framework profile. The optional Python report tool produces Markdown and CSV output for comparing runs. See the [Profiler guide](https://github.com/DooDesch-Mods/MMORPGTycoon2-Profiler/blob/main/README.md) and [measurement reference](https://github.com/DooDesch-Mods/MMORPGTycoon2-Profiler/blob/main/docs/measurements.md).

## Interpret a result

Use the same world, game speed and workload in each comparison. Native recordings measure wall time in instrumented sections, rather than stack samples or rendered FPS. A mod must provide instrumentation for its internal sections to appear. The raw event prefix is bounded; check the reported drops and incomplete frames before using a report.

The Profiler uses the experimental [MT2 native services](../experimental/native-services.md). Those services are versioned separately from the stable native plugin ABI. Installing the Profiler does not establish Multiplayer compatibility.
