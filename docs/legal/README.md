# Legal and Licensing Router

The root [LICENSE](../../LICENSE) is the sole operative component-license SOT.
This directory explains compatibility, provenance, and release enforcement
without redefining those assignments.

## Routes

| Goal | Source |
|---|---|
| Read the operative component assignment | [Root LICENSE](../../LICENSE) |
| Understand why the assignments fit Unreal and other dependencies | [Compatibility and Enforcement](component_license_policy.md) |
| Resolve component inheritance or generate the effective report | [Component Boundary Inheritance](component_license_policy.md#4-component-boundary-inheritance) |
| Check reciprocity or dependency compatibility | [Component safeguards](component_license_policy.md#71-what-reciprocity-can-and-cannot-guarantee) |
| Check contributor legal terms | [Root contribution terms](../../LICENSE#5-contributions) |
| Approve world data, generated artifacts, and `.uasset` publication | [World Data and Unreal Asset Policy](world_data_and_asset_policy.md) |
| Approve a public packaged Product | [Packaged Product Legal Compliance](release_compliance.md) |
| Understand the current social contract | [ALIS Pact](../../ALIS_PACT.md) |
| Contribute under the current terms | [Contributing](../../CONTRIBUTING.md) |
| Protect the ALIS identity | [Trademark Policy](../../TRADEMARKS.md) |

## Hard Boundary

License classification follows how a component is combined and distributed,
not whether a source file happens to include an Unreal header.

```text
Loaded or linked into Unreal
    -> UE-facing policy

Separate executable or service using files or a documented protocol
    -> independent software policy

Shared schema or interoperability contract
    -> neutral interface policy

Asset or generated artifact
    -> content, data, and provenance policy
```

Unknown ownership or conflicting terms fail closed: do not publish, package,
or mirror the material until the boundary is resolved.
