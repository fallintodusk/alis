# Fix City17 Landscape SM5 Sampler Overflow

Status: confirmed; implementation pending

## Current defect

Win64 Shipping cook compiles
`/ProjectObject/Nature/Landscape/Material/M_Landscape` for City17 with more
than the SM5 sampler limit. Unreal substitutes the default material for the
affected landscape component permutations. This predates the package-size
cleanup and is owned by the World/material boundary.

## Investigation boundary

1. Inspect the compiled `M_Landscape` sampler usage and the exact failing
   City17 landscape permutation.
2. Identify duplicate texture samples, non-shared samplers that can safely use
   `Shared:Wrap` or `Shared:Clamp`, and landscape-layer branches active for the
   affected components.
3. Make the smallest material-graph correction that preserves City17's
   accepted appearance.

Do not raise platform limits, suppress the shader failure, change RHI, or
redesign the full landscape material system.

## Acceptance

- `M_Landscape` compiles for `PCD3D_SM5` without sampler-register overflow.
- A fresh City17 Shipping cook contains no default-material fallback for the
  landscape hierarchy.
- A same-vantage City17 visual check preserves the intended landscape.
