# Gameplay API example

Read the framework API contract and experimental gameplay contract before editing.
This example lists operations without changing the world. Keep queries in game-thread
callbacks and UI calls in render callbacks. Accepted actions need confirmed results;
never retry an unknown action. Do not introduce raw addresses or native Lua modules.
