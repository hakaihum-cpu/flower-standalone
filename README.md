# androidapp

Bootstrap package for a new Android application project.

## What is intentionally included
- Minimal Android app shell
- Governance contract
- GitHub PR template and branch rules
- Jira seed issues
- Confluence source documents
- CircleCI manual-build configuration
- Build history schema

## What is intentionally NOT decided
- Product functionality
- Product UI
- Architecture frameworks
- Account/network/storage design
- Distribution/signing

## CI safety
CircleCI's Android build workflow is guarded by the boolean pipeline parameter:
`run_build=false` by default.

A normal source push therefore does not execute the build job.

## First real development step
Define the product purpose and first feature as Jira requirements before changing product code.
