#!/usr/bin/env python3
"""Static lower bounds for approved EFFECTS -> POCKET AMP audio/visual buffers.

POCKET AMP-specific EUREKA capture length: user-approved 3.0 seconds on 2026-11-02.
Original approved Android EUREKA remains 10.2 seconds and is unchanged.

This is NOT a firmware builder and does NOT establish device SDRAM availability.
"""
import argparse
import json
import math


def budget(rate: int) -> dict:
    if not 8000 <= rate <= 192000:
        raise ValueError("sample rate must be in 8000..192000 Hz")
    eureka_frames = max(4096, math.ceil(rate * 3.0))
    dreamy_loop_frames = max(4096, round(rate * 2.5))
    dreamy_delay_frames = max(4096, round(rate * 2.0))
    return {
        "sample_rate_hz": rate,
        "eureka_capture_seconds": 3.0,
        "eureka_main_buffers_bytes": eureka_frames * 2 * (4 + 4),
        "dreamy_main_buffers_bytes": (dreamy_loop_frames + dreamy_delay_frames) * 2 * 4,
        "clouds_large_small_bytes": 118784 + 65408,
        "eureka_visual_raw_240_rgb565_bytes": 11 * 36 * 240 * 240 * 2,
        "eureka_visual_frames": 11 * 36,
        "memory_exclusions": [
            "firmware, OS, display stacks, other heap/stack and DMA",
            "Clouds resampling/ring and DSP objects",
            "Eureka extra warble/reverb, Dreamy reverb",
            "DRY routing buffers and display decoding buffers",
        ],
    }


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--rate", type=int, default=44100)
    p.add_argument("--onchip-kib", type=int, default=578,
                   help="AC79 family SRAM size; NOT measured free RAM")
    p.add_argument("--external-kib", type=int, default=0,
                   help="hypothetical external RAM; 0 means unconfirmed")
    p.add_argument("--flash-kib", type=int, default=1024,
                   help="reported SPI NOR total, NOT free firmware space")
    p.add_argument("--json", action="store_true")
    p.add_argument("--self-test", action="store_true")
    args = p.parse_args()

    if args.self_test:
        b = budget(44100)
        assert b["eureka_main_buffers_bytes"] == 2116800, b
        assert b["dreamy_main_buffers_bytes"] == 1587600, b
        assert b["clouds_large_small_bytes"] == 184192, b
        assert b["eureka_visual_raw_240_rgb565_bytes"] == 45619200, b
        print("PASS: memory formulas and reference values")
        return
    if min(args.onchip_kib, args.external_kib, args.flash_kib) < 0:
        p.error("hardware size arguments must be >= 0")
    b = budget(args.rate)
    available = (args.onchip_kib + args.external_kib) * 1024
    fits = {
        name: b[name] <= available
        for name in ("eureka_main_buffers_bytes",
                     "dreamy_main_buffers_bytes", "clouds_large_small_bytes")
    }
    result = {
        "hardware_assumptions_not_verified": {
            "onchip_ram_kib": args.onchip_kib,
            "external_ram_kib": args.external_kib,
            "total_flash_kib": args.flash_kib,
        },
        "lower_bounds": b,
        "fits_raw_main_buffers_in_assumed_total_ram_only": fits,
        "notes": "Fits are necessary NOT sufficient. Main buffers cannot use SPI flash as RAM.",
    }
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(f"RATE {args.rate} Hz | assumed aggregate RAM {available} B (NOT verified on device)")
        for key, size in b.items():
            if key.endswith("_bytes"):
                print(f"{key}: {size:,} B ({size/1024:.1f} KiB)")
        for name, fit in fits.items():
            print(f"{name}: {'LOWER-BOUND FIT' if fit else 'TOO LARGE'}")
        print(result["notes"])


if __name__ == "__main__":
    main()
