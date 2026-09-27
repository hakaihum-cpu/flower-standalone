# ADR-0001: Keep bootstrap product-neutral

Status: Accepted for bootstrap

## Decision
The initial application contains only the minimum code required to prove the Android project structure.
No UI framework, persistence framework, network stack, DI framework, or product feature is selected.

## Reason
The product requirements have not yet been defined.
Choosing frameworks now would create assumptions and unnecessary rework.

## Consequence
The first feature issue may introduce architecture only when justified by its requirements.
