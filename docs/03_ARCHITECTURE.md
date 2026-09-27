# Architecture

## Current bootstrap architecture
- Single Android application module: `app`
- No network layer
- No persistence layer
- No third-party runtime libraries
- No analytics
- No authentication
- No product-specific permissions

This is intentionally minimal to avoid selecting architecture before product requirements exist.

## Architecture Decision Records
Architectural changes should be recorded under `docs/adr/`.
Use one ADR per material decision.
