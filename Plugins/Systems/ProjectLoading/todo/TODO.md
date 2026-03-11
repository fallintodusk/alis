# ProjectLoading TODO

## Runtime loading hygiene

- [ ] Per-handle delegates: keep plugin-specific callbacks on the returned handle to avoid global multicast cross-talk
- [ ] ActivateFeatures: ensure phase remains module-free (modules are preloaded at boot). Add a small unit test covering expected behavior
