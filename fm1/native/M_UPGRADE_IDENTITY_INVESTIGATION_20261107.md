# MIYAKO FM-1: installer-visible package identity correction (2026-11-07)

## Actual evidence and limits

- M-UPGRADE loaded the separate stock `FM-1.fwsc` as `FM-1_016`.
- The current device reports `FM-1_926` (KARMA).
- The previous MIYAKO `fm1.toml` was `id_prefix = "FM-1_91"`.
  Pinned `fm1-base/tools/build.py` appends `"00"` for non-release
  firmware, which produces `FM-1_9100`, not three-digit 910 or 911.
- In the M-UPGRADE session logs furnished by the user, MIYAKO never reached
  the `firmware.loaded` event. The UI reports "Invalid OTA firmware:
  missing or incomplete model, version, or metadata."
- These logs do NOT isolate which parser rule rejects MIYAKO. In particular,
  the same error appears during device scans, without a selection event.
- Stock `FM-1_016` being selectable proves the updater can read some FWSCs
  but does not imply that generated custom packages share identical metadata.
- `firmware.json` (tagline/install/screenshot) is a separate web-catalogue
  descriptor produced by fm1-base. It is NOT embedded as the FWSC identity
  marker, so inventing a screenshot or URL is not an OTA-format fix.

## The controlled trial

The prior four-digit MIYAKO identity is removed from active build settings.
Use `FM-1_931`, a distinct three-digit identifier relative to the current
`FM-1_926`. `FM-1_931` did not appear in the published FM-1 firmware hub's
identity list when checked, but that is not a reservation or exclusive right.
The pinned upstream build.py applies the same `PRODUCT` to app CFLAG
`FELUCCA_ID` AND to `fm1pkg_make.ufw(..., PRODUCT)`, so edit the
developer build to `id_prefix = "FM-1_9"` + suffix `"31"` BEFORE
compilation. Do not rewrite only the 20 FWSC marker bytes afterward:
the app's runtime identity must agree.

CI verifies that the package parser extracts exactly `FM-1_931` and
the firmware has the expected OTA loader. This is NOT a substitute for
trying to SELECT (not yet INSTALL) the new FWSC in M-UPGRADE.

## Necessary acceptance sequence

1. New CircleCI run must show `package ... identity FM-1_931`, with
   all link, audio, CRC, and native loader checks passed.
2. User may select that specific SHA256-verified FWSC in M-UPGRADE.
3. Only when M-UPGRADE shows the selected firmware ready and **without**
   the metadata/model/version error can installing be considered.
4. Physical device, sound and LCD behavior remain unverified.

Do not claim the M-UPGRADE failure is fixed before step 2 succeeds.
Do not modify or flash the user's hardware without consent.
