# Terrain Reader example

Requires Lua API 1.3.0. Keep native queries in game-thread callbacks and cache
their results for render. Use Region handles from the current world. Grid columns
and rows are zero-based. Samples are raw native height-grid values, not an
interpolated surface or collision test. Keep the example read-only, bounded and
free of per-frame world scans.
