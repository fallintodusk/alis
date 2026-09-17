# Generate Skeletal Mesh Thumbnails

Status: uninvestigated

## Current gap

Single-frame custom skeletal-mesh thumbnail rendering can produce black output
or GPU hangs because skeletal render state initializes asynchronously.

## Investigation boundary

- Confirm the defect on the current engine and generated definition assets.
- Prefer package-cached thumbnails created by the definition generator over a
  parallel realtime renderer.
- Preserve existing StaticMesh and Texture thumbnail behavior.
- Prove generation, Content Browser display, regeneration, and headless command
  behavior without D3D timeout warnings.
