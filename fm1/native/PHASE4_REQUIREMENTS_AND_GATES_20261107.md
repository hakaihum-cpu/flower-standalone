# FM-1 MIYAKO Phase 4 — requirements, source truth, and gates

Date: 2026-11-07. Golden baseline: **branch** `feature/fm1-miyako-recorder-eureka-fx-20261107`; prior latest confirmed `855d01c3a3a670b2cb0fe1a87ac0f08b0a012b48`.
No changes to `main`. No autonomous CircleCI/GitHub Actions builds. This inventory is a work ledger, not a statement of completed hardware validation.

## Working status — source commit tracking

This section records **source implementation**, NOT FM-1 flashability.

- **⑧ OSC2 — source implemented**: independent `osc2_wave`, `osc2_level`,
  `osc2_detune` (+/-50 cents), `osc2_octave` (-2..+2) per voice with own
  phase; original OSC1/SUB/CRUSH left intact. EDIT pressed again in OSC
  toggles OSC1/OSC2 control pages. AUDIO/MIDI 4-voice ARP paths unchanged.
- **③ and ④ — source implemented, output fidelity unverified**:
  DREAMY/EUREKA have six real FX-specific controls and one MODE/WET screen
  page; PRESETS encoder toggles page 1 (MODE, WET, two original options)
  and page 2 (PITCH, REVERSE, SPREAD or GRAIN, FEEDBACK).
  Pitch shifts -12 to +12 st, grain read-head reverses, no dummy controls;
  original JUCE effect behavior has NOT been established.
- **Saved user settings**: appended fields retain the MI13 prefix.
  `app.c` tries new full state, then loads old 68-byte MI13 stored state,
  copies the original prefix into initialized defaults and validates bounds.
  New stored state = 104 B (host x86-64 ABI check); old store = 68 B.
  Confirm FM-1 target ABI independently before release.
- **Local pre-build verification**: full source textual delimiter / include /
  reference scan passed for modified C/H; isolated GCC/Clang ASan+UBSan
  preflight tested pitch ratio across all 128 settings (max abs error
  0.00001669 versus exp2) and 16,362 DREAMY read-head positions
  (valid range 2377.5..31498.7 in the 32768-sample ring). Tested selected
  edge EUREKA parameters for bounded read locations. These are **isolated
  mathematical replays, not compiling original repo sources**.
- **Build gate remains RED**: Original-source JieLi target compilation,
  all-mode audio measurement, actual 240x240 display scan, app link budget,
  real source media packaging and user/device acceptance are still unverified.
  No CircleCI pipeline was triggered.

## Audio and storage regression audit

The DSP was replayed as a **separate native C audio simulation**, compiled
and executed with GCC and Clang AddressSanitizer/UndefinedBehaviorSanitizer,
then measured via FFT. With a 220 Hz sine input and the main DREAMY or EUREKA
grain path:

| mode | PITCH min | PITCH centre | PITCH max |
| --- | --- | --- | --- |
| DREAMY | dominant 110 Hz | dominant 220 Hz | dominant 442 Hz |
| EUREKA | dominant 104 Hz | dominant 220 Hz | dominant 442 Hz |

These measured pitch movements are *not proof* that the complete original
MIYAKO processors, FM-1 target DSP, latency or CPU budget are acceptable.
Synthetic test wave input and recreated processing equations were used.
Full native source compile, target link and flash packaging remain blocked.

**Storage API re-audit:** Pinned `fm1-base/device/storage.c` `st_load()`
truncates its result to a caller-provided maximum, therefore the old-store
migration must check `plat_store_size(OBJ_SETTINGS)` FIRST. The implementation
now distinguishes a real legacy 68-byte MI13 stored object from the new
104-byte saved object and rejects corrupt newer objects rather than
accidentally accepting their prefix as an old save. Exactly these size
checks are present in the source; target ABI has not been independently linked.

**CI source contract:** previously referenced the obsolete
`mi_fx_dsp_process` dispatch in `mi_audio.c`. It now checks the real
`mi_fx_dsp_process_ex` call with six routed FX parameters to avoid a repeat
of a stale-source assertion failure. No additional CircleCI test job or build
was started.

## 2026-11-07 most recent hardware feedback — ①〜⑧ (supersedes earlier task numbering)

Source branch `feature/fm1-miyako-recorder-eureka-fx-20261107`. This is **not a release**;
the fixes below are in committed source, not validated on the actual FM-1.
No CircleCI/GitHub Actions build was triggered.

| ID | Confirmed user requirement | Latest source code result | Acceptance / remaining hard gate |
| --- | --- | --- | --- |
| ① | Overall audio crackles/breaks up | Reduced unnecessary video CPU use: removed per-byte base64 decode and switched back to the **exact existing EFFECTS** compile-time RGB565 VQ palette/codebook/map arrays. This only addresses a potential video/UI load source. | **NOT FIXED/VERIFIED.** Need device XRUN/CPU/PCM comparison FX OFF, DREAMY, EUREKA, dual FX and simultaneous animation. No root cause established. |
| ② | FX: eliminate PREVIEW/AUTO/EUREKA/VHS labels; maximize right picture | All captions/metadata removed from image region; 116×116 source-square photo preview at x123, y62, four controls left. No extra PRESETS text. | Target LCD render visual check pending. |
| ③ | DREAMY/EUREKA PITCH/other physical params discoverable | FX K1 MODE selection of DREAMY/EUREKA automatically opens PITCH/REVERSE/SPREAD-or-GRAIN/FEEDBACK page; PRESETS encoder toggles MODE/WET/original controls. Real values route to DSP. | Host unit tests updated; source-accurate original effect audio not yet proved. |
| ④ | People visible; reuse same encoder as normal EFFECTS, no monochrome blob | All 488 user frames now 48×48 16-colour **existing 8×4/256-dictionary RGB565 VQ**; source generated from supplied contact sheets, stored as direct C palettes/codebooks/maps. Old 32×32 1-bit fallback removed. | Packed source SHA-256 `429f2aa8f561079481c3b60562ef3f88e2f5abd044a446eca1b1ebe1c6bc11ee`. Separate C preflight compiled GCC ASan+UBSan; 488 frame-map indexes/packed pixels PASS. Hardware appearance unverified. |
| ⑤ | FX OFF + SEL returns to normal animation, no irreversible lock | FX OFF has priority over fixed FX photo selection; pressing SEL resets visual override and enters the regular feed. | Hardware button sequence pending. |
| ⑥ | HOME WAVE includes MULTI; MODE selects OSC1/OSC2, WET replaced by DETUNE | WAVE=4 MULTI; HOME K2 MODE changes active edit oscillator, K3 modifies DETUNE; EDIT page switches oscillator waveform/level/octave. OSC2 is audio-enabled **only** when MULTI selected. MI14 saved-state migration supports MI13. | Actual two-voice detuned sound / CPU / old-save cross-version behavior unverified. |
| ⑦ | **Integrated animation in NORMAL SEL playback**, not FX-only | SEL AUTO rotates all currently integrated sources: 6 user-supplied contact-sheet banks + original EFFECTS + EUREKA, on note attacks. FX preview independent. | **Partial.** Prior MIYAKO TWILIGHT, KNOWING, CLOUDS-DOT, other Android/EP-sampler banks have not been stored. No claim of complete all-source MIYAKO integration. |
| ⑧ | REC is **OVERDUB** (layering), not overwrite. Long REC=clear | REC on nonempty clip retains PCM and additively layers live post-FX bus; monitor plays old PCM before the next sample write. Re-press stops take, preserved loop plays. Long REC ~700ms queues CLEAR on audio callback. Existing regression updated. | FM-1 loop boundary/click, multi-layer saturation and control latency still need physical verification. |

### Flash budget before final target link

EFFECTS 144 frames: 62,592 bytes; EUREKA 576: 250,368 bytes;
user 488 frame VQ: 92,928 bytes. Total compressed media: **405,888 bytes**
of 581,564-byte FM-1 application slot. Remaining **175,676 bytes** must hold
program, other static data and packaging. This is an upper-bound accounting,
not evidence of a successful JieLi link. It does not justify removing original
images or writing a custom storage implementation without approval.

### Independent prebuild evidence and limits

- Existing PHOTO VQ representation was executed in an **isolated native C
  preflight** under GCC `-Wall -Wextra -Werror -fsanitize=address,undefined`.
  The packed payload SHA-256 matched the user tile source; 488 48×48 source
  frames, each with 72 map entries and 1152 packed output bytes, decoded
  within bounds. Array bytes FNV32 `df1d1580` matched decoded payload.
- C/H source bracket/comment/string structural scan passed on 13 files;
  static source references, selected FX controls, REC button dispatch,
  save-state migration, and manual-only CircleCI configuration were checked.
- No original repo complete native compilation / target JieLi
  compilation/link / firmware packaging / real FM-1 audio, UI or performance
  measurements in this turn. **Do not call any item fully tested** until
  matching actual-source prebuild and a user-authorized build/physical test.

## One-to-one mapping of eight original requirements

| ID | User requirement | Observed source status at baseline | Acceptance conditions |
|--|--|--|--|
| ① | DREAMY and EUREKA animation | EUREKA 576 VQ frames present; DREAMY auto-uses user portrait tiles, NOT original Dreamy character/atlas motion | Both original modes have visible, expressive, sound-reactive motion. Original user imagery, not AI-generated substitutions. |
| ② | SEL animation same aspect as original 720×720 | Newly added 488 tiles are 1:1 (32×32); EFFECTS/EUREKA are 128×78 | Every original square asset displayed square on 240×240. Letterbox, never stretch; track original non-square separately. |
| ③ | Pitched audio from DREAMY/EUREKA | Simplified two-grain DSP; pitch ratio absent | Audible independent upward/downward PITCH with original FX enabled and no freeze/underrun. |
| ④ | Four-plus parameters on DREAMY/EUREKA | Currently mode, wet, GRAIN/SPACE or TIME/HAZE; only two effect-specific parameters stored | At least four **actual DSP parameters** with parameter pages, readable values and physical knobs. No dummy controls. |
| ⑤ | REC re-press overdubs? NO: replace/overwrite; hold CLEAR | New REC starts fresh take. 700ms hold queues audio-thread CLEAR | Confirm REC short press starts/stops/replaces, long press clears, no accidental simultaneous record or duplicated toggle. |
| ⑥ | Loop head attack click | up to 128-sample overlap crossfade implemented | Smooth loop wrap at multiple pitch ratios/IN-OUT positions and no out-of-range reads. |
| ⑦ | Audio-reactive glitches/yellow DOT/tile splits | Native pixel operations exist, FX-specific VFX, not complete original MIYAKO VFX | Visible on both FX preview and SEL, stable at idle, bounded drawing work and no memory overflow. |
| ⑧ | Independent multi-oscillator, 2 allowed | Primary wave + sub sine fixed octave, no independent OSC2 | Add oscillator2 wave/pitch/level independent of OSC1; retain existing SUB and CRUSH, polyphonic voice isolation, deterministic handling on note/arp/panic. |

## Separate asset bank work (not confused with requirement #①)

Current user-supplied six sheets: 488 images in `mi_tile_atlas.h`; presently lossy 32×32 1-bit. 48×48/4-grey 21,664-byte format prepared separately, NOT shipped. Original MIYAKO scene, Knowing, Clouds DOT, Twilight, EP-sampler video banks still not integrated. Every bank needs byte count, host playback and capacity evidence BEFORE promising all-bank integration.

## Change order and invariants

1. State and hardware controls: OSC2 parameters and controls/page, no conflicting button/encoder assignment. Backward-compatible saved-state strategy; reject corrupt old stored structs safely.
2. DSP: OSC2 independent phase and source waveform, predictable mix gain. Keep old voice, SUB, CRUSH, four-voice polyphony, ARP, MIDI.
3. DREAMY/EUREKA parameter-page controls (four+ genuine params), sound implementation including pitch, no placeholder labels.
4. Visual/asset improvements: fixed source-to-geometry mapper, actual original Dreamy motion, improved supplied atlas quality, more banks. Do not silently drop or invent frames.
5. Offline verification **before** an approved build: source bounds and C arithmetic, native test harness, platform symbol audit, control graph, asset byte accounting, upstream compiler + linker if available. If target compiler is not available, clearly mark it unverified — CI is not the place to discover basic compile errors.
6. User alone authorizes CircleCI build (`run_build=true`); a test script update does not count as proof of functionality.

## Three review passes (performed separately, each must produce evidence)

- Review A: source contract (struct storage, parameter ownership, mode transitions, no conflict with FX A/B, SELECT visual bank, REC long press).
- Review B: DSP (phase limits, FX buffer validity, dry/wet, loop wrap, no startup clicks).
- Review C: compiler and presentation (FM-1 240×240 clipping/bands, flash/RAM budgets, actual JieLi compile/link and packaged FWSC).
