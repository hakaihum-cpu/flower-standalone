# EFFECTS -> M-VAVE POCKET AMP: feasibility gate (2026-11-02)

## Authority and non-goals

- Source baseline: `baseline/effects-mood-mkii-master-20261031` (read-only).
- Target: M-VAVE POCKET AMP, **not** FM-1 or TANK-G.
- Only three audio modes: EUREKA, DREAMY, CLOUDS, plus the EFFECTS user-supplied animations for these modes.
- User explicitly authorized **EUREKA 3.0 seconds maximum recording buffer** on 2026-11-02. DREAMY and CLOUDS are not shortened. Retain all other algorithm settings, including 2-channel processing where possible. This is a POCKET AMP port-only change, not an alteration to Android EFFECTS Golden Baseline.
- **No MIDI features** and no CHORD, MOOD, VONGON, Pure Data or MULTI port.
- This branch is an independent **investigation/porting branch**, not a firmware build or approved master.
- Do not alter main, approved baseline, CI files, audio routes or generate/flash a `.fwsc` until the hardware gates below are met.
- No synthetic/substitute image assets. Do not simplify/reduce algorithms, buffer duration, modes or visuals without user approval.

## Directly inspected current EFFECTS source

Source relative to the named baseline:
- `Source/HazeProcessor.h`: EUREKA's two `lanes`, each with `capacity = ceil(sampleRate*10.2)`, `float` sample buffer and `uint32_t` generation array. `clear()` invalidates the generation rather than memsetting audio memory. Additional warble/reverb state.
- `Source/PluginProcessor.cpp`: DREAMY `dreamyBuffer` stereo 2.5 s and `dreamyDelayBuffer` stereo 2.0 s, float32; sample-accurate processing; EUREKA uses `hazeProcessor.renderBlock`, DREAMY uses `processDreamy`, CLOUDS uses `cloudsEngine.process`.
- `Source/EffectsCloudsEngine.h`: static `largeBuffer[118784]` and `smallBuffer[65408]`, plus resampling/ring buffers, real original Clouds DSP via `Source/clouds/`. It uses 32 kHz/32-frame internal blocks.
- `Source/PluginEditor.cpp` and `Source/VideoInteraction.h`: EUREKA's 11 x 36 frame clips, audio-onset/random-sustain cuts, rewinds and input-reactive up-to-4.5 fps cadence; DREAMY audio-reactive image selection; CLOUDS onset and pitch-dependent tile mosaic; UI timer 30 Hz. Video/DOT must not be reduced to a fixed timer slideshow.
- `Resources/eureka_*_frame_chunks`, `Resources/frame_chunks`: original user-supplied images; existing repo stores old and replacement banks. Do not assume the sum of all historical chunks is the runtime image pack size.

## Hardware evidence, with provenance and limits

1. Official AC79 series SDK overview: WL82/AC791N-family max dual-core DSP 320 MHz, on-chip **578 KiB SRAM**; **2 or 8 MiB external SDRAM** supported on *selected chip packages*, not guaranteed on POCKET AMP. https://doc.zh-jieli.com/SDKWiki/master/Jieli-Tech/fw-AC79_AIoT_SDK/1-overview/1.1-sdk-overview.html
2. POCKET AMP firmware updater's **source code** identifies chip `AC791N`, packages named `Pocket AMP_105` and 1 MiB-limit flash entry in `.fwsc` parser; documented official update via USB-MIDI, matching FM-1 update loader. This is an update transport, **not** a driver SDK. https://github.com/vitoramaral10/pocket-amp-upgrader
3. M-VAVE vendor download center publishes POCKET AMP V105 dated 2026-10-13. Source URL: https://www.m-vave.com/download
4. A first-person POCKET AMP recovery report (V07 unit, *not* confirmed V105) reports SPI0 NOR `JEDEC 0x856014`, **1 MiB flash**, twice-verified entire device dump and boot-sector recovery via WL82 UBOOT1.00. This is a **single-device report**, not proof for all production revisions. https://www.reddit.com/r/guitarpedals/comments/1w1zem5/mvave_pocket_amp_black_screen_wl82_uboot100_i/
5. M-VAVE product specs: quarter-inch instrument input and headphones; device control surface with 1.54-inch screen and push/button/knob controls. *Real ADC/DAC mapping, RAM chips/pins, flash partitions and LCD controller have not been observed on a POCKET AMP PCB.*
6. Public `m-vave/fm1_open` sources expose FM-1 I2S/DMA **output**, but `app/board/fm1_audio.h` explicitly says *FM-1 has no connected analogue input*. FM-1 `app/board/audio.c` is **not** a usable POCKET AMP input driver. https://github.com/m-vave/fm1_open/blob/main/app/board/fm1_audio.h

## Exact first-pass audio buffer sizes (44,100 Hz, float32 unless noted)

- EUREKA **port target**: `ceil(44100*3.0)*2*(4+4)` = **2,116,800 bytes** for sample and generation arrays alone; excludes warble, reverbs and firmware. Prior approved Android reference: `ceil(44100*10.2)*2*(4+4)` = **7,197,120 bytes**. Reduction is 5,080,320 bytes (70.59%).
- DREAMY: `(ceil(44100*2.5)+ceil(44100*2.0))*2*4` = **1,587,600 bytes**, excluding reverb state.
- CLOUDS: `118784+65408=184192 bytes` = **179.9 KiB** just for the two fixed DSP arrays; excludes the ring and other engine allocations.
- EUREKA and DREAMY allocations need **not** coexist if the implementation safely allocates only the selected mode, but simply sharing the current allocations does *not* make 578 KiB SRAM sufficient.
- Compressed or reduced-precision representation can be studied, but cannot be counted as equivalent until null, full-scale, transients, feedback, repeated clear, interpolation, and clicks are regression-tested against the approved reference. In particular EUREKA's `writeGeneration` array is semantically necessary; dropping it without a replacement invalidates CLEAR.
- 1 MiB SPI flash is **code/data/storage**, not RAM. It must not be counted as writable audio buffer capacity; audio data streamed/rewritten to flash at realtime rates would encounter endurance and latency issues.
- A 240x240 RGB565 frame is 115,200 bytes raw; 396 frames are **45,619,200 bytes** raw. Source JPEG sizes do not predict compressed 240x240 totals. The design needs a measured compression/transport plan and actual display throughput.
- Use `ports/pocket_amp/memory_budget.py` for repeatable numeric checks with explicit user-adjustable hardware assumptions; **no default simulated hardware number is evidence of installed SDRAM**.

## Hard blocking gates (all unresolved as of this document)

| Gate | Need to establish | Current evidence | Status |
|---|---|---|---|
| H1 RAM | POCKET AMP physical RAM/SDRAM component ID, size, address and actual free heap/stack | Only family SRAM and generic optional SDRAM known | BLOCKED |
| H2 ADC | Guitar input ADC or codec register route; sample format, measured audio callback | FM-1 source describes no analog input | BLOCKED |
| H3 DAC | POCKET AMP headphone/line output codec, output DMA and clocks | FM-1 output board not transferable without validation | BLOCKED |
| H4 display | LCD controller, SPI/pins, working pixel blit, decoder, effective fps while DSP runs | Screen exists; driver not known | BLOCKED |
| H5 flash | POCKET AMP *current hardware revision* boot partition map and safe independent animation storage | Single V07 1 MiB NOR report; V105 package available but **not locally parsed here** | BLOCKED |
| H6 recovery | User explicitly waived preservation of original functionality on 2026-11-02 | Factory firmware kept for research; no guarantee of restoration | NON-BLOCKING BY USER CHOICE |

## Safe implementation gate order

1. Obtain/parse official unmodified POCKET AMP V105 `.fwsc` **offline** without flashing and identify its file entries/partitions and image size. The download location is confirmed, but automated binary retrieval was unavailable in this environment on 2026-11-02. **Do not invent parser output.**
2. Obtain PCB evidence or an on-device *read-only* information dump to identify SDRAM (if present), codec and LCD; compare hardware revision. No automatic USB writes.
3. Recovery is NOT a gating condition: user explicitly accepts loss of the original firmware on 2026-11-02. Keep the provided factory image unchanged for reference, but do not postpone firmware research while trying to guarantee restoration. Read-only device inspection is still preferred whenever it provides missing hardware information.
4. Port bare-metal input -> dry output loop **first**, then assert no xrun/clipping/audio silence on target. No I/O code is present/validated for POCKET AMP.
5. Preserve the existing algorithm and selected-mode buffers. Move Clouds to target runtime and test against offline reference; then Dreamy and Eureka. Never silently shorten time buffers.
6. Convert user-owned animation assets to an explicitly verified 240x240 pack *without generating substitute images*; keep input-driven event logic, deterministic test clips, random-cut distribution and playback/rewind semantics. Only proceed after flash/storage and LCD gates.
7. Run native audio & visual tests 3 times per accepted target build; do not reuse Android/CircleCI build paths for unsupported targets. Any target build workflow requires a separately established, reviewed method; do not run a build without user authorization.

## Assessed status

The **target hardware compatibility is not yet demonstrated**. Loss of original firmware operation is explicitly acceptable to the user (2026-11-02), so restoration is NOT a project completion or prototype gating requirement. Do not suggest recovery is guaranteed; retain all safety measures unrelated to preserving stock firmware. User-approved 3-second EUREKA recording is now a PORT-ONLY scope change. The main memory arrays alone still need 2,116,800 bytes at 44.1 kHz, exceeding the AC79 family 578 KiB on-chip SRAM. The FM-1 SDK and an update tool do not establish an executable POCKET AMP effects image. No firmware binary, image assets, executable or custom firmware has been produced on this branch. The deliverables so far are a verified source inventory, documented platform blockers, repeatable memory accounting, and a no-JUCE/no-MIDI EUREKA **frame-event scheduler** (not image decoding, audio DSP or target display driver) in `eureka_visual_clock.h`. The scheduler is validated by `eureka_visual_clock_test.cpp`: deterministic replay, silence stays still, cut trigger, frame bounds and speed, compiled and executed three times with host g++ `-std=c++17 -Wall -Wextra -Werror -O2`, all passed on 2026-11-02. The target hardware gates remain blocking. Any future work must maintain fidelity, never present a diagnostic or incomplete binary as a finished product.


## User-provided official POCKET AMP V1.05 firmware: offline evidence (2026-11-02)

Files supplied: `Pocket AMP.fwsc`, `POCKET AMP.zip`, `M-UPGRADE.zip`. Archive EXEs/DLLs were **not executed**. Offline packet inspection on the supplied firmware established:

- `.fwsc` is **767,028 bytes**, SHA-256 `9584e81da98215e8e70ddffc28311423ceaa1a5a8a12a478dc68f4679e10902b`.
- Product ID `Pocket AMP_105`, chip `AC791N`; header and directory CRCs valid.
- Unpacked `flash.bin` **741,376 bytes**, SHA-256 `f9da501931c053d0bec0c21fa8da770c0c18d82536cf8bbf7698c62b3c1721c0`.
- `ota.bin` **19,969 bytes**, SHA-256 `ef1f2db89b011fa53e71df363f0122de0ea896618c53fb53e8da676322004541` (matches public updater pin).
- Read-only JLFS top-level entries verified with CRC: `uboot.boot` (offset 0xA0, 14,488 bytes); `isd_config.ini` (0x3938, 755 bytes); `app_dir_head` (0x4000); protected `key_mac` allocation (0xFF000, size 4096). No device-specific secret contents were published.
- Read-only `isd_config.ini` byte inspection: `ENABLE_SDRAM=0`, `SDRAM_SIZE=0`, `SDRAM_MODE=0`, `SDRAM_CONFIG_VAL=0`. This proves the **supplied V1.05 firmware configuration** has external SDRAM disabled, **not** that every POCKET AMP board lacks a RAM chip.
- Flash image header reports nominal flash size `0xFF000` (1044480 bytes usable image area in that header) and cannot be treated as available image storage. Full physical flash capacity still requires per-device identification.
- None of this proves execution of arbitrary firmware, runtime free SRAM, audio ADC registers or LCD blitting; hardware discovery gates H1-H5 remain unresolved. H6 recovery is now non-blocking by explicit user decision.


## Recovery waiver and audio-memory engineering (2026-11-02)

The user explicitly permits a POCKET AMP that no longer boots its original factory firmware. Do NOT block the port while seeking a guaranteed stock restore. This does NOT imply the new EUREKA/DREAMY/CLOUDS firmware may be silent, missing features or falsely reported as operational. CPU/RAM/ADC/DAC/LCD discoveries remain prerequisites for a working device.

EUREKA 3.0-second, 44.1 kHz / two-lane ring storage arithmetic (raw histories only): float32 samples plus uint32 per-sample generation = 2,116,800 B; uint16 PCM samples plus one packed validity bit per physical sample = 562,276 B (about 549.1 KiB), leaving only 29,596 B of the 578 KiB family SRAM figure **even before software/UI/stack/DSP**. So this is NOT an approved runnable target memory layout. Quantization changes have not been approved for sound quality. A safe storage test can check ring semantics, invalidation on CLEAR, TIME-dependent loop sizes, echo feedback and numerical error before selecting any compact DSP representation.


## Tested storage primitive after 3-second change (2026-11-02)

- New **non-firmware** `eureka_history_3s.h` contains two independent 3-second PCM16 circular histories and a compact per-sample validity bitmap, retaining echo-on-write, dynamic loop lengths and interpolation/clear semantics. `eureka_history_3s_test.cpp` verifies 3-second end access, stale data suppression after CLEAR, independent lanes, echo, interpolation, repeated loop-length changes and wrap. Compiled and executed 3 times with host `g++ -std=c++17 -O2 -Wall -Wextra -Werror` and 3/3 passes.
- The prototype's static storage at 44.1 kHz is **562,276 bytes**; excludes stack, run-time DSP, reverb, graphics, OS, DMA, etc. This is a working data-structure TEST, not an established memory-fit of the full device.
- PCM16 introduces quantization versus original float histories and must pass comparative audio regression before being accepted as the output implementation; this optimization is exploratory and has not been silently substituted into source EFFECTS.
- Factory firmware recovery was waived explicitly; new target firmware must still be produced and run on the actual Pocket AMP. No device flash or target firmware generation has occurred.
