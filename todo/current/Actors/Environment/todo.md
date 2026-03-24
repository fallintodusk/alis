### Assets & Generation
- [ ] Downspouts: current merged mesh approach is heavy on memory. With 10 pipe types = 10 large meshes. Move to data asset driven approach - define in parent JSON, change once, update everywhere.
- [ ] Spline_BP2, Spline_BP, Spline_BP3 - migrate to parent JSON as part of environment setup.
- [ ] WoodenPalette_BP2 (store setting) - migrate to JSON for randomization support.
- [ ] Add spline fence near Khrushchev building.

### Placement & Layout
- [ ] Store trash on floor has shifted under the stage - reposition. Mouse selection broken for this actor.
- [ ] Luxury apartment: sofa overlaps closet - fix furniture placement.

### Structural Improvements
- [ ] Shop: left wall of stand and floor have z-fighting, front wall is transparent - fix materials/geometry.
- [ ] School: improve exterior look.
- [ ] Garage: improve look.

### Bug Fixes
- [ ] Missing collision on fence near vehicle area.

### Future
- [ ] Add environment objects: MetalFence, Border, Surface, TrashBin, Playground.
