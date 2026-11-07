# FM-1 MIYAKO Phase 4 — requirements, source truth, and gates

Date: 2026-11-07. Golden baseline: **branch** `feature/fm1-miyako-recorder-eureka-fx-20261107`; prior latest confirmed `855d01c3a3a670b2cb0fe1a87ac0f08b0a012b48`.
No changes to `main`. No autonomous CircleCI/GitHub Actions builds. This inventory is a work ledger, not a statement of completed hardware validation.

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
