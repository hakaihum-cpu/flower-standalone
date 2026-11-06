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
# CircleCI expands <<...>> even in YAML block-scalar scripts. Only this
# valid workflow parameter marker may remain; unescaped shell here-documents
# cause configuration failure before ANY job or project test can execute.
assert "<<" not in ci.replace("<< pipeline.parameters.run_build >>", ""), (
    "CircleCI 2.1 unclosed << tag: replace shell here-documents with printf"
)
assert "printf '%s\\n'" in ci, "README must use a CircleCI-safe printf"
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
# fm1-base tools/build.py uses sys.executable for gen_font.py.
# Pillow MUST already be installed in the SAME interpreter before first --host.
assert ci.index("Install Python font and media dependencies") < ci.index("Host-compile real MIYAKO app")
assert ci.index("Pillow==10.4.0") < ci.index("Host-compile real MIYAKO app")
assert ci.count("fm1/native/words.tsv") == 1, (
    "fm1-base gen_words.py requires words.tsv copied into app directory"
)
words = (root / "fm1/native/words.tsv").read_text(encoding="ascii")
assert any(x.startswith("# url: https://") and x.endswith("/words/")
           for x in words.splitlines()), "gen_words.py requires a words URL"
assert "from PIL import Image, ImageDraw, ImageFont" in ci
# fm1-base defaults to Barlow TTF with OpenType tnum features, which fails
# in CircleCI Pillow builds without libraqm. Use tested bundled BDF fonts.
assert 'X0X_FONTSET: "terminus"' in ci, (
    "FM-1 font generator must not require unavailable libraqm"
)
assert "assets/fonts/ter-u12n.bdf" in ci and "assets/fonts/ter-u16b.bdf" in ci
assert ci.index("tools/gen_font.py") < ci.index("tools/build.py /tmp/fm1-base/miyako --host")
assert "test -s /tmp/mi_font_preflight.h" in ci
assert "python3 /tmp/fm1-base/tools/build.py" not in ci
assert ci.count("/tmp/mi-video-venv/bin/python /tmp/fm1-base/tools/build.py") >= 3
assert ci.index("Host-compile real MIYAKO app") < ci.index("Reconstruct all 144 original")
assert ci.index("Reconstruct all 144 original") < ci.index("Compile FM-1 native platform host")
assert "grep -q 'MI_EFFECTS_MEDIA_PREFLIGHT_STUB'" in ci
assert "mi_fx_dsp.c fm1/native/test_mi_audio.c" in ci
assert "MIYAKO_HOME.png" in ci and "MIYAKO_FX.png" in ci
assert "MIYAKO_EFFECTS_VIDEO.png" in ci
assert "--base /tmp/fm1-base --product FM-1_9100" in ci
print("PASS: manual no-EUREKA FM-1 firmware pipeline with original EFFECTS video and native FX")
