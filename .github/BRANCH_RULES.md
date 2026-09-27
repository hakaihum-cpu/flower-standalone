# GitHub branch rules for androidapp

## main
- Golden Baseline only.
- No direct routine development.
- Changes arrive only through pull request.
- Merge only after explicit user approval that the revision becomes the new Golden Baseline.
- Never rewrite history.

## feature/*
Recommended naming:
`feature/AN-<issue-number>-<short-name>`

## fix/*
Recommended naming:
`fix/AN-<issue-number>-<short-name>`

## release/*
Used only when a release candidate must be stabilized separately.

## CI cost-control rule
GitHub Actions are not used.
CircleCI pushes do not build by default; build workflow runs only when pipeline parameter `run_build=true`.
