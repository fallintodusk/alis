# Fix City17 Interior Lighting Artifacts

Status: parked

## Current gap

City17 apartment interiors show two separate defects: static Lumen grain on
opaque surfaces and temporal shadow trails behind moving casters. They require
separate reproduction and acceptance controls.

## Investigation boundary

- Reproduce both effects in the current packaged route and capture fixed views.
- Classify geometry, material, direct-light, Lumen, VSM, and post-process
  contributions one variable at a time.
- Prefer content and light-authoring fixes before global console-variable
  changes.
- Preserve outdoor presentation and the fixed packaged performance budget.
- Add a durable pitfall only after one cause and regression are proven.

Do not mix the two symptoms into one tuning campaign.
