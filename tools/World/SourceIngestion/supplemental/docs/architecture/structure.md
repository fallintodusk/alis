# Supplemental source admission

SourceIngestion owns admission of an already qualified derived Ground product.
It hides capsule validation, retention and candidate publication from canonical
consumers. It owns neither raw decoding nor geographic quality or selection.
The [relationship view](diagrams/main.md) defines the immediate composition.

| Part | Responsibility |
|---|---|
| [Public API](../../api.py) | Admission and immutable authenticated reads |
| [Implementation](../../app/) | Validation and publication |
| [Contracts](../../contracts/) | Closed product and receipt shapes |

Executable acceptance and refusal behavior runs through the parent
[SourceIngestion test route](../../../README.md#commands).

The [Ground product contract](../../ground_products.md) owns qualification
semantics, retention, recovery and acceptance limits. No independent cache,
geographic compiler or source-acquisition service is introduced.
