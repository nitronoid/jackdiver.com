// Sets a global, then throws while it loads.
globalThis.loaded = true;
throw new Error("failed to load");
