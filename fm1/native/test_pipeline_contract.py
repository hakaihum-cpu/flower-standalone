#!/usr/bin/env python3
"""CI contract: fail closed rather than accidentally shipping legacy FM-1 code."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
ci = (root / ".circleci/config.yml").read_text(encoding="utf-8")
required = (
    "version: 2.1",
    "default: false",
    "when: << pipeline.parameters.run_build >>",
    "only: feature/fm1-miyako-no-eureka-20261106",
    "fm1_native_build",
    "MIYAKO-FM1-NATIVE-DEV.fwsc",
    "MI_EFFECTS_ASSET_COUNT 144u",
    "fm1/native/verify_standalone.py",
    "fm1/native/test_mi_control.c",
    "fm1/native/test_mi_audio.c",
    "fm1/native/test_mi_video.c",
    "mi_video.c",
    "test_mi_fx_dsp.c",
    "mi_fx_dsp.c",
    "tools/compile_only_media_stub.py",
    "tools/check_host_wav.py",
    "tools/verify_fwsc.py",
    "MIYAKO_FX_PREVIEW.wav",
)
for item in required:
    assert item in ci, f"missing mandatory CircleCI gate: {item}"
for forbidden in (
    "eatyourgreensstudio-dotcom",
    "KARMA_PIN",
    "fm1/karma/",
    "fm1-karma",
    "build/felucca.fwsc",
):
    assert forbidden not in ci, f"legacy implementation in native CI: {forbidden}"
assert ci.count("workflows:") == 1
assert ci.count("when: << pipeline.parameters.run_build >>") == 1
assert re.search(r"^\s+only: feature/fm1-miyako-no-eureka-20261106\s*$", ci, re.M)
assert "/tmp/mi-video-venv/bin/python /tmp/fm1-base/tools/build.py /tmp/fm1-base/miyako" in ci
assert "test_media_crc.c" in ci
assert "pack_effects_vq_candidate.py" in ci
assert "pack_eureka_vq_candidate.py" not in ci
assert "test -s /tmp/fm1-base/miyako/build/miyako.fwsc" in ci
assert "scripts/materialize_effects_video_pack.py" in ci
assert "scripts/materialize_miyako_eureka_video.py" not in ci
assert "DREAMY and the source-accurate multi-FX" in ci
assert "MI_EUREKA_ASSET_COUNT" not in ci
assert ci.index("Host-compile real MIYAKO app") < ci.index("Reconstruct all 144 original")
assert ci.index("Reconstruct all 144 original") < ci.index("Compile FM-1 native platform host")
assert "grep -q 'MI_EFFECTS_MEDIA_PREFLIGHT_STUB'" in ci
assert "mi_fx_dsp.c fm1/native/test_mi_audio.c" in ci
assert "MIYAKO_HOME.png" in ci and "MIYAKO_FX.png" in ci
assert "MIYAKO_EFFECTS_VIDEO.png" in ci
assert "--base /tmp/fm1-base --product FM-1_9100" in ci
print("PASS: manual no-EUREKA FM-1 firmware pipeline with original EFFECTS video and native FX")
