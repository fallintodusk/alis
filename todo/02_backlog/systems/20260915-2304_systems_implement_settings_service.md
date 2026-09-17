# Implement Settings Service

Status: uninvestigated

## Problem

ProjectSettings contains configuration surfaces without a verified service
provider and consumer lifecycle.

## Investigation boundary

- Inventory current settings data, UI consumers, and persistence paths.
- Establish one settings authority and explicit lifetime.
- Define validation, defaults, persistence, and failure behavior.
- Prove runtime application and restart persistence through real consumers.

Do not add another settings store beside an existing authority.
