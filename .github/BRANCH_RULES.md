# Branch rules

## main
Golden Baseline only. No routine direct development. Merge only after explicit user approval.

## feature/* / fix/*
One issue/scope per branch. No unrelated edits.

## CI
GitHub Actions are not used. CircleCI builds only with manual pipeline parameter `run_build=true`.