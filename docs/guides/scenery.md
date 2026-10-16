---
title: Import static scenery
---

# Import static scenery

Bring an original static GLB into the native Scenery catalog. Start with the **Scenery Pack** template to learn the package layout, then use **Import GLB** for your own model.

![Two authored models, including a textured model, rendered by the game](/media/scenery.png)

## Preview and convert

Choose **Import GLB** in the helper. Inspect geometry, materials, scale, orientation, bounds and placement origin. Adjust scale and export a content package into your profile's `mods` folder. Restart the game before using new or changed content.

The importer supports static triangle geometry, node transformations, normals, UVs, vertex colors, base colors and embedded PNG/JPEG textures. It reports unsupported animation, skins, morphs, sparse accessors, compression or material extensions. Materials map to the game's diffuse model; metallic/roughness differences are reported.

## Generated files

A package contains `mod.json`, `content.json` and namespaced `assets`. The converter writes a native ModelV1 `.vmb`, material and scenery variant, translations and an editable `.vmd`. The native building catalog loads the binary model; renaming a GLB or VMD to VMB does not convert it.

Content IDs use the mod namespace, for example `yourname.scenery:pyramid`. Duplicate IDs and competing resource names are reported. Resources are registered additionally, without merging arbitrary game archives.

## Save and load

The framework records required content versions and hashes with confirmed saves. A missing required pack is named before loading, while the save remains unchanged. Keep the pack installed for worlds using its objects.

![An authored scenery building restored after a full game restart](/media/restored.png)

These real captures come from retained game acceptance. Original game artwork in the captures has separate rights. See [content.register](../reference/functions/content-register.md), [content schema](../reference/manifests.md) and the complete `examples/content` project.
