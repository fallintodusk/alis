# Harden Vitals Integration Tests

Status: uninvestigated

## Problem

Vitals has unit coverage, but its cross-plugin runtime and presentation path
does not yet have a concise accepted integration proof.

## Investigation boundary

- Identify the current ProjectVitals and ProjectVitalsUI integration boundary.
- Cover data load, state transition, ViewModel update, and visible HUD behavior.
- Prove test selection and a representative packaged runtime route.

Do not duplicate vitals behavior in test documentation.
