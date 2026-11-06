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
    "only: feature/fm1-miyako-recorder-eureka-fx-20261107",
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
    "MIYAKO_ENV.png",
    "MIYAKO_LFO.png",
    "MIYAKO_ARP.png",
    "MI_EUREKA_ASSET_COUNT 576u",
    "mi_recorder.c",
    "test_mi_recorder.c",
    "MIYAKO_RECORDER.png",
    "MIYAKO_EUREKA_VIDEO.png",
    "MIYAKO_FX_DUAL.png",
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
# #250 was a Permission denied exit 126 on an unexecutable ./build.sh.
# Call the already-tested upstream build.py with a Python interpreter instead.
assert "./build.sh" not in ci, "regression #250: executable build.sh permission failure"
# #251 had dangling KARMA/ACID/PRISM/PHYS engine references after partial deletion.
native_audit = (root / "fm1/native/verify_standalone.py").read_text(encoding="utf-8")
for retired in ("ENG_PHYS", "ENG_ELEM", "ENG_PRISM", "EM_BOUNCE",
                "EM_PICK", "PRM_FOLD", "PRM_WAVE", "PRM_HARM"):
    assert retired in native_audit, f"regression #251: guard missing {retired}"
# CircleCI expands <<...>> even in YAML block-scalar scripts. Only this
# valid workflow parameter marker may remain; unescaped shell here-documents
# cause configuration failure before ANY job or project test can execute.
assert "<<" not in ci.replace("<< pipeline.parameters.run_build >>", ""), (
    "CircleCI 2.1 unclosed << tag: replace shell here-documents with printf"
)
assert "printf '%s\\n'" in ci, "README must use a CircleCI-safe printf"
assert re.search(r"^\s+only: feature/fm1-miyako-recorder-eureka-fx-20261107\s*$", ci, re.M)
assert "/tmp/mi-video-venv/bin/python /tmp/fm1-base/tools/build.py /tmp/fm1-base/miyako" in ci
assert "test_media_crc.c" in ci
assert "pack_effects_vq_candidate.py" in ci
assert "pack_eureka_vq_candidate.py" in ci
assert "test -s /tmp/fm1-base/miyako/build/miyako.fwsc" in ci
assert "scripts/materialize_effects_video_pack.py" in ci
assert "scripts/materialize_miyako_eureka_video.py" in ci
assert "two serial FX with conflict guards" in ci
assert "MI_EUREKA_ASSET_COUNT 576u" in ci
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
assert "mi_fx_dsp.c fm1/native/mi_recorder.c fm1/native/test_mi_audio.c" in ci
assert "MIYAKO_HOME.png" in ci and "MIYAKO_FX.png" in ci
assert "MIYAKO_EFFECTS_VIDEO.png" in ci
# Approved on 2026-11-06: no 26dB (or other arbitrary numeric) image
# quality gate may block a trial .fwsc. PSNR is measured and reported only.
vq = (root / "fm1/native/tools/pack_effects_vq_candidate.py").read_text(encoding="utf-8")
assert "--min-psnr-db" not in vq
assert "min(worst) <" not in vq
assert "mandatory" not in vq.lower()
assert "raise SystemExit" in vq  # frame-count / source integrity still fail closed
assert "CANDIDATE: RGB565 16 colours" in vq
assert "decoded_crc32 = zlib.crc32(pixels, decoded_crc32)" in vq
assert "write_candidate(a.output_header, palettes, books, maps, decoded_crc32)" in vq
assert "PSNR is a non-blocking visual quality metric" in vq
eureka_vq = (root / "fm1/native/tools/pack_eureka_vq_candidate.py").read_text(encoding="utf-8")
assert "--min-psnr-db" not in eureka_vq
assert "min(worst) <" not in eureka_vq
assert "MI_EUREKA_ASSET_COUNT 576u" in eureka_vq
assert "decoded_crc32 = zlib.crc32(pixels, decoded_crc32)" in eureka_vq
assert ci.index("Reconstruct all 144 original MIYAKO EFFECTS") < ci.index(
    "Reconstruct all 576 original MIYAKO EUREKA")
assert ci.index("Reconstruct all 576 original MIYAKO EUREKA") < ci.index(
    "Verify all 720 EFFECTS and EUREKA")
# Target-only validation MUST precede the expensive user-video VQ stage.
assert ci.index("Install pinned JieLi compiler") < ci.index(
    "Verify freestanding MIYAKO C with the actual JieLi target compiler")
assert ci.index("Verify freestanding MIYAKO C with the actual JieLi target compiler") < ci.index(
    "Reconstruct all 144 original MIYAKO EFFECTS source frames")
assert "for unit in mi_fx_dsp.c mi_audio.c mi_recorder.c mi_control.c mi_video.c" in ci
assert "fsyntax-only" in ci and "fno-builtin" in ci
assert "fm1/native/mi_memory.h" in ci
# JieLi's pi32v2 toolchain doesn't have hosted libc string.h; scanning
# every target source+header makes the exact observed error a stage-one fail.
import re as _re
for target in list((root / "fm1/native").glob("mi_*.c")) + [
    root / "fm1/native/app.c",
] + list((root / "fm1/native").glob("*.h")):
    body = target.read_text(encoding="utf-8")
    assert not _re.search(r'#\s*include\s*<string\.h>', body), (
        f"target imports missing bare-metal string.h: {target.name}"
    )
    if target.suffix == ".c":
        assert not _re.search(r'\b(?:memset|strcmp)\s*\(', body), (
            f"target still calls unavailable hosted libc: {target.name}"
        )
assert (root / "fm1/native/mi_memory.h").is_file()
# The FWSC package identity and app-visible release/version text are
# separate fields. Neither is permitted to be empty or implicit "DEV".
manifest = (root / "fm1/native/fm1.toml").read_text(encoding="utf-8")
app_header = (root / "fm1/native/app.h").read_text(encoding="utf-8")
assert 'id_prefix = "FM-1_9"' in manifest
assert '#define APP_VERSION "0.3.0-DEV"' in app_header
assert 'APP_TAGLINE "NATIVE TRIAL"' in app_header
assert "--product FM-1_933" in ci
# Keep package header marker and running firmware FELUCCA_ID in sync by
# patching the pinned original builder BEFORE ANY compilation step.
id_patch = (root / "fm1/native/tools/set_miyako_product_id.py").read_text(encoding="utf-8")
assert 'TARGET = "FM-1_933"' in id_patch
assert 'OLD = \'    PRODUCT = prefix + "00"\'' in id_patch
assert 'NEW = \'    PRODUCT = prefix + "33"' in id_patch
# Check the package product ID against the actual pinned build patch,
# rather than maintaining two contradictory hard-coded CI expectations.
actual_product = re.search(r'^TARGET = "(FM-1_[0-9]+)"', id_patch, re.M)
assert actual_product, "FM-1 product version must exist in the build patch"
expected_product = actual_product.group(1)
assert len(expected_product.split("_")[1]) == 3, (
    "M-UPGRADE accepts only three numeric product/version digits"
)
assert "--product " + expected_product in ci
# Prevent repeats of the stale assertion that stopped the prior pipeline.
for n, line in enumerate(Path(__file__).read_text(encoding="utf-8").splitlines(), 1):
    if line.strip().startswith("assert ") and "--product FM-1_" in line and " in ci" in line:
        assert expected_product in line, (
            f"conflicting product-ID assertion at line {n}: {line.strip()}"
        )
assert 'set_miyako_product_id.py --base /tmp/fm1-base' in ci
assert ci.index('set_miyako_product_id.py') < ci.index('tools/build.py /tmp/fm1-base/miyako --host')
assert "FM-1_9100" not in ci
assert (root / "fm1/native/tools/verify_fwsc.py").read_text().count('FM-1_933') >= 1

# Root issue from the current build log: fm1-base's own mmio_check()
# correctly rejects a volatile pointer cast outside the BSP, even though
# generic C compilation and our previous custom checks happily passed it.
mem = (root / "fm1/native/mi_memory.h").read_text(encoding="utf-8")
assert "volatile unsigned char *" not in mem
assert "unsigned char *p = (unsigned char *)destination;" in mem
# Auditing the ACTUAL pinned fm1-base algorithm (not our imitation) is now
# required before video VQ. The whole target link must also pass BEFORE VQ
# using a compiler-only header that must be discarded, not shipped.
assert "run_upstream_platform_checks.py" in ci
assert "fm1build.mmio_check()" in (root /
    "fm1/native/tools/run_upstream_platform_checks.py").read_text()
assert ci.index("run_upstream_platform_checks.py") < ci.index(
    "Reconstruct all 144 original MIYAKO EFFECTS source frames")
assert ci.index("Exercise real FM-1 target compile") < ci.index(
    "Reconstruct all 144 original MIYAKO EFFECTS source frames")
assert "rm -rf /tmp/fm1-base/miyako/build" in ci
assert ci.index("rm -rf /tmp/fm1-base/miyako/build") < ci.index(
    "Reconstruct all 144 original MIYAKO EFFECTS source frames")
# Feature coverage: reject a UI-only placebo update to the proven baseline.
ui = (root / "fm1/native/app.c").read_text(encoding="utf-8")
audio = (root / "fm1/native/mi_audio.c").read_text(encoding="utf-8")
fxcode = (root / "fm1/native/mi_fx_dsp.c").read_text(encoding="utf-8")
controls = (root / "fm1/native/mi_control.c").read_text(encoding="utf-8")
assert "static void mi_media_draw_half(unsigned half)" in ui
assert "if(state.view==MI_VIDEO)" in ui and "mi_media_draw_half(1)" in ui
assert "MIYAKO FM-1 / NATIVE PREVIEW" not in ui
assert "mi_wave_name(state.wave)" in ui
assert "mi_graph_env();" in ui and "mi_graph_lfo();" in ui
assert "mi_graph_arp();" in ui
assert "mi_audio_input_on(&audio,&state" in ui
assert "mi_audio_input_off(&audio,&state" in ui
assert "arp_tick(a,s);" in audio
assert "held_total" in audio and "release_samples" in audio
assert "lfo_value" in audio
assert "mode==MI_FX_DREAMY" in fxcode
assert "mi_wave_name" in controls
assert "case MI_VIDEO:" in controls
# All non-REEL FX, including DREAMY, are routed through one generic
# native DSP dispatcher. Do not regress to checking a non-existent per-mode
# branch: that stale assertion caused #263 to fail before any C tests ran.
assert "mi_fx_dsp_process(&native_fx,mode,dry,a->sample_rate" in audio
assert "mode!=MI_FX_OFF&&mode<MI_FX_COUNT" in audio
# The DSP and its host test must both exercise DREAMY, not just name a menu.
assert "mode==MI_FX_DREAMY" in fxcode
fx_test = (root / "fm1/native/test_mi_fx_dsp.c").read_text(encoding="utf-8")
assert "mi_fx_dsp_process(&p,MI_FX_DREAMY" in fx_test
assert "assert(peak>0.01f)" in fx_test
own_lines = Path(__file__).read_text(encoding="utf-8").splitlines()
assert len(own_lines)>100 and own_lines[-1].startswith("print(")
recorder = (root / "fm1/native/mi_recorder.c").read_text(encoding="utf-8")
assert "MI_REC_CAPACITY" in recorder and "mi_rec_tick" in recorder
assert "mi_audio_rec_request(&audio,1u)" in ui
assert "mi_audio_rec_request(&audio,2u)" in ui
assert "mi_graph_recorder()" in ui
assert "mi_eureka_vq_tilebook" in ui
assert "MI_FX_EUREKA" in audio and "mi_fx_dsp_reset_shared" in audio
assert "mi_fx_pair_supported" in audio
assert "s->fx2" in controls and "s->fx_slot" in controls
assert "fx_stage(a,s,second" in audio
assert "RECORDING" in ui and "LOOP PLAY" in ui
assert "mi_recorder.c" in (root / "fm1/native/fm1.toml").read_text()
print("PASS: manual Phase 3 REC + EUREKA 2sec/576 frames + guarded dual-FX")
