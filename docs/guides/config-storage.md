---
title: Settings and storage
---

# Settings and storage

Define settings once. The Mods menu shows the schema and saves each mod's values inside its profile.

```lua
mt2.config.define("greeting", {
    type = "string", default = "Hello, MT2!", label = "Greeting"
})
mt2.ui.window("My greeting", function()
    mt2.ui.text(mt2.config.get("greeting"))
end)
```

Declare `ui` for this window. Boolean, integer, number and string settings are supported. Numeric schemas can define `min` and `max`; setting a wrong type or an out-of-range value reports an error.

## Keep profile data

`mt2.storage.get(key, default)` and `set(key, value)` use the current mod's profile data. Use JSON-compatible values. Each scope has a 1 MiB budget. Arbitrary filesystem APIs, processes and native Lua DLLs are outside the regular Lua contract.

```lua
mt2.events.on("start", function()
    local starts = mt2.storage.get("starts", 0)
    mt2.storage.set("starts", starts + 1)
end)
```

## Keep world data

Use the `"world"` scope only while a world is loaded. Data is associated with the exact confirmed game save. A `saved` event means the original save and its framework metadata have been confirmed.

```lua
mt2.events.on("world_ready", function()
    local visits = mt2.storage.get("visits", 0, "world")
    mt2.storage.set("visits", visits + 1, "world")
end)
```

For root-world buildings, store `mt2.objects.persist(handle)` in world data, then use `mt2.objects.resolve(id)` after loading. The framework validates saved identity before returning a new handle. A deleted or mismatched building is an error, not a reason to create a replacement automatically.

See [config](../reference/functions/config-define.md), [storage](../reference/functions/storage-get.md), [types](../reference/types.md) and complete projects under `examples/config` and `examples/storage`.
