# Packaged Product Legal Compliance

The root [LICENSE](../../LICENSE) owns component assignments. This document
owns the permanent legal gate for public packaged Products and does not change
source licenses.

This is a release specification, not evidence that its required Product terms,
inventories, notices, source offers, provenance outputs, or automation already
exist. A policy link never satisfies a gate that requires a generated artifact.

## 1. Release Classes

```text
Source archive
    -> root license policy, applicable standard texts, notices, and source

Packaged game
    -> source obligations above plus Product terms, Epic conditions,
       content permissions, credits, and release-specific attribution

Editor or development tooling
    -> separate Engine Tools distribution review
```

A successful build, checksum, or signature does not prove legal
redistributability.

## 2. Rights Clearance

Before public distribution, the release operator must confirm that:

- every first-party portion is controlled by its identified copyright holder
  and may be distributed under the root assignment;
- contributor identity aliases and employer ownership questions are resolved
  in private evidence records;
- accurate copyright, upstream, and proprietary notices are retained;
- every dependency, asset, dataset, and generated input has compatible terms;
- private official layers remain optional relative to the
  [public survivability floor](../../ALIS_PACT.md#public-survivability-floor);
  and
- unresolved ownership blocks the release.

The public repository must not contain personal identity evidence. Record only
the release decision and non-personal component provenance publicly.

The durable private evidence record must bind the decision to a repository
revision and record:

- author-alias and automation-identity groups;
- employer or contractor ownership assessment;
- copied, generated, and imported material assessment;
- known third-party exclusions;
- controlled, excluded, and unresolved decisions;
- the responsible project role; and
- an opaque evidence-record ID suitable for the public release manifest.

Unresolved identity or material remains excluded. Do not publish the private
record or copy its personal fields into GitHub.

## 3. Product Terms

A packaged game must include approved end-user terms that:

- grant the narrow rights needed to install and play the packaged Product;
- cover private official art, story, music, levels, and world identity;
- explicitly disclaim representations, warranties, conditions, and
  liabilities related to Unreal Licensed Technology;
- do not restrict rights already granted under MPL, AGPL, Apache, Creative
  Commons, ODbL, CDLA, or another applicable upstream license;
- keep trademarks separate; and
- identify where recipients can obtain covered source and notices.

Public packaged distribution is blocked until those Product terms have been
reviewed and included. Do not improvise operative EULA language in a build
script.

## 4. Unreal Distribution Checks

For the exact Unreal version and agreement accepted by the release operator:

1. Ship Engine Code only in object code and only as an inseparable part of the
   Product.
2. If the Product has credits, include Epic's then-current prescribed Unreal
   notices.
3. Check whether the package contains Engine Tools. Public Engine Tools
   distribution must use a channel permitted by Epic.
4. Retain Epic proprietary notices wherever they appear in Licensed
   Technology or Epic-derived material.
5. Record the agreement revision used for the release review.

Primary contract: [Unreal Engine EULA](https://www.unrealengine.com/eula/unreal).

## 5. Manifest-Driven Release Bundle

The signed release manifest must identify the actual paths or URLs for:

- a non-personal rights-review result containing the reviewed revision,
  completion state, and opaque private evidence-record ID;
- each distributed first-party path's root component assignment;
- the root license policy and applicable standard license texts;
- approved Product terms;
- open-source and third-party notices;
- source locations or source offers required by the covered components;
- world-data attribution and database or alteration offers when applicable;
- Product credits when applicable; and
- the source revision and release tag.

Generate only applicable artifacts. Do not create empty notice files merely to
satisfy a fixed filename checklist.

## 6. Signing Boundary

Release signing authenticates exact manifest bytes and their listed payloads.
The trust workflow owns keys, fingerprints, and verification. Signing does not
replace rights clearance, notices, source availability, Product terms, or
Epic distribution review.

Previously distributed source revisions and releases retain their grants.
Never replace assets under an existing release tag to simulate a legal or
metadata correction; publish a new release record.

## 7. Public Source and Tagged Releases

A public source branch is a source distribution. Each published revision must
contain the root `LICENSE`, applicable standard texts, local notices, source
pointers, and provenance records. It must pass the repository licensing and
mirror checks. It does not need a generated manifest or release bundle merely
because the branch advanced.

A tagged source release adds:

- a clean source tag;
- an effective-component manifest reproducibly generated from that exact tag;
- a source archive, notices, and signed release manifest;
- the non-personal rights-review result defined above; and
- release notes identifying the source revision and tag.

Prepare and verify the exact tag and complete release bundle before publishing
the tag. Only then update site and promotion links to that public release.
Previously published revisions and tags retain their original grants. A
packaged Product remains separately blocked until every earlier gate in this
document is satisfied.
