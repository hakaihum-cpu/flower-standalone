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
# #275 built and verified a valid FWSC, then failed on a README quote.
# Validate all CircleCI shell snippets before costly native build work.
assert "python3 fm1/native/tools/check_ci_run_shell.py" in ci
assert ci.index("python3 fm1/native/tools/check_ci_run_shell.py") < ci.index(
    "python3 -m compileall -q fm1/native")
assert "five-sheet set's" not in ci
assert (root / "fm1/native/tools/check_ci_run_shell.py").is_file()
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
# All three host scripts must run before expensive 720-frame VQ.
preflight_name = "Install pinned JieLi compiler"
for smoke in ("host_smoke.txt", "host_fx_preview.txt", "host_eureka_preview.txt"):
    command = "fm1/native/" + smoke + " /tmp/miyako-host-preflight"
    assert command in ci, "missing early host preflight: " + smoke
    assert ci.index(command) < ci.index(preflight_name)
assert ci.index("mkdir -p /tmp/miyako-host-preflight") < ci.index(preflight_name)
for wav_check in ("check_host_wav.py", "check_eureka_wav.py"):
    name = "python3 fm1/native/tools/" + wav_check
    assert name in ci and ci.index(name) < ci.index(preflight_name), (
        "missing early audio preflight: " + wav_check
    )
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
assert 'replace_black_frames(idx, recon, ids, palette, "EFFECTS", clip)' in vq
assert "write_candidate(a.output_header, palettes, books, maps, decoded_crc32)" in vq
assert "PSNR is a non-blocking visual quality metric" in vq
eureka_vq = (root / "fm1/native/tools/pack_eureka_vq_candidate.py").read_text(encoding="utf-8")
assert "--min-psnr-db" not in eureka_vq
assert "min(worst) <" not in eureka_vq
assert "MI_EUREKA_ASSET_COUNT 576u" in eureka_vq
assert "decoded_crc32 = zlib.crc32(pixels, decoded_crc32)" in eureka_vq
assert 'replace_black_frames(idx, recon, ids, palette, "EUREKA", clip)' in eureka_vq
assert "test_black_frame_guard.py" in ci
assert "test_video_palette_natural.py" in ci
assert ci.index("test_video_palette_natural.py") < ci.index("Reconstruct all 144 original")
assert ci.index("test_black_frame_guard.py") < ci.index("Reconstruct all 144 original")
assert "visible<32u" in (root / "fm1/native/test_media_crc.c").read_text()
assert (root / "fm1/native/tools/black_frame_guard.py").is_file()
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
assert '#define APP_VERSION "0.3.5-DEV"' in app_header
assert 'APP_TAGLINE "NATIVE TRIAL"' in app_header
assert "--product FM-1_938" in ci
# Keep package header marker and running firmware FELUCCA_ID in sync by
# patching the pinned original builder BEFORE ANY compilation step.
id_patch = (root / "fm1/native/tools/set_miyako_product_id.py").read_text(encoding="utf-8")
assert 'TARGET = "FM-1_938"' in id_patch
assert 'OLD = \'    PRODUCT = prefix + "00"\'' in id_patch
assert 'NEW = \'    PRODUCT = prefix + "38"' in id_patch
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
assert (root / "fm1/native/tools/verify_fwsc.py").read_text().count('FM-1_938') >= 1

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
audio_h = (root / "fm1/native/mi_audio.h").read_text(encoding="utf-8")
audio_test = (root / "fm1/native/test_mi_audio.c").read_text(encoding="utf-8")
# Target is eight simultaneous voices; measure target xruns before choosing six.
assert "#define MI_NATIVE_POLY 8" in audio_h
assert "for(i=0;i<MI_NATIVE_POLY;i++)" in audio
assert "All eight slots full" in audio_test
assert "for(i=0;i<MI_NATIVE_POLY;i++)mi_audio_note_on" in audio_test
fxcode = (root / "fm1/native/mi_fx_dsp.c").read_text(encoding="utf-8")
controls = (root / "fm1/native/mi_control.c").read_text(encoding="utf-8")
assert "static void mi_media_draw_half(unsigned half)" in ui
# TAPE DOT removed by user request; keep the unrelated REEL sound FX.
assert "mi_tape_dot_draw_half" not in ui and "mi_tape_circle_x" not in ui
assert "((int32_t)visual_choice+steps)%12" in ui
assert "if(next<0)next+=12;" in ui
assert "if(next<0)next+=13;" not in ui
assert 'mi_streq(what,"visual_choice")' in ui
assert "expect visual_choice 11" in (root / "fm1/native/host_smoke.txt").read_text()
assert "(normal_visual_bank+1u)%11u" in ui
assert "if(mode==9u||mode==10u)return (int)(mode-3u);" in ui
assert "case MI_FX_REEL:return 9u;" not in ui
assert "if(state.view==MI_VIDEO)" in ui and "mi_media_draw_half(1)" in ui
assert "MIYAKO FM-1 / NATIVE PREVIEW" not in ui
# FM-1 OSC page displays OSC1 or OSC2, never the obsolete OSC1-only
# expression that caused a source-contract false failure before host tests.
assert "mi_wave_name(show_wave)" in ui
assert "audio.osc_scope[idx]" in ui
assert "osc_scope_last_active=active;" in ui
assert "float p=(float)(i%112u)/112.f;" not in ui
assert "osc_capture+=main" in audio and "osc_capture+=second" in audio
assert "++a->osc_scope_head;" in audio
assert "a->osc_scope_active=scope_audible;" in audio
assert "MIYAKO_OSC_LIVE_SINE.png" in ci
assert "expect osc_scope_active 1" in (root / "fm1/native/host_smoke.txt").read_text()
assert "state.osc_page" in ui and "state.osc2_wave" in ui
# HOME MULTI uses an actual selected OSC1/OSC2 waveform.
assert "mi_wave_name(state.view==MI_HOME?" in ui
assert "state.wave==MI_WAVE_MULTI" in ui
assert "state.osc1_wave" in ui
# HOME K1 shows program MULTI; OSC waveforms are edited within OSC pages.
assert "s->wave==MI_WAVE_MULTI&&s->osc_page<2u" in controls
assert 'static const char *const multi[4]={"WAVE","OSC","DETUNE","LEVEL"};' in controls
assert 'mi_streq(what,"osc1_wave")' in ui
assert 'mi_streq(what,"osc2_wave")' in ui
assert "expect osc1_wave 3" in (root / "fm1/native/host_smoke.txt").read_text()
assert "expect osc2_wave 2" in (root / "fm1/native/host_smoke.txt").read_text()
# FX SELECT restores page navigation; only SEL changes photo banks.
assert 'if(i==EN_SELECT&&state.view==MI_VIDEO)' in ui
assert 'if(i==EN_SELECT&&(state.view==MI_FX||state.view==MI_VIDEO))' not in ui
assert '"PRESET:P1/2"' in ui
assert "turn SELECT 1\nexpect view 3\ntap HOME" in (root / "fm1/native/host_smoke.txt").read_text()
assert "mi_graph_env();" in ui and "mi_graph_lfo();" in ui
assert "mi_graph_arp();" in ui
assert "mi_audio_input_on(&audio,&state" in ui
assert "mi_audio_input_off(&audio,&state" in ui
assert "arp_tick(a,rs);" in audio
assert "held_total" in audio and "release_samples" in audio
assert "lfo_value" in audio
assert "mode==MI_FX_DREAMY" in fxcode
assert "mi_wave_name" in controls
assert "case MI_VIDEO:" in controls
# All non-REEL FX, including DREAMY, are routed through one generic
# native DSP dispatcher. Do not regress to checking a non-existent per-mode
# branch: that stale assertion caused #263 to fail before any C tests ran.
# Full DSP interface has six parameters for DREAMY/EUREKA, while keeping
# the original 2-param API for older host tests. Contract matches current code.
assert "mi_fx_dsp_process_ex(&native_fx,mode,dry,a->sample_rate" in audio
assert "s->fx_extra[mode]" in audio
assert "mi_fx_dsp_process_ex(" in fxcode
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
# Expanded ③..⑫ require full real control/audio paths, never a UI-only stub.
assert "MI_WAVE_FMDRUM" in controls and "drum_on(a,s" in audio
assert "mi_rec_external_tick" in audio and "MI_REC_CAPACITY" in recorder
assert "MI_FX_BITCRUSH" in fxcode and "MI_FX_PITCHSHIFT" in fxcode
assert "mi_lfo_dest_name" in controls and "apply_lfo_destination" in audio
assert "mi_visual_map(&sx,&sy" in ui and "mi_visual_photo_bank" in ui
assert "next=visual_rng%10u<3u?" in ui
assert "filter_cutoff" in controls and "a->filter_a+=" in audio
# Current user requirements: original animation into normal SEL (not FX-only),
# no newly invented video codec or new CircleCI workflow; multi-OSC and overdub.
atlas = (root / "fm1/native/mi_tile_atlas.h").read_text(encoding="utf-8")
dot = (root / "fm1/native/mi_dreamy_dot_portrait.h").read_text(encoding="utf-8")
assert "mi_dreamy_dot_portrait.h" in ci
assert '#include "mi_dreamy_dot_portrait.h"' in ui
assert "MI_DREAMY_DOT_BYTES 690u" in dot
assert "mi_dreamy_dot_draw_half(half,0,0,240,0)" in ui
# TAPE DOT was deleted; the 11-bank SEL contract is verified above.
assert "if(state.fx==MI_FX_OFF&&state.fx2==MI_FX_OFF)" in ui
assert "static const uint8_t mi_photo_vq_maps[488][72]" in atlas
assert "mi_photo_vq_tilebook" in atlas and "mi_photo_vq_palette" in atlas
assert "mi_atlas_byte" not in ui  # no per-pixel base64 decode in rendering
assert "rs->wave==MI_WAVE_MULTI" in audio and "osc2_mix" in audio
assert "s->fx_page=0u;" in controls
assert "s->fx_page^=1u;" in controls
assert "REC manual stop, adaptive 2/4 bars, 192 MIDI ticks" in (root / "fm1/native/test_mi_recorder.c").read_text()
assert "mi_soft_limit(sample)" in audio
assert "p->dreamy_filter+=" in fxcode
assert "r->overdubbing=1u" in recorder
assert "r->overdubbing&&r->count>=4u" in recorder
# Upstream fm1-base's existing CRC-checked separate resource installer,
# explicitly approved for integrated animation; user data sectors stay intact.
resource_header = (root / "fm1/native/mi_resource_media.h").read_text()
resource_packer = (root / "fm1/native/tools/pack_miyako_resource.py").read_text()
assert 'backup = true' in manifest
assert 'resources = "miyako_media.bin"' in manifest
assert 'APP_RES_BASE 0x97000u' in app_header
assert 'APP_RES_SECTORS 69u' in app_header
assert 'APP_STORE_MAP {0xFC000u, 0xDC000u}' in app_header
assert 'APP_BACKUP_OBJS {OBJ_SETTINGS}' in app_header
assert 'MI_TILE_VQ_MAP_OFFSET 99072u' in resource_header
assert 'MI_TILE_VQ_BOOK_OFFSET 768u' in resource_header
assert 'MI_TILE_VQ_MAP_STRIDE 288u' in resource_header
assert 'MI_TILE_VQ_FRAMES_PER_CLIP 27u' in resource_header
assert 'MI_TILE_SIDE 96u' in resource_header
assert 'MI_TILE_COUNT 628u' in resource_header
assert 'MI_TILE_BANKS 8u' in resource_header
assert 'MI_TILE_DATA_BYTES 279936u' in resource_header
assert 'test_miyako_resource96.py' in ci
assert '279968' in ci and '279936' in ci
assert '#include "mi_resource_media.h"' in ui and 'plat_res(&length)' in ui
assert "mi_resource_media.h" in ci
assert "pack_miyako_resource.py" in ci
assert 'MIYAKO-FM1-NATIVE-DEV-res.bin' in ci
# Photo bank requires the CRC-checked separate resource. Both SEL and FX
# must display a visible existing original when a user flashes FWSC alone.
assert "mi_dreamy_dot_draw_half(half,px,py,size,clip_left);" in ui
assert "resource_length<MI_TILE_DATA_BYTES" in ui
assert "mi_photo_cached=-1;" in ui
# Toggling res 0 must force a render; pixel tests must not accept the
# previous cached photo screenshot without rendering the fallback.
smoke = (root / "fm1/native/host_smoke.txt").read_text()
assert "res 0\nexpect media_ready 0\n" in smoke
assert "turn SELECT 1\nturn SELECT -1\nexpect visual_choice 1" in smoke
assert "stale rendering" in (root / "fm1/native/tools/check_noresource_display.py").read_text()
for name in ("MIYAKO_NO_RESOURCE_SEL.png", "MIYAKO_NO_RESOURCE_FX.png",
             "MIYAKO_PHOTO_RESOURCE_RESTORED.png"):
    assert name in (root / "fm1/native/host_smoke.txt").read_text()
assert ci.count("check_noresource_display.py") == 2
assert 'expect media_ready 1' in (root / "fm1/native/host_smoke.txt").read_text()
assert 'MIYAKO_ADDED_SHEET1.png' in (root / "fm1/native/host_smoke.txt").read_text()
assert 'MIYAKO_ADDED_SHEET2.png' in (root / "fm1/native/host_smoke.txt").read_text()
# Host must prove FX P1 starts on MODE, PRESET detents alone toggle P2.
host_smoke = (root / "fm1/native/host_smoke.txt").read_text()
assert "turn K1 1\nexpect fx 2\nexpect fx_page 0" in host_smoke
assert "turn PRESET 1\nexpect fx_page 1" in host_smoke
assert "turn PRESET 1\nexpect fx_page 0" in host_smoke
assert "turn K1 4\nexpect fx 6" in host_smoke
assert "turn K1 2\nexpect fx2 2\nexpect fx_page 0\nturn K1 1\nexpect fx 6\nexpect fx2 4" in host_smoke
assert "turn K1 5\nexpect fx 6" not in host_smoke
# All three host scripts must select EUREKA with the correct per-detent
# K1/PRESET sequence; unlike "spin", "turn" never batches detents.
host_eureka = (root / "fm1/native/host_eureka_preview.txt").read_text()
assert "turn K1 2\nexpect fx 2\nturn K1 4\nexpect fx 6" in host_eureka
assert "turn K1 6\nexpect fx 6" not in host_eureka
host_fx = (root / "fm1/native/host_fx_preview.txt").read_text()
assert "spin K1 -3\nexpect fx 2" in host_fx
assert "spin PRESET 1" not in host_fx
assert "bb5345df9b9e4118052b2feac603e38f16db0fccd5af49223e52b08115a1cc40" in resource_packer
assert "RESOURCE_CAPACITY = 69 * 4096 - 32" in resource_packer
assert "photo96_rebuild.py" in (root / "fm1/native/tools/PHOTO96_REBUILD.md").read_text()
assert "mi_photo_palette[i]=(uint16_t)" in ui
assert "clip=frame/MI_TILE_VQ_FRAMES_PER_CLIP" in ui
assert "mi_visual_photo_bank(mode)" in ui
assert "MIYAKO_FX_DREAMY_LARGE.png" in ci
assert "MIYAKO_FX_EUREKA_LARGE.png" in ci
assert "shot MIYAKO_FX_DREAMY_LARGE.png" in (root / "fm1/native/host_smoke.txt").read_text()
assert "shot MIYAKO_FX_EUREKA_LARGE.png" in (root / "fm1/native/host_smoke.txt").read_text()
# All user photo/EUREKA/DREAMY FX media is enlarged above all four knobs.
# Reject the old cropped right-hand 116px panel and artificial scanlines.
assert "mi_tile_draw_half(half,40,18,160,0,audio_activity)" in ui
assert "mi_dreamy_dot_draw_half(half,40,18,160,0)" in ui
assert "mi_media_draw_scaled_half(half,0,25,240,146,0,mode==7u)" in ui
assert "mi_fx_dial(half,181,183,3u)" in ui
assert "mi_fx_dial(half,121,183,2u)" in ui
assert "mi_video_vfx_half" not in ui
assert "int32_t tear=0" not in ui
assert "mi_tile_draw_half(half,123,62,116,123" not in ui
assert "mi_media_draw_scaled_half(half,123,84,116,71" not in ui
assert "if(x<clip_left)" in ui and "mi_image_rect(" in ui
assert "mi_photo_reference_rgb(rgb)" not in ui
assert "mi_user_reference_rgb565[16]" not in ui
assert "mi_photo_palette[i]=(uint16_t)" in ui
assert "mi_tile_bank_first[8]" in resource_header
for source in ("mi_palette.py", "pack_eureka_frames.py"):
    color_source = (root / "fm1/native/tools" / source).read_text()
    assert "ImageStat" in color_source and "blue_lut" in color_source
    assert "ImageFilter.UnsharpMask" in color_source
    assert "gain = min(1.10," in color_source
    assert "** 0.86" in color_source
    assert "from mi_reference_palette import reference_pixels" in color_source
ref_palette = (root / "fm1/native/tools/mi_reference_palette.py").read_text()
assert "REFERENCE_RGB = (" in ref_palette
assert "REFERENCE_565" in ref_palette
assert "Image.Dither.NONE" in ref_palette

assert "fm1/native/mi_tile_atlas.h \\" not in ci, (
    "no duplicate 93 KB photo C table in the actual target app")
assert "rec_overdub" in ui and "rec_overdub 1" in (root / "fm1/native/host_smoke.txt").read_text()
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
