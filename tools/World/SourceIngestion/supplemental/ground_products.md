# Qualified derived Ground products

This operation admits a frozen, separately qualified processed product as
supplementary model evidence. It does not decode raw point clouds, recreate an
earlier source replay, select geographic authority, or accept terrain quality.
The [descriptor](contracts/derived-ground.schema.json) owns its closed shape and
explicit basis. Observed LAS Ground retains its strict class-2 contract. A provider
bare-earth raster is a different model: its grid samples are not point returns
and receive no LAS classification. The same admission and retention operation
handles both; neither becomes geographic authority through admission.

The caller must supply a qualified extraction and frame-processing capsule.
That capsule includes the exact raw object, source and transformed Ground
points, or the qualified raster header and processed grid, extraction and
transform recipes, processing receipts, runtime evidence,
qualification evidence and required transform grids. Provider identity, rights,
source frame, target frame, query selection and axis-specific accuracy remain
explicit. Unknown accuracy stays null. Source statistics and transformation
roundtrip precision do not become local hard error bounds.

The qualification artifact is supplied evidence, not a trusted verdict parser.
Its presence or hash cannot establish correct processing. The selected frozen
product must first have actual raw/query/frame qualification; the operation
then binds those exact artifacts and rejects changed bytes, classification,
count, frame, nonfinite coordinates, conflicting XY heights and escaped paths.
The observed branch validates source-query coverage against the effective
selection envelope and retains classified Ground only. Requested and effective query rectangles are
distinct: an integer-lattice COPC selector can include points just beyond the
requested decimal rectangle. That selection rule is processing provenance,
not a geographic accuracy allowance. The declared decimal serialization is
applied to the effective bounds when checking serialized source coordinates;
raw LAS coordinates and rounded GeoJSON values are distinct representations.
It does
not infer a measured waterline, basin depth, operating state or unchanged
present-day geography from those observations.
An explicit GeoJSON CRS must agree with the descriptor; an unknown explicit
encoding refuses rather than silently overriding it. If a file has no CRS
member, its qualified processing capsule supplies the frame evidence.

The raster branch binds the externally qualified native header, provider surface
kind, source units, pixel interpretation, NoData and resampling to a finite
canonical grid. The [grid contract](contracts/ground-grid.schema.json) owns its
shape. Header metadata, sizes, node bounds, spacing and frame must agree with
the descriptor; unknown samples refuse. Explicit CRS component identities come
from the native structured header, not nested datum or projection identifiers.
Native source decoding and the
header-to-object/transform proof belong to the preceding independent processing
qualification. Admission does not repeat that decode or trust a supplied review
verdict as proof. A raster's missing explicit vertical header remains qualified
by the retained provider/frame evidence, rather than an invented datum.

Raster qualification must bind the complete sampling recipe, including native
filter scale, overview selection, transform approximation, masks and units.
A method name alone does not define its consumed support: GDAL's default
bilinear downsampling can widen its kernel and change its scale per processing
chunk. Explicit point-bilinear sampling and scale-aware low-pass downsampling
are different derived products. Their proofs must cover the selected kernel,
shared-window reproducibility and actual adjoining-source consumption. Four
valid source centres certify point-bilinear support; they do not certify an
automatically widened filter. A finite output produced by NoData weight
renormalization does not establish complete source support.

The raster model can supply broader terrain context. Its evidence cannot satisfy
the classified-observation contract of the contained-Water constructor. Raw
observations and a provider's TIN, breakline and raster model remain separately
identified even when their heights are close.

Before a candidate receipt is published, every capsule artifact and the input
descriptor are verified in both stores through the existing
[snapshot retention](../retention/) contract. The candidate contains its own
verified capsule, with relative paths, and is confined to project `tmp`.
Publication is a final directory rename under an exclusive output lock. A
failed replica prevents the receipt; completed verified archive copies remain
reusable. An existing candidate is never overwritten. A matching repeat
revalidates the selected inputs, candidate and both retained copies.
The input descriptor is parsed and hashed from one byte snapshot, retained as
that exact snapshot, and carried alongside its relocated capsule descriptor.
The original input and consumed implementation are rechecked before promotion.

Consumers pin the exact admission receipt and use the public reader. It verifies
the admission implementation, descriptor and all capsule artifacts, then
returns basis-specific content identity and source/frame provenance. Only the
observed basis returns classified points; the raster basis returns its grid.
This is immutable
derived-product consumption, not fresh raw extraction or production promotion.
Quality and final geographic selection remain with Canonical Compilation.

The current descriptor and receipt contracts require an explicit basis and
basis-specific content hash. A changed admission implementation requires fresh
admission of the retained frozen product. That verification does not reacquire
raw sources, repeat the upstream extraction, or rewrite a historical receipt.

The optional child has its own consumed implementation identity. Base
SourceIngestion does not consume this child and does not change identity merely
because this capability changes. A real raw-source, base-adapter or consumed
base-contract change still requires its normal source replay.
