# Implement Combat Initialization

Status: uninvestigated

## Problem

ProjectCombat exposes combat types but its module startup does not establish a
verified runtime integration route.

## Investigation boundary

- Identify the intended combat authority and current consumers.
- Trace module, feature, controller, and ability-system initialization.
- Define the smallest supported combat slice before adding behavior.
- Prove the selected route with focused automation and packaged runtime
  evidence.

Do not infer a required system from placeholder types alone.
