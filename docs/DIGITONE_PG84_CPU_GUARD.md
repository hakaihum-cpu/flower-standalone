# PG84 SOURCE-ONLY recovery experiment: EUREKA audio overload fail-safe

Hardware results: PG80/PG81/PG82 EUREKA audio selection slows or freezes the original Digitone. Reel and Bitcrusher are also slow. This is a **real-device failure**, not a build issue. Root cause is not verified.

The original Elekloader DigiReel/FX toolkit uses DMA timer 0 (DTCN0) and a per-block CPU guard to detect audio time overruns. PG82's custom FX rack lacked any such guard.

- For REEL/EUREKA/CRUSH only (not PG79's original Granular), record DTCN0 at the render callback start/end.
- If a single insert render consumes >=80% of a valid block interval, or >=33% for 6 consecutive callbacks, or >400000 timer counts, the owner is switched off immediately by a volatile flag write. No memory is allocated or cleared in the audio IRQ.
- The native parameter page displays FX CPU LIMIT when this emergency bypass triggers.
- Only explicit selection from TRACK's Mod Menu resets the fault, never an automatic retry.
- The one-shot guard does not change the original EUREKA video, 576 user frames, yellow padding, any audio DSP algorithms or the original Digitone FM/MUTE controls.
- **This is only a guard:** it could stop hangs due to repeated CPU overruns but cannot prove the source of the freezes or make EUREKA's DSP fast enough. If it trips, the FX will be silent while stock audio continues. Profiling remains necessary.
- No SYX has been built from this branch. Build remains manual-only and requires explicit authorization.
