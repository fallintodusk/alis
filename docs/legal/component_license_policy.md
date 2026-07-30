# ALIS License Compatibility and Enforcement

Status: canonical compatibility rationale and enforcement policy.

The root [LICENSE](../../LICENSE) is the sole operative component assignment.
This document owns the compatibility reasoning, boundary tests, and dependency
guards; it does not restate or replace the operative terms.

This is an engineering compliance decision, not a substitute for advice from
qualified counsel or written confirmation from Epic.

## 1. Operating Rule

Use standard licenses without custom text. Do not add MPL Exhibit B or an
AGPL Unreal linking exception without a new review.

The Apache boundary is deliberately narrow. It lets independent games,
servers, and tools speak the ALIS protocol without asking permission. Core
gameplay, authority, persistence, compiler algorithms, and runtime behavior
remain inside the MPL or AGPL reciprocity perimeter.

## 2. Why MPL-2.0 for UE-Facing Code

Epic's current EULA prohibits combining Licensed Technology with code whose
license would directly or indirectly require any Epic technology to be
governed by different terms. It names GPL, most LGPL combinations, and
CC BY-SA as examples.

MPL-2.0 does not impose that result:

- MPL section 1.10 defines modifications at file scope.
- MPL section 3.1 keeps the covered source files and modifications under MPL.
- MPL section 3.2 requires a source offer for those covered files when
  executable form is distributed.
- MPL section 3.3 permits a Larger Work under terms chosen by its distributor.
- MPL section 2.1 grants copyright and contributor patent rights.

Mozilla describes MPL as file-level copyleft that permits static linking into
a proprietary Larger Work. An Epic representative has also stated that a
developer may license its own UE-referencing code under MPL-2.0 when Engine
Code is not brought under MPL. Earlier Epic guidance called the combination
only potentially compatible and required careful separation. Both answers are
forum guidance, not a contract amendment or an ALIS-specific approval.

This makes MPL-2.0 the strongest established reciprocal license for the
UE-facing boundary supported by both licenses' text and published Epic
guidance. Apache-2.0 remains the lower-friction fallback, but it permits closed
modifications and therefore does not satisfy the ALIS reciprocity goal as well.

Published guidance is not a contract amendment. Releases must preserve the
file and process boundaries above and comply with the then-current Epic terms.

## 3. Why Plain AGPL Does Not Cross the UE Boundary

AGPL section 5 requires a covered combined work to be licensed as a whole.
Its Corresponding Source definition can include specifically required linked
components. Applying unmodified AGPL to code compiled or loaded as part of an
Unreal Product therefore creates a release-blocking conflict risk under Epic's
rule.

AGPL is not one of the licenses named in Epic's examples, and no cited court
decision settles this exact combination. Treat it as unapproved and high-risk,
not as an adjudicated fact.

Downloading Unreal separately does not change the combination created at
build and runtime. Absence of copied Engine Code from Git only resolves the
Epic-source redistribution question.

An AGPL section 7 linking exception is technically possible, but it is not the
selected default because:

- it is custom text requiring separate interpretation;
- every relevant copyright holder must grant it;
- no current written Epic approval for an ALIS exception has been obtained;
- contributor handling becomes more complex; and
- MPL already provides standard file-level reciprocity for distributed UE
  products.

Keep AGPL where the process boundary is real. A separately launched server or
compiler that communicates through files, HTTP, or another documented
protocol does not become part of the Unreal executable merely because Unreal
calls it.

## 4. Component Boundary Inheritance

The owning component and declared distribution boundary in a repository
revision fix the file's assignment for that revision. The decision tree below
is used when creating, moving, or splitting a file; hypothetical later use
does not change an existing grant.

Do not maintain an exhaustive repository path map. Classification inherits
from existing ownership boundaries:

1. An explicit component-local declaration controls genuine exceptions,
   upstream material, and first-party tool roots that have no native package
   manifest. A `LICENSE` or `NOTICE` participates in ALIS classification only
   when it contains `ALIS-Component-Class`.
2. Native package boundaries classify normal components: Unreal project and
   `.uplugin` modules, Cargo packages, separate services, and deliberately
   marked interoperability packages.
3. Documentation inherits the root documentation class. Assets and world data
   remain provenance-controlled until their manifests grant specific terms.
4. Unknown or conflicting evidence blocks distribution.

Only an exception or a component without a native package manifest needs local
machine-readable fields:

```text
ALIS-Component-Class: original-terms
ALIS-Public-Source: excluded
```

The first field selects a root class or `original-terms`. The optional second
field makes the existing public-mirror policy prove the matching exclusion.
Components with native Unreal or Cargo boundaries must not repeat these fields.

The repository validator checks explicit metadata consistency only. It does
not infer ownership or generate a complete legal classification from folder
names, filename suffixes, or copyright strings.

Any effective-report resolver must find the nearest explicit ALIS declaration
first, then the nearest native component root. Unreal paths are interpreted
relative to the discovered `.uproject` or `.uplugin` root:

| Component-relative path | Result |
|---|---|
| Descriptor, native build metadata, `Source/**`, `Config/**`, or functional `Data/**` | `ue-in-process` |
| `Content/**` or `Resources/**` | `provenance-required` |
| Component documentation | `documentation` |
| Standalone first-party tooling or configuration at the repository root | `separate-process` |
| Anything else | Unknown; require an explicit local declaration |

Therefore `Plugins/Resources/ProjectAudio/Source/**` is UE source: the
repository category named `Resources` is outside the `ProjectAudio` plugin
root and has no classification meaning. Cargo packages inherit
`separate-process` unless their native metadata explicitly declares another
root class. Every Cargo package's effective license field must match that
class.

An unexpected Epic or other ownership notice is evidence, not a classifier.
The release resolver must stop until a reviewed local declaration records the
decision. Unmarked `LICENSE` and `NOTICE` files remain legal evidence and do
not reclassify their directory.

Generated reports have two distinct trust modes:

- A development report may inspect the working tree, must identify itself as
  ephemeral, and cannot be release evidence.
- A release report requires a clean tree, reads paths from one exact commit,
  records that commit SHA, and must be reproducible from that commit.

Only the release report may enter a signed release manifest. Generate it with
`scripts/ue/check/governance/generate_component_manifest.py` from the clean
public source tag. The generated view is never an editable policy input or
SOT.

The current public source release is text-only. Tagged public asset payloads
under component-relative `Content/**` or `Resources/**` fail closed until ALIS
approves and implements a machine-readable provenance sidecar contract.

```text
Does the component copy Epic code or restricted Epic content?
    yes -> do not publish; apply Epic distribution terms
    no
      |
      v
Is it linked, loaded, imported, or executed inside an Unreal process?
    yes -> ue-in-process
    no
      |
      v
Is it a neutral contract consumed on both sides?
    yes -> interoperability
    no
      |
      v
Is it a separate ALIS executable, CLI, or service?
    yes -> separate-process
```

Examples:

| Example | Classification |
|---|---|
| C++ file with no UE include but linked into a UE module | UE-facing |
| Python file that imports `unreal` | UE-facing |
| PowerShell script that launches Build.bat as a child process | Separate developer tool |
| Rust compiler that emits a manifest consumed by UE | Separate tool |
| JSON schema shared by compiler and UE adapter | Neutral contract |
| UE dedicated server target | UE-facing Product, not an independent service |

### 4.1 Headers, Modules, and Downloadable Plugins

An include statement is evidence of a dependency, not a transfer of ownership
or a new license grant:

| Dependency | ALIS treatment |
|---|---|
| Built-in Unreal header, module, or Epic plugin | License ALIS-owned files under the UE-facing policy; do not publish Epic source; require developers to obtain Unreal under Epic's terms |
| Fab or Marketplace plugin, free or paid | Follow the exact listing and provider terms; publish only ALIS integration files and a dependency manifest unless redistribution is expressly allowed |
| Open-source UE plugin | Check its exact license and dependencies before combining; retain required notices |
| Copied upstream code, generated glue, or modified vendor file | Classify the copied material itself; a separate download instruction does not remove its license |

"Free," "included," and "easy to download" describe price or acquisition.
They do not make Epic or third-party code open source and do not permit ALIS to
relicense it.

Retain accurate Epic, upstream, and contributor ownership notices. A root
license pointer supplements those notices; it never replaces them.

## 5. Repository Audit

- `Source/Alis` and the first-party plugin modules are built through Unreal
  Build Tool and depend on Epic modules such as Core, CoreUObject, and Engine.
- `AlisServer.Target.cs` is an Unreal Server target. Authority code currently
  lives inside UE gameplay modules, so hosted dedicated-server changes would
  not receive AGPL's network trigger under MPL.
- Some Python automation imports `unreal` and belongs to the UE-facing class.
- Other scripts and the Rust Build Service invoke Unreal command-line tools as
  separate processes and can remain independently licensed.
- `tools/BuildService/Cargo.toml` inherits the separate-process AGPL
  assignment.
- The working repository tracks UE assets, including third-party and
  MetaHuman content. The public mirror intentionally excludes Content,
  `.uasset`, `.umap`, third-party plugins, and all binary payloads.

Architecture consequence: future network-sensitive authority should move into
an independent AGPL service only where latency, trust, and gameplay design
permit it. Do not pretend the current Unreal dedicated server already has that
boundary.

## 6. Unreal Assets

Epic states that ALIS owns rights in its Product other than Epic Licensed
Technology. Epic also identifies self-developed asset files, excluding Starter
Content, as Non-Engine Products in qualifying cases. The `.uasset` container
therefore does not make an ALIS-authored work Epic-owned.

Apply the license by what the asset contains:

| Asset | Policy |
|---|---|
| Original mesh, texture, sound, animation, or material | Apply the root public-asset rule only when intentionally released |
| Blueprint graph or other program logic | Apply the root UE-facing rule and provide the preferred editable source |
| Original asset that only references a separately fetched dependency | Conditional; publish the reference and dependency manifest, not the dependency |
| Generated terrain, building, road, or cell | Calculate from the full provenance graph |
| Fab, Marketplace, Megascans, MetaHuman, Starter Content, or other licensed payload | Do not place its source in a public repository unless its exact terms expressly allow it |
| Mixed or undocumented asset | Quarantine |

CC BY 4.0 permits commercial use and modifications while requiring
attribution and change indication. It does not license trademarks. Exclude the
ALIS name, logos, and brand identifiers explicitly through the trademark
policy.

Do not use CC BY-SA or CC BY-NC-SA for public production assets:

- Epic expressly identifies CC BY-SA as non-compatible.
- NonCommercial blocks ordinary commercial use by community developers.
- maintainers cannot promise commercial exceptions for contributor-owned
  material unless they actually hold those rights.

Keep hero content private when attribution-only reuse is too permissive.
There is no standard Creative Commons option that simultaneously provides
commercial developer freedom, source reciprocity, and confirmed Unreal
compatibility.

## 7. What Developers Receive

| License | Developer assurance | ALIS assurance |
|---|---|---|
| MPL-2.0 | Commercial use, static UE integration, source access to covered files, contributor patent grant | Distributed modifications to covered files stay open |
| AGPL-3.0-only | Commercial use, source access, self-hosting, patent grant | Distributed and modified hosted-service code stays open |
| Apache-2.0 | Broad commercial interoperability and patent grant | Attribution and notices; no source reciprocity |
| CC BY 4.0 | Commercial asset reuse and adaptation | Attribution and change notices |

None of these licenses grants the ALIS trademark or makes Epic or third-party
material available under ALIS terms.

### 7.1 What Reciprocity Can and Cannot Guarantee

| Boundary | Must remain available | May remain private or differently licensed |
|---|---|---|
| MPL UE-facing code | Modified MPL files when their executable form is distributed | Private changes, hosted-only changes, and separate new files |
| AGPL service or tool | Covered source on distribution; modified service source offered to its remote users | Private changes that are neither conveyed nor offered as a modified network service |
| Apache protocol bridge | Notices and patent terms | Modified bridge code and independent implementations |
| CC BY public art | Attribution and change indication | Adaptations under other terms |
| ODbL database | Public Derivative Database or required alteration/source offer | Qualifying Produced Works under other terms, with attribution |

Copyleft does not force a pull request to ALIS. It gives recipients or service
users source rights. They may share that source anywhere, including upstream,
but the license cannot require ALIS to be the exclusive recipient.

The Apache bridge and CC BY asset boundary are deliberate freedoms, not
accidental holes. Apache prevents one project from owning the protocol. CC BY
avoids Unreal's ShareAlike conflict and permits developers to ship. Keep both
boundaries small: no core implementation belongs in Apache packages, and no
protected hero identity belongs in the public CC BY asset set.

### 7.2 Dependency Compatibility Gate

The root [LICENSE](../../LICENSE#5-contributions) owns the legal contribution
model. [CONTRIBUTING.md](../../CONTRIBUTING.md) owns submission mechanics.

No license name is an automatic approval. Review the exact version, source,
linkage, generated output, notices, and distribution mode.

| ALIS boundary | Normal candidates after notice audit | Reject or require specific review |
|---|---|---|
| UE-facing MPL files | Unreal under its separate EULA; MIT, BSD, ISC, Apache-2.0, and file-separated MPL dependencies | GPL, LGPL, AGPL, ShareAlike, NonCommercial, custom source-available terms, or any dependency that attempts to govern Unreal |
| Separate AGPL process | AGPL and compatible permissive dependencies | Proprietary linked SDKs, incompatible copyleft, field-of-use restrictions, or unclear transitive terms |
| Apache protocol package | Permissive dependencies only | MPL, GPL-family, proprietary, or implementation-bearing generated code |
| Public CC BY asset package | ALIS-owned, CC0, and compatible CC BY inputs | NC, ND, ShareAlike, Epic/Fab source content, or unknown ownership |
| ODbL database lane | Approved ODbL inputs and separately tracked compatible overlays | Unlicensed data, prohibited provider extraction, or content whose database rights cannot be reconciled |
| External CLI or hosted provider | Separate process or documented API with approved use terms | Copying its code, redistributing its payload, or assuming service access grants output ownership |

The repository must implement these guards:

1. Store every used standard license text under `LICENSES/`.
2. Keep the root `LICENSE` as the only ALIS component assignment. Source
   comments may point to it but must not copy license selections.
3. Record each dependency's version, source, hash, license expression,
   linkage mode, bundled files, generated output, and required notices.
4. Generate an SBOM, third-party notice bundle, source offers, and public asset
   manifests from that inventory.
5. Fail validation on unknown licenses, unapproved combinations, missing
   notices, dependency drift, copied Epic content, or files crossing their
   licensed boundary.
6. Re-run compatibility review whenever a dependency, provider agreement,
   generation template, build mode, or distribution channel changes.

These checks protect both directions: ALIS cannot silently absorb restricted
work, and downstream users receive enough information to exercise the rights
the component license promises.

## 8. Ongoing Release Gates

Every source and binary release must verify:

1. Rights-cleared first-party portions, `LICENSE`, source pointers, release
   manifests, and required package metadata agree.
2. Every dependency and generated input has an approved version, source,
   license, linkage mode, distribution class, and notice.
3. Public source contains no Engine Code or restricted Epic, Marketplace, or
   third-party payload.
4. MPL source availability, AGPL source offers, third-party notices, and data
   attribution are generated for the actual release contents.
5. New contributions identify their component boundary, ownership, and
   third-party provenance.
6. Unknown ownership, an unapproved license combination, or dependency drift
   fails the release.

Packaged Products must also pass
[Packaged Product Legal Compliance](release_compliance.md), including Product
terms, Epic notices, Engine Tools routing, credits, and source offers.

Re-run the compatibility review when Epic terms, a dependency license, linkage
mode, generated template, data provider, or distribution channel changes.

## 9. Primary Sources

- [Unreal Engine EULA](https://www.unrealengine.com/eula/unreal)
- [Epic Content License Agreement](https://www.unrealengine.com/eula/content)
- [Epic guidance on MPL-2.0 project source](https://forums.unrealengine.com/t/releasing-project-source-code-under-mpl-2-0/443610)
- [Earlier Epic MPL compatibility guidance](https://forums.unrealengine.com/t/how-compatible-is-ue4-to-code-under-mpl-2-0/329499)
- [MPL-2.0 license](https://www.mozilla.org/MPL/2.0/)
- [MPL-2.0 FAQ](https://www.mozilla.org/MPL/2.0/FAQ/)
- [AGPL-3.0 license](https://opensource.org/license/agpl-3.0)
- [GNU incompatible-library and exception guidance](https://www.gnu.org/licenses/gpl-faq.en.html#GPLIncompatibleLibs)
- [Apache-2.0 license](https://www.apache.org/licenses/LICENSE-2.0)
- [Apache-2.0 GPLv3 compatibility guidance](https://www.apache.org/licenses/GPL-compatibility.html)
- [CC BY 4.0 legal code](https://creativecommons.org/licenses/by/4.0/legalcode.en)
- [Creative Commons software and game-art guidance](https://creativecommons.org/faq/#can-i-apply-a-creative-commons-license-to-software)
- [Fab Standard License](https://www.fab.com/eula)
