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
    "only: feature/fm1-miyako-video-opaque-letter-sub-20261108",
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
# CircleCI expands <<...>> at config compile time. Permit precisely
# two documented placeholders: run_build and the pipeline's number. The
# latter is explicitly injected into the job environment for the version
# generator; CIRCLE_PIPELINE_NUMBER is not a built-in job variable.
assert ci.count("<< pipeline.number >>") == 1
assert "CIRCLE_PIPELINE_NUMBER: << pipeline.number >>" in ci
assert 'case "${CIRCLE_PIPELINE_NUMBER:-}" in' in ci
assert "PASS: CircleCI pipeline #$CIRCLE_PIPELINE_NUMBER" in ci
assert "<<" not in ci.replace("<< pipeline.parameters.run_build >>", "").replace(
    "<< pipeline.number >>", ""), (
    "CircleCI 2.1 unclosed or unapproved << tag: use only documented tokens"
)
assert "printf '%s\\n'" in ci, "README must use a CircleCI-safe printf"
# #275 built and verified a valid FWSC, then failed on a README quote.
# Validate all CircleCI shell snippets before costly native build work.
assert "python3 fm1/native/tools/check_ci_run_shell.py" in ci
assert ci.index("python3 fm1/native/tools/check_ci_run_shell.py") < ci.index(
    "python3 -m compileall -q fm1/native")
assert "five-sheet set's" not in ci
assert (root / "fm1/native/tools/check_ci_run_shell.py").is_file()
assert re.search(r"^\s+only: feature/fm1-miyako-video-opaque-letter-sub-20261108\s*$", ci, re.M)
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
# Current visible version, FWSC model and installed device identity MUST
# derive from the same CircleCI pipeline number; fixed 938 never passes CI.
manifest = (root / "fm1/native/fm1.toml").read_text(encoding="utf-8")
app_header = (root / "fm1/native/app.h").read_text(encoding="utf-8")
ui = (root / "fm1/native/app.c").read_text(encoding="utf-8")
id_patch = (root / "fm1/native/tools/set_miyako_product_id.py").read_text(encoding="utf-8")
verify_fwsc = (root / "fm1/native/tools/verify_fwsc.py").read_text(encoding="utf-8")
assert 'id_prefix = "FM-1_9"' in manifest
assert '#define APP_VERSION "UNVERSIONED"' in app_header
assert 'APP_TAGLINE "NATIVE TRIAL"' in app_header
assert "CIRCLE_PIPELINE_NUMBER" in id_patch
assert "CIRCLE_PIPELINE_NUMBER: << pipeline.number >>" in ci
assert "PIPELINE_OFFSET = 650" in id_patch
assert 'APP_OLD = \'#define APP_VERSION "UNVERSIONED"\'' in id_patch
assert "product_for_pipeline" in id_patch
assert "939 <= value <= 999" in id_patch
assert 'OLD = \'    PRODUCT = prefix + "00"\'' in id_patch
assert "set_miyako_product_id.py --base /tmp/fm1-base" in ci
assert 'test -s /tmp/fm1_product_id.txt' in ci
assert 'grep -Fq "#define APP_VERSION' in ci
assert '--product "$(cat /tmp/fm1_product_id.txt)"' in ci
assert 'ap.add_argument("--product", required=True)' in verify_fwsc
assert "FM-1_938" not in ci and "FM-1_938" not in app_header
assert 'PRODUCT = prefix + "38"' not in ci
assert ci.index('set_miyako_product_id.py') < ci.index(
    'tools/build.py /tmp/fm1-base/miyako --host')
# Firmware UI renders the exact generated identity rather than static 0.3.x.
assert 'cv_text(75,11,&FONT_XS,APP_VERSION,GREY);' in ui

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
assert "((int32_t)visual_choice+steps)%11" in ui
assert "if(next<0)next+=11;" in ui
assert "if(next<0)next+=13;" not in ui
assert 'mi_streq(what,"visual_choice")' in ui
assert "expect visual_choice 10" in (root / "fm1/native/host_smoke.txt").read_text()
assert "normal_visual_bank==7u?9u:" in ui
assert "normal_visual_bank==10u?0u:" in ui
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
# HOME MULTI: PRESET controls OSC1/OSC2 selection without applying
# a global preset (which previously reset WAVE and the chosen oscillator).
# Four physical K knobs edit the selected wave / SUB or DETUNE / MIX2 / LEVEL.
assert "mi_wave_name(state.osc_page==1u?" in ui
assert "state.osc2_wave:state.osc1_wave),sizeof val" in ui
assert "state.wave==MI_WAVE_MULTI" in ui
assert "state.osc1_wave" in ui and "state.osc2_wave" in ui
assert 'static const char *const multi1[4]={"WAVE","SUB","MIX2","LEVEL"};' in controls
assert 'static const char *const multi2[4]={"WAVE","DETUNE","MIX2","LEVEL"};' in controls
assert "s->view==MI_HOME&&s->wave==MI_WAVE_MULTI" in controls
assert "if(s->osc_page>1u)s->osc_page=0u;" in controls
assert 'cv_text(104,35,&FONT_XS,"OSC",GREY);' in ui
assert 'mi_streq(what,"osc1_wave")' in ui
assert 'mi_streq(what,"osc2_wave")' in ui
assert "expect osc1_wave 0" in (root / "fm1/native/host_smoke.txt").read_text()
assert "expect osc2_wave 3" in (root / "fm1/native/host_smoke.txt").read_text()
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
assert "visual_rng%10u<3u)?visual_rng%MI_TILE_COUNT:" in ui
assert "filter_cutoff" in controls and "a->filter_a+=" in audio
# Old MI13/MI14 structs had seven FX slots; new state has nine.
# Never memcpy the old save into the new layout (misaligned FX matrices).
assert "sizeof(saved_v14_t)==104u" in ui
assert "sizeof(saved_legacy_t)==68u" in ui
assert "mi_migrate_old(&old14.state,1)" in ui
assert "mi_migrate_old(&decoded,0)" in ui
assert "mi_memcopy(&state,old14.state_bytes" not in ui
assert "if(s->view==MI_OSC&&s->osc_page==2u)break;" in controls
assert 'return "PWIDTH";' in controls
# Current user requirements: original animation into normal SEL (not FX-only),
# no newly invented video codec or new CircleCI workflow; multi-OSC and overdub.
# The old 488-frame compile-time atlas is NOT part of the active firmware.
# All 628 photo frames now come from the CRC-checked separate resource.
# Reject accidental reintroduction of the obsolete atlas into the renderer.
assert '#include "mi_dreamy_dot_portrait.h"' not in ui
assert "mi_dreamy_dot_draw_half" not in ui
assert "case MI_FX_DREAMY:return normal_visual_bank;" in ui
# TAPE DOT and the crying DREAMY portrait are not rendered.
assert "if(state.fx==MI_FX_OFF&&state.fx2==MI_FX_OFF)" in ui
assert '#include "mi_tile_atlas.h"' not in ui
assert "mi_photo_vq_maps" not in ui and "mi_photo_vq_tilebook" not in ui
assert "mi_photo_vq_palette" not in ui
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
assert '#include "mi_resource_media.h"' in ui and 'mi_photo_source(&length)' in ui
assert "mi_resource_media.h" in ci
assert "pack_miyako_resource.py" in ci
assert 'MIYAKO-FM1-NATIVE-DEV-res.bin' in ci
# Photo bank requires the CRC-checked separate resource. Both SEL and FX
# must display a visible existing EFFECTS frame when FWSC is installed alone.
assert "mi_media_draw_scaled_half(half,px,py,size,size,clip_left,0);" in ui
assert "*length<MI_TILE_DATA_BYTES" in ui
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
# The media preflight uses zero-initialised compiler stubs, not pictures.
# Image-visibility checks MUST run once after both real VQ packs are built.
assert ci.count("check_noresource_display.py") == 1
assert ci.index("Reconstruct all 576 original MIYAKO EUREKA frames") < ci.index(
    "check_noresource_display.py")
assert ci.index("Compile FM-1 native platform host simulator and exercise UI") < ci.index(
    "check_noresource_display.py")
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
# Fill the entire 240x240 LCD with media; FX dials remain visible on top.
# Reject the old letterboxed SEL and cropped FX image area.
assert "mi_tile_draw_half(half,0,0,240,0,audio_activity)" in ui
assert "mi_dreamy_dot_draw_half" not in ui
assert "mi_media_draw_scaled_half(half,0,0,240,240,0,mode==7u)" in ui
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
    assert "from mi_reference_palette import natural_pixels" in color_source
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
# MI17: real persistent 4-slot patch data, not four labels pointing to
# the same one-slot OBJ_SETTINGS object. Old MI16/15/14/13 still migrate.
assert "mi_state_t slots[4];" in ui
assert "#define SAVED_MAGIC_M16" in ui
assert "sizeof(saved_mi16_t)" in ui
assert "MI16_STATE_BYTES" in ui
assert "sizeof(saved_bank_t)<=3840u" in ui
assert "mi_store_bank(void)" in ui
assert "mi_store_slot(void)" in ui and "mi_load_slot(void)" in ui
assert "saved_bank.valid_mask" in ui
assert "mi_state_valid(&saved_bank.slots[index])" in ui
assert "saved_t save;" not in ui
# Dedicated pages and physical controls; no generic OSC/FX column UI.
assert "static void mi_draw_config(void)" in ui
assert "static void mi_draw_save(void)" in ui
assert "state.view==MI_CONFIG){mi_draw_config();return;" in ui
assert "state.view==MI_SAVE){mi_draw_save();return;" in ui
assert "state.config_load_armed" in ui
assert "MI_ACTION_LOAD" in ui and "MI_ACTION_LOAD" in controls
assert "s->midi_channel" in controls
assert 'static const char *const names[MI_VIEW_COUNT][4]' in controls
assert '"MIDI CH","CLOCK","PRESET","LOAD"' in controls
assert '"SLOT","ACTION","CONFIRM",""' in controls
# Clock setting applies to SEQ only. The existing REC MIDI auto-sync is
# untouched, and CC21 continues to accept all incoming MIDI channels.
assert "if(s->global[3]!=1)return" in audio
assert "mi_rec_external_tick(&a->recorder,a->rendered_samples)" in audio
assert "note_channel==state.midi_channel" in ui
assert "note==MI_REC_PITCH_CC" in ui
# DRUM is summed after synth FILTER and FX but before REC capture.
assert "drum_mix+=drum" in audio
assert "sample=effect(a,rs,sample);" in audio
assert "sample=mi_soft_limit(sample+drum_mix);" in audio
assert "mi_rec_tick(&a->recorder,sample,rs->seq)" in audio
assert "if(r->armed && ((r->clock_ticks-1u)%96u)==0u)" in recorder
# Real host screenshots plus flash-reboot/load assertions.
for name in ("MIYAKO_GLOBAL.png","MIYAKO_GLOBAL_MIDI.png",
             "MIYAKO_SAVE_LOAD.png"):
    assert name in ci and name in host_smoke
assert "expect save_mask 3" in host_smoke
assert "expect midi_channel 2" in host_smoke
assert "expect clock_mode 1" in host_smoke
assert "expect wave 5" in host_smoke
# Exact user-supplied 8x9 animation: 72 real frames, RGB565/16 colour,
# 80x40 scene-only. The existing OSC scope MUST remain visibly overlaid
# between the original postbox and letter; no invented artwork.
letter_script = (root / "fm1/native/tools/build_letter_anim_header.py").read_text()
letter_data = root / "fm1/native/tools/mi_letter_72frames.b64"
assert letter_data.is_file() and letter_data.stat().st_size > 4000
assert 'RAW_LENGTH = 32 + 256 * 8 + 72 * 200' in letter_script
assert 'RAW_SHA256 = "8e4a5b1f02d6dcf424e9ca28b7d1813e6837432ed23a56fd4f565400ad905e72"' in letter_script
assert '#include "mi_letter_anim.h"' in ui
assert 'mi_letter_maps[frame*200u+' in ui
assert 'mi_letter_book[block*8u+' in ui
assert 'mi_letter_brightness(mi_letter_palette[p])' in ui
assert 'previous?YELLOW:C_BLACK' in ui
assert 'state.sub*(MI_LETTER_FRAMES-1u)+63u' in ui
assert 'mi_draw_sub_scene();' in ui
assert 'mi_draw_letter_waveform();' in ui
assert 'audio.osc_scope[idx]*18/127' in ui
assert '"sub_frame"' in ui
assert '"sub"' in ui
assert 'mi_letter_anim.h' in (root / "fm1/native/verify_standalone.py").read_text()
assert "build_letter_anim_header.py --out /tmp/mi_letter_preflight.h" in ci
assert '--out /tmp/fm1-base/miyako/mi_letter_anim.h' in ci
assert "test_miyako_dynamic_version.py" in ci
assert "MIYAKO_OSC_SUB_0.png" in ci and "MIYAKO_OSC_SUB_127.png" in ci
assert "expect sub_frame 0" in host_smoke
assert "expect sub_frame 71" in host_smoke
assert "expect sub 127" in host_smoke
assert "MIYAKO_NORMAL_SEL_RETURN.png" in ci and "MIYAKO_NORMAL_SEL_FROM_FX.png" in ci
assert "MIYAKO_NORMAL_SEL_RETURN.png" in host_smoke
assert "MIYAKO_NORMAL_SEL_FROM_FX.png" in host_smoke
# All source palettes, including zero, are opaque; no accidental black holes.
assert "if(idx==0u)continue;" not in ui
assert "if(idx){" not in ui
assert "clip_left,mi_photo_palette[idx]);" in ui
assert "clip_left,mi_media_palette[idx]);" in ui
# Re-entering the video screen resets only display scheduling state.
assert "if(state.view!=visual_previous_view)" in ui
assert "if(state.view==MI_VIDEO||state.view==MI_FX){" in ui
assert "visual_last_mode=255u;" in ui
assert "video.effects_cooldown=0u;" in ui
assert "video.eureka_previous=0.f;" in ui
# Repeat the manual gating and branch controls after introducing the assets.
assert "when: << pipeline.parameters.run_build >>" in ci
assert "only: feature/fm1-miyako-video-opaque-letter-sub-20261108" in ci
print("PASS: manual Phase 3 REC + EUREKA 2sec/576 frames + guarded dual-FX")
