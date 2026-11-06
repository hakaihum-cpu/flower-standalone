# FM-1 / CircleCI host compilation — regression ledger (2026-11-04)

Branch: `feature/fm1-miyako-port-20261104` in `hakaihum-cpu/flower-standalone`.

## Failures and preventative code changes

| Previously reported or found | Exact cause | Permanent regression protection |
|---|---|---|
| `FAIL unsupported target dependency: new` | Raw substring search matched English in a C block comment | `check_contract.py` strips C comments; positive/negative fixture ensures real `malloc(` is still forbidden |
| #247 `eng_miyako.c:179: F_INT undeclared` | Host `test_engine.c` contained `#define F_ENUM 1` and `#define F_PCT 2`, but no `F_INT` | Replaced with **all 16** format enum values in pinned KARMA `core.h` order (`F_INT=0`, `F_PCT=1`, `F_ENUM=8`) plus `_Static_assert`. Static preflight verifies the complete list and the live engine's `F_*` references. This is the actual #247 corrective commit, not just a previously claimed fix. |
| Follow-on boot test `FELUCCA_VERSION` undeclared (found by static review before the next reported run) | `mi_splash.c` requires the firmware-version macro but `test_boot_ui.c` never defined it | Host boot harness defines `FELUCCA_VERSION "MIYAKO FM1 P1"`, enforced by `check_contract.py` |
| Undefined host `P_E0..P_E7` or `ENGI_ACID` (potential regression) | A native source may introduce symbols the stub does not provide | `check_contract.py` scans source-code identifiers and rejects missing parameter IDs |
| Host compile errors found only after processing 576 JPEGs (#247) | Host C test compilation ran after video generation | CircleCI now runs `cc -std=c99 -Wall -Wextra -Werror=implicit-function-declaration -fsyntax-only` on 4 media-independent host tests **immediately after the `gcc` prerequisite installation, before Python video dependencies, media processing and toolchain downloads**. |

## Check scope and honesty
- Verified directly in the GitHub branch: all five C harness source files exist, their transitive include dependencies are present, all native engine format identifiers map to host declarations, the enum order matches pinned KARMA, the firmware version macro is provided, and the CI syntax gate precedes dependency downloads.
- These are **source-level checks**. A real `cc` execution and all ASan/UBSan checks have **not** been executed within this chat, and the JieLi target firmware has not been compiled here.
- The CircleCI manual gate remains `run_build` default `false`; no workflow or pipeline has been triggered by this repair.
- The CircleCI branch listing UI and the historical `<<` YAML error cannot be certified by GitHub source inspection alone. A valid branch is present and the configuration contains only the intended parameter interpolation.

Next allowed validation is the existing CircleCI `androidapp-build` pipeline with both source branches set to this feature branch and `run_build=true` **only when the user authorizes execution**.

## Pool headroom failure (observed JieLi target link)
Observed: .pool 341632 / 344064, spare 2432 < required 8192.
Fixed in source by bounding the MIYAKO track-1 arena under Juno's
existing 2456-byte arena and relocating only the second 441-sample native
reverb allpass (882 bytes) from .pool to .bss. EUREKA preserves 256..3304
input-sample slice timing via 4:1 capture with interpolated playback.
Projected next link: .pool free 9086, .data+.bss 97610 / 98304; no
linker guard weakened. Native target link has NOT yet been rerun.
