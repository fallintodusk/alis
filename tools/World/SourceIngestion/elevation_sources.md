# Elevation source suitability

Owns provider-product interpretation and the source checks needed before choosing
elevation evidence. The [Ground admission contract](supplemental/ground_products.md)
owns processing qualification; [Canonical Compilation](../CanonicalCompilation/README.md)
owns geographic selection. This guide does not admit a replacement Terrain model.

## Contents

- [Establish the physical surface](#establish-the-physical-surface-before-repairing-geometry)
- [Provider products and limits](#provider-products-and-limits)
- [Professional treatments and the ALIS boundary](#professional-treatments-and-the-alis-boundary)
- [Engineered shoreline evidence](#engineered-shoreline-evidence)
- [Existing capabilities and comparison exit](#existing-capabilities-and-comparison-exit)

## Establish the physical surface before repairing geometry

Before tuning heights or transitions, identify the physical surface required by
the consumer and the surface actually represented by the source. Record:

- product variant, acquisition epoch, distribution and exact object identity;
- DSM, modeled bare earth, classified Ground returns, bathymetry or authored intent;
- actual horizontal/vertical frame, axis order, XY/Z units and pixel interpretation;
- complete sampling support, NoData, masks, interpolation and local quality limits;
- intended Terrain/Water/object use and where that interpretation remains unknown.

Compare like surfaces after qualified frame conversion. A DSM-minus-DTM residual
is not automatically a ground error. A high-resolution or seamless raster can
still contain interpolated or historical surfaces. Numerically reproducible
sampling establishes processing, not present-day geographic truth. If the source
role is unsuitable, reconsider the model before refining a local splice.

## Provider products and limits

### Copernicus GLO-30

The [provider definition](https://dataspace.copernicus.eu/explore-data/data-collections/copernicus-contributing-missions/collections-description/COP-DEM)
identifies an edited DSM, including buildings, vegetation and infrastructure.
TanDEM-X acquisition is mainly 2011-2015; gap-fill sources can be older. The DGED
product uses WGS84 horizontal coordinates, EGM2008 heights in metres and point
posts. Resampling does not turn those heights into bare earth. Product quality
layers include editing, filling, height-error and Water masks; they are supporting
evidence, not an automatic building-removal operation.

A [TanDEM-X/LiDAR study](https://doi.org/10.3390/rs11070876) reports reduced urban
agreement associated with radar layover and shadow among tall buildings and
narrow streets. This supports checking neighborhoods beyond exact building
footprints. It neither diagnoses an individual ALIS pixel nor proves the same
error magnitude in the edited, coarser Copernicus product.

### NYC 2017 LiDAR products

NYC's [provider explanation](https://maps.nyc.gov/lidar/2017/) links to its
[technical StoryMap](https://storymaps.arcgis.com/stories/f6758ce6248645e2859de5448019185e).
The products share survey lineage, but represent different surfaces:

| Product | Meaning and possible use |
|---|---|
| Base Bare Earth DEM | Ground-point TIN with artifact treatment; candidate Terrain evidence |
| Hydroflattened DEM | Derived ground model with edited Water surfaces; shoreline reference, not an operating fountain stage |
| Hydroenforced, unfilled | Drainage paths cut into the model; analytical flow surface |
| Hydroenforced, filled | Depressions filled for flow analysis; altered elevations, not default physical ground |
| Highest Hit DSM | Elevated surface context; not interchangeable with bare earth |

NYC describes automated/manual artifact cleanup and bridge "saddling":
interpolation can create an artificial rise after bridge removal. Breaklines
constrain TIN edges rather than merely smoothing raster heights. Their existence
does not prove a particular ALIS seam has that cause or supply its correct grade.
NYC documents original State Plane Long Island coordinates in US survey feet
and NAVD88/GEOID12B; check the actual delivered product and processing lineage.

The [NOAA dataset 9307 record](https://www.fisheries.noaa.gov/inport/item/64732)
names "2017 NYC Topobathy Lidar DEM: New York City", a bare-earth model from Ground
and bottom returns collected in May-July 2017. This name and NYC's "Base Bare Earth
DEM" are not interchangeable product identities. Shared survey context proves
neither byte equivalence nor different samples; compare exact objects, metadata
and processing lineage before substituting one for the other.
Its [original files and metadata](https://noaa-nos-coastal-lidar-pds.s3.amazonaws.com/dem/NYC_topobathy_BE_DEM_2017_9307/index.html)
are distinct from a customized download, which may change frame or units. The
record reports unrestricted access and warns about temporal change and fitness
for use. Retain exact product terms and attribution through normal rights
admission; this is not approval to redistribute any other NYC product.

The [official NYS tile index](https://elevation.its.ny.gov/arcgis/rest/services/Dem_Indexes/MapServer/0)
identifies the 2017 NYC topobathymetric DEM collection. Tile footprints locate
candidate files; they do not prove valid data at every interpolation stencil.
Use metadata and a bounded tile selection before considering a survey download.

### NYC 2010 and independent references

The [City's 2010 DEM metadata](https://github.com/CityOfNewYork/nyc-geo-metadata/blob/main/Metadata/Metadata_DigitalElevationModel.md)
describes an interpolated, hydroflattened bare-earth raster, including treatment
beneath buildings and over Water. It lists public access alongside a DEP approval
requirement for access/use. Resolve that product-specific condition before use or
redistribution; an open download alone does not settle it. An older Ground model
can help diagnose change but cannot certify current conditions.

[USGS 3DEP](https://www.usgs.gov/3d-elevation-program/about-3dep-products-services)
offers point clouds and source-resolution DEMs. Before treating a comparison as
independent, inspect its acquisition/project lineage: another distributor or
derivative of the same survey is not an independent observation. Compare stable
dry locations in a common qualified frame, not just two nearby raster values.

### USGS seamless 1/3-arc-second DEM

The [USGS collection](https://data.usgs.gov/datacatalog/data/USGS%3A3a81321b-c153-416f-98b7-cc8e5f0e17c3)
is a roughly 10 m bare-earth model assembled from diverse source data. Seamless
coverage does not imply one survey epoch or independent observations from another
product with shared input lineage. Retain the selected tiles' FGDC acquisition
periods and processing metadata; a filename or publication date is not a local
survey date. The [seamless DEM specification](https://www.usgs.gov/publications/national-map-seamless-digital-elevation-model-specifications)
distinguishes source dates from tile dates and defines hydroflattened topography
for mapping. Edited Water elevations are not submerged ground, a current tide or
an operating fountain level.

The [USGS frame guidance](https://www.usgs.gov/faqs/what-projection-horizontal-datum-vertical-datum-and-resolution-a-usgs-digital-elevation-model)
identifies geographic NAD83 coordinates for this resolution and generally NAVD88
heights in the conterminous United States. The selected native headers and FGDC
metadata must qualify the actual frame and metre units before conversion; an
absent band-unit field does not imply metre elevations. The
[supplemental collection binding](supplemental/ground_products.md) admits the
qualified EPSG:4269/NAVD88-metre case with both original objects and matching
provider evidence. It does not add a base adapter, select geographic authority
or certify final Terrain/Water quality. Pixel spacing and published accuracy
statistics are not local hard error bounds.

## Professional treatments and the ALIS boundary

The [USGS surface-treatment specification](https://www.usgs.gov/ngp-standards-and-specifications/lidar-base-specification-digital-elevation-model-surface)
distinguishes ground beneath removed bridge decks from elevated structures and
requires logical interpolation and supporting breaklines. Hydroflattening edits
the derived surface, not the original point geometry. It does not guarantee
small constructed basins are modeled, establish a fountain's stage or provide
submerged ground. Do not substitute flattened Water height for bathymetry.

[PDAL ground filtering](https://pdal.io/en/stable/stages/filters.smrf.html) can
classify point clouds; classification and selecting existing class-2 returns
are different processing operations. Neither recovers ground hidden from the
sensor. Preserve the original classification, interpolation support and unknowns
when qualifying a derived model; do not add a filter framework without a case.

A suitable bare-earth candidate can address a source-role mismatch without
preserving object-influenced DSM boundaries. Its domain, boundaries, Water and
playable use still require admission. A coherent domain follows supported ground
and intended use, not an arbitrary rectangle or an obligation to retain every DSM
boundary. Missing support remains unknown. Any authored game-world reconstruction
stays distinct from observations.

## Engineered shoreline evidence

The [Hudson River Park bulkhead history](https://hudsonriverpark.org/the-park/waterfront-transformation/historic-hudson-river-park-bulkhead/)
supports an engineered retaining-shore interpretation along Manhattan's West
Side. It does not locate an individual wall or provide its elevations. The
[Pier25/Pier26 survey in Appendix 8](https://hudsonriverpark.org/app/uploads/2020/03/2019_Pier_25_Marine_Facility_RFP_updated.pdf#page=42)
distinguishes bulkhead and pier faces, but describes shoreline/structure positions
as approximate imagery-derived context. Its sounding depths use feet and MLLW;
they are not canonical EGM2008 Water levels or surveyed wall heights.

The [NYC 2017 shoreline metadata](https://github.com/CityOfNewYork/nyc-geo-metadata/blob/main/Metadata/Metadata_TidalShoreline.md)
declares State Plane Long Island NAD83 US feet, differing tidal capture conditions
and discontinuous coverage. Preserve each segment's capture type and epoch;
missing segments are not observations. Visible water edges, pile-supported deck
faces, landward bulkheads and a canonical Water-mask edge are different possible
roles. Neither their proximity nor a height residual identifies them automatically.
Compare supported segments in a common frame before modeling a retaining edge.
A plan disagreement requires a role/epoch/position disposition; it does not by
itself prove which source is wrong or authorize moving Water to fit a wall.

## Existing capabilities and comparison exit

The [source profile owner](app/profiles.py) supports the declared base adapters;
there is no general NYC DEM adapter. The [mosaic owner](app/raster_mosaic.py)
combines compatible Copernicus components, not Ground-over-DSM priorities. A URL
swap is not a qualified provider change. The compiler's
[base raster alignment](../CanonicalCompilation/app/terrain.py) is not a general
vertical-datum conversion stage for an arbitrary incoming NYC raster.

Already qualified, frozen Ground products can use the
[supplemental admission owner](supplemental/README.md). Existing
[raster Ground construction](../CanonicalCompilation/terrain_refinement.md#raster-ground-construction)
constructs unpublished selected quads and refuses changed outer heights; it is
not a regional base-source selector. Reuse these boundaries. Implement a missing
selection/adaptation capability only after an admissible source/model demonstrates
the need; retain one canonical Terrain authority and its Water dependencies.

A source-model comparison should stop with a bounded answer:

1. Identify the proposed domain, physical use and exact available source products.
2. Establish dry-ground and Water support, halos, frame/epoch compatibility and
   expected Base-versus-Hydro differences before generating a new surface.
3. Test a small representative neighborhood and every required boundary with
   explicit slope, continuity, Water-contact and uncertainty criteria. A sampling
   certificate alone cannot establish these properties.
4. When a demonstrated failure drives implementation or a missing regression,
   distill it into tracked, source-faithful inputs with provenance and an offline
   replay. Preserve masks and necessary neighboring faces. A rejected source/model
   comparison alone does not require another permanent fixture milestone.
5. Report a viable candidate and the exact missing owner capability, or the
   specific unsupported scope. Do not extend the search repeatedly, invent
   heights, implement a generic selector or start native verification to rescue
   an unsuitable model. Geographic acceptance follows its own owner and authority.
