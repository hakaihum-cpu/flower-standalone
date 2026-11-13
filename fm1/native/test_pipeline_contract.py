#!/usr/bin/env python3
"""CI contract: fail closed rather than accidentally shipping legacy FM-1 code."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]

# Catch stale TEST expectations before executing the rest of this contract.
# The #298 failure was an assertion for a removed grayscale renderer even
# though source-RGB had already been adopted. Inspect literal "in"/"not in"
# source contracts against the CURRENT files, including both video encoders.
# This runs in the existing source-only CircleCI step, with no new CI job.
def _audit_stale_literal_contracts():
    import ast
    aliases = {
        "ci": [".circleci/config.yml"],
        "ui": ["fm1/native/app.c"],
        "app_source": ["fm1/native/app.c"],
        "resource_header": ["fm1/native/mi_resource_media.h"],
        "header": ["fm1/native/mi_resource_media.h"],
        "resource_packer": ["fm1/native/tools/pack_miyako_resource.py"],
        "vq": ["fm1/native/tools/pack_effects_vq_candidate.py"],
        "eureka_vq": ["fm1/native/tools/pack_eureka_vq_candidate.py"],
        "encoder": [
            "fm1/native/tools/pack_effects_vq_candidate.py",
            "fm1/native/tools/pack_eureka_vq_candidate.py",
        ],
        "color_source": [
            "fm1/native/tools/mi_palette.py",
            "fm1/native/tools/pack_eureka_frames.py",
        ],
        "color_encoder": ["fm1/native/tools/mi_reference_palette.py"],
        "ref_palette": ["fm1/native/tools/mi_reference_palette.py"],
        "frame_guard": ["fm1/native/tools/black_frame_guard.py"],
        "frame_guard_test": ["fm1/native/tools/test_black_frame_guard.py"],
        "native_audit": ["fm1/native/verify_standalone.py"],
        "manifest": ["fm1/native/fm1.toml"],
        "app_header": ["fm1/native/app.h"],
        "id_patch": ["fm1/native/tools/set_miyako_product_id.py"],
        "verify_fwsc": ["fm1/native/tools/verify_fwsc.py"],
        "mem": ["fm1/native/mi_memory.h"],
        "audio": ["fm1/native/mi_audio.c"],
        "audio_h": ["fm1/native/mi_audio.h"],
        "audio_test": ["fm1/native/test_mi_audio.c"],
        "controls": ["fm1/native/mi_control.c"],
        "fxcode": ["fm1/native/mi_fx_dsp.c"],
        "fx_test": ["fm1/native/test_mi_fx_dsp.c"],
        "recorder": ["fm1/native/mi_recorder.c"],
        "env90_builder": ["fm1/native/tools/build_ui_anim_header.py"],
        "letter_script": ["fm1/native/tools/build_letter_anim_header.py"],
        "smoke": ["fm1/native/host_smoke.txt"],
        "host_smoke": ["fm1/native/host_smoke.txt"],
        "host_fx": ["fm1/native/host_fx_preview.txt"],
        "host_eureka": ["fm1/native/host_eureka_preview.txt"],
    }
    sources = {p: (root / p).read_text(encoding="utf-8")
               for paths in aliases.values() for p in paths}
    checked = 0
    for test_path in (
        "fm1/native/test_pipeline_contract.py",
        "fm1/native/tools/test_video_palette_natural.py",
    ):
        code = (root / test_path).read_text(encoding="utf-8")
        tree = ast.parse(code, filename=test_path)
        for node in ast.walk(tree):
            if not isinstance(node, ast.Assert):
                continue
            for check in ast.walk(node.test):
                if not (isinstance(check, ast.Compare) and
                        len(check.ops) == 1 and len(check.comparators) == 1):
                    continue
                if not (isinstance(check.left, ast.Constant) and
                        isinstance(check.left.value, str) and
                        isinstance(check.comparators[0], ast.Name) and
                        isinstance(check.ops[0], (ast.In, ast.NotIn))):
                    continue
                wanted = isinstance(check.ops[0], ast.In)
                for source in aliases.get(check.comparators[0].id, ()):
                    found = check.left.value in sources[source]
                    if found != wanted:
                        raise AssertionError(
                            f"STALE TEST: {test_path}:{node.lineno} "
                            f"contradicts {source}: {check.left.value!r}"
                        )
                    checked += 1
    if checked < 200:
        raise AssertionError("STALE TEST audit unexpectedly lost coverage")
    print(f"PASS: {checked} current-source contract comparisons")

_audit_stale_literal_contracts()

# #303: the original OSC splash test assumed there would never be more than
# two reboots in the growing host_smoke.txt. Reject reintroducing a GLOBAL
# smoke reboot count/last-occurrence test. Other independent cases may freely
# append reboots; the original checks must remain scoped before the OSC shots.
def _global_smoke_reboot_calls(source):
    import ast
    result = []
    for node in ast.walk(ast.parse(source)):
        if not isinstance(node, ast.Call) or not isinstance(node.func, ast.Attribute):
            continue
        method = node.func
        if method.attr not in ("count", "rindex"):
            continue
        if not isinstance(method.value, ast.Name) or method.value.id not in (
                "smoke", "host_smoke"):
            continue
        if node.args and isinstance(node.args[0], ast.Constant) and (
                isinstance(node.args[0].value, str) and
                "reboot" in node.args[0].value):
            result.append(node.lineno)
    return result

assert _global_smoke_reboot_calls(
    'assert smoke.count("reboot\\nwait 3200") == 2')
assert _global_smoke_reboot_calls(
    'assert smoke.rindex("reboot\\nwait 3200") < 99')
_regression_source = Path(__file__).read_text(encoding="utf-8")
assert not _global_smoke_reboot_calls(_regression_source), (
    "REGRESSION #303: a global smoke reboot count/rindex will fail whenever "
    "later features legitimately add reboot scenarios")
ci = (root / ".circleci/config.yml").read_text(encoding="utf-8")
required = (
    "version: 2.1",
    "default: false",
    "when: << pipeline.parameters.run_build >>",
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
# #301: an unused local in the late SET1 host test failed -Werror.
# Check ALL eleven real first-stage compiler invocations before running tests.
shell_gate = (root / "fm1/native/tools/check_ci_run_shell.py").read_text()
assert "def precompile_first_stage(" in shell_gate
assert 'len(compiles) != 11' in shell_gate
assert '"-fsyntax-only"' in shell_gate and '["bash", "-n"]' in shell_gate
assert "precompile_first_stage(scripts[0][1], root)" in shell_gate
assert "def assert_compiler_warning_gate()" in shell_gate
assert "int main(void){int unused;return 0;}" in shell_gate
assert '"unused-variable" not in bad.stderr' in shell_gate
assert "if not assert_compiler_warning_gate():" in shell_gate
assert shell_gate.index("if not assert_compiler_warning_gate():") < shell_gate.index(
    "if not precompile_first_stage(scripts[0][1], root):")
piano_host = (root / "fm1/native/test_mi_piano_set1.c").read_text()
assert "const uint8_t *samples=mi_piano_set1_data(zone);" not in piano_host
assert "mi_piano_set1_tick(zone,&pos,1.f)" in piano_host
# #305: bind the manual workflow filter to its real checkout guard rather
# than preserving a stale feature branch literal in this Python test.
checkout_branches = re.findall(
    r'^\s+test "\$CIRCLE_BRANCH" = "(feature/fm1-[^"]+)"\s*$', ci, re.M)
filtered_branches = re.findall(
    r'^\s+only: (feature/fm1-[^\s]+)\s*$', ci, re.M)
assert len(checkout_branches) == 1 and len(filtered_branches) == 1, (
    "FM-1 job and workflow must each restrict exactly one feature branch")
assert checkout_branches[0] == filtered_branches[0], (
    "FM-1 checkout guard and workflow branch filter differ")
# Fail closed on reintroducing another literal branch assertion in this
# Python contract. Moving to a later feature branch then remains safe.
assert not re.search(r'only: feature/fm1-[\w-]+\d{8}',
                     Path(__file__).read_text(encoding="utf-8"))

# #308: JieLi SDK's full Gitee pack was severed by curl 56 / TLS EOF.
# The approved SDK tag and the 3 exact package input hashes must remain
# mandatory. Narrow sparse checkout is retried without changing CI workflows.
sdk_step = ci[ci.index("name: Install pinned JieLi compiler"):
              ci.index("name: Verify freestanding MIYAKO C")]
assert "sudo apt-get update" not in sdk_step, (
    "regression #308: redundant apt refresh triggers dead git-core PPA")
assert ci.count("sudo apt-get update") == 1
assert "for attempt in 1 2 3; do" in sdk_step
assert "rm -rf \"$AC79_SDK\"" in sdk_step
assert "--filter=blob:none --sparse" in sdk_step
assert "sparse-checkout set cpu/wl82/tools" in sdk_step
assert "timeout 240 git -c http.version=HTTP/1.1 clone" in sdk_step
assert "timeout 300 git -c http.version=HTTP/1.1 clone" in sdk_step
assert "sdk_tag=AC79NN_SDK_V1.2.1_2023-12-13" in sdk_step
assert "test \"$sdk_head\" = \"$sdk_pin\"" in sdk_step
assert 'if [ "$sdk_ok" -ne 1 ]; then' in sdk_step
assert 'echo "PASS: pinned JieLi SDK tag and all three SHA-256 packaging inputs"' in sdk_step
for sdk_path, sha in (
    ("cpu/wl82/tools/uboot.boot",
     "4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4"),
    ("cpu/wl82/tools/cfg_tool.bin",
     "276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b"),
    ("cpu/wl82/tools/cfg/eq_cfg_hw.bin",
     "41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d"),
):
    assert sha + "  $AC79_SDK/" + sdk_path in sdk_step
assert sdk_step.count("sha256sum -c -") == 3

# A correct branch in the WRONG GitHub repository cannot build this app.
# Reject the checkout before downloading 720 frames or starting the toolchain.
assert 'git remote get-url origin' in ci
assert 'PASS: FM-1 checkout source hakaihum-cpu/flower-standalone' in ci
assert 'FAIL: CircleCI checkout source must be hakaihum-cpu/flower-standalone' in ci
assert ci.index("source_url=") < ci.index("python3 fm1/native/tools/check_ci_run_shell.py")
assert "/tmp/mi-video-venv/bin/python /tmp/fm1-base/tools/build.py /tmp/fm1-base/miyako" in ci
assert "test_media_crc.c" in ci
assert "pack_effects_vq_candidate.py" in ci
assert "pack_eureka_vq_candidate.py" in ci
assert "test -s /tmp/fm1-base/miyako/build/miyako.fwsc" in ci
assert "scripts/materialize_effects_video_pack.py" in ci
assert "scripts/materialize_miyako_eureka_video.py" in ci
assert "SLICER FX (RATE/RND/WET) processes the final stereo mix" in ci
assert "Automatic step-sequencer FMDrum bypasses the recorder capture bus" in ci
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
assert "mi_fx_dsp.c fm1/native/mi_recorder.c fm1/native/mi_rec_pcm7.c fm1/native/mi_slicer.c fm1/native/mi_piano_set1_engine.c" in ci
assert "mi_piano_set1_51.c fm1/native/mi_piano_set1_58.c fm1/native/mi_piano_set1_65.c fm1/native/test_mi_audio.c" in ci
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
# The same 128x78 16-colour media footprint is preserved. Quality
# candidates may use ALL 256 opaque VQ entries, but may never replace the
# historical candidate with worse measured worst- or mean-frame PSNR.
for name, count, media_size in (
    ("pack_effects_vq_candidate.py", "144u", "62592"),
    ("pack_eureka_vq_candidate.py", "576u", "250368"),
):
    encoder = (root / "fm1/native/tools" / name).read_text()
    assert "free_zero=True" in encoder
    assert "start = 0 if free_zero else 1" in encoder
    assert "def improves_both(candidate, prior):" in encoder
    assert "def source_quality(reconstruction):" in encoder
    assert "original_rgb = np.stack" in encoder
    assert "improves_both(source_quality(trial[2]), source_quality(chosen[2]))" in encoder
    assert "improves_both(trial[3], chosen[3])" in encoder
    assert "for row, frame in enumerate((0, FRAMES//2, FRAMES-1)):" in encoder
    assert count in encoder and media_size in encoder
color_encoder = (root / "fm1/native/tools/mi_reference_palette.py").read_text()
assert "Image.Quantize.MEDIANCUT" in color_encoder
assert "Image.Quantize.FASTOCTREE" in color_encoder
assert "Image.Quantize.MAXCOVERAGE" in color_encoder
assert "dtype=np.int32" in color_encoder  # no RGB luma overflow
assert "frame_loss.max()" in color_encoder
# Index zero is an opaque RGB565 palette colour. Treating it as alpha
# discarded real frames from BOTH FX and EUREKA.
frame_guard = (root / "fm1/native/tools/black_frame_guard.py").read_text()
assert "visible[0] = False" not in frame_guard
assert "Palette entry zero is an OPAQUE colour" in frame_guard
frame_guard_test = (root / "fm1/native/tools/test_black_frame_guard.py").read_text()
assert "bright_zero[0]=0xFFFF" in frame_guard_test
assert "visible_pixel_counts(all_zero,bright_zero)" in frame_guard_test
assert ci.index("Reconstruct all 144 original MIYAKO EFFECTS") < ci.index(
    "Reconstruct all 576 original MIYAKO EUREKA")
assert ci.index("Reconstruct all 576 original MIYAKO EUREKA") < ci.index(
    "Verify all 720 EFFECTS and EUREKA")
# Target-only validation MUST precede the expensive user-video VQ stage.
assert ci.index("Install pinned JieLi compiler") < ci.index(
    "Verify freestanding MIYAKO C with the actual JieLi target compiler")
assert ci.index("Verify freestanding MIYAKO C with the actual JieLi target compiler") < ci.index(
    "Reconstruct all 144 original MIYAKO EFFECTS source frames")
assert "for unit in mi_fx_dsp.c mi_audio.c mi_recorder.c mi_rec_pcm7.c mi_slicer.c mi_piano_set1_engine.c mi_piano_set1_51.c" in ci
assert "mi_piano_set1_58.c mi_piano_set1_65.c mi_control.c mi_video.c" in ci
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
# FWSC/product identity still follows pipeline.number internally. The user
# explicitly did NOT request a visible version: the artwork replaces it.
assert 'cv_text(75,11,&FONT_XS,APP_VERSION,GREY);' not in ui
assert 'mi_ui_draw_logo();' in ui
assert 'mi_ui_draw_start();' in ui
assert 'mi_ui_draw_motion(0u,mi_ui_param_frame(0u));' in ui
assert 'mi_ui_draw_motion(1u,mi_ui_param_frame(1u));' in ui
assert 'mi_ui_motion_decode(unsigned kind,unsigned frame)' in ui

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
# HOME K1 globally selects ALL sounds, including FMDRUM/PIANO beyond MULTI.
# PRESET still selects OSC1/OSC2; those LOCAL waves are edited on the OSC page.
# A previous contract incorrectly pinned the HOME K1 MULTI trap.
assert "if(state.view==MI_HOME&&i==0u)" in ui
assert "textcpy(val,mi_wave_name(state.wave),sizeof val);" in ui
assert "if(s->view==MI_HOME&&k==0u){" in controls
assert "while(left--){" in controls
assert "if(old==3u&&s->wave==MI_WAVE_MULTI){" in controls
assert "if(s->wave==old)break;" in controls
assert "if(left>MI_WAVE_PIANO)left=MI_WAVE_PIANO;" in controls
assert "s->osc1_wave=old;" in controls
assert 'if(old==MI_WAVE_MULTI&&s->wave!=MI_WAVE_MULTI)' in controls
assert "if(old<=3u)s->osc1_wave=old;" in controls
assert "state.wave==MI_WAVE_MULTI" in ui
assert "state.osc1_wave" in ui and "state.osc2_wave" in ui
assert 'static const char *const multi1[4]={"WAVE","SUB","MIX2","LEVEL"};' in controls
assert 'static const char *const multi2[4]={"WAVE","DETUNE","MIX2","LEVEL"};' in controls
assert "s->view==MI_HOME&&s->wave==MI_WAVE_MULTI" in controls
assert "if(s->osc_page>1u)s->osc_page=0u;" in controls
assert 'cv_text(104,35,&FONT_XS,"OSC",GREY);' in ui
assert 'mi_streq(what,"osc1_wave")' in ui
assert 'mi_streq(what,"osc2_wave")' in ui
# #311: the physical turns 0->1->2->3->MULTI; OSC1 inherits PULSE (3).
# Stopping at 0 was a defective oracle, and batched K1 input must agree.
home_smoke=(root/"fm1/native/host_smoke.txt").read_text(encoding="utf-8")
home_part=home_smoke.split("# HOME K1 must pass through MULTI",1)[1].split(
    "# Physical SEQ button long-hold",1)[0]
assert home_part.count("expect osc1_wave 3")==4
assert "expect osc1_wave 0" not in home_part
assert "turn K1 -2\nexpect osc1_wave 1" in home_part
control_test=(root/"fm1/native/test_mi_control.c").read_text(encoding="utf-8")
assert "for(i=0u;i<4u;i++)mi_rotate(&stepped,MI_EN_K1,1);" in control_test
assert "assert(s.osc1_wave==stepped.osc1_wave);" in control_test
assert "assert(memcmp(&batched,&sequential,sizeof batched)==0);" in control_test
assert "mi_rotate(&s,MI_EN_K1,2); /* PULSE -> MULTI -> FMDRUM in 2 clicks */" in control_test
assert "assert(s.wave==MI_WAVE_FMDRUM&&s.osc1_wave==3u);" in control_test
assert "expect osc2_wave 3" in (root / "fm1/native/host_smoke.txt").read_text()
assert "expect osc2_wave 2" in (root / "fm1/native/host_smoke.txt").read_text()
assert "turn K1 1\nexpect wave 5\nturn K1 1\nexpect wave 6" in (root / "fm1/native/host_smoke.txt").read_text()
# FX SELECT restores page navigation; only SEL changes photo banks.
assert 'if(i==EN_SELECT&&state.view==MI_VIDEO)' in ui
assert 'if(i==EN_SELECT&&(state.view==MI_FX||state.view==MI_VIDEO))' not in ui
assert '"PRESET:P1/2"' in ui
assert "turn SELECT 1\nexpect view 3\ntap HOME" in (root / "fm1/native/host_smoke.txt").read_text()
assert "mi_graph_env();" in ui and "mi_graph_lfo();" in ui
assert "mi_draw_arp();" in ui
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
assert "mode!=MI_FX_OFF&&mode<MI_FX_STORAGE_COUNT" in audio
assert "if(a->slicer_mix>0.0001f)" in audio
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
# Three independent 7-bit log-PCM buffers replace the original monolithic PCM.
# Capturing is sample-accurate; block writes are committed only once per 64
# sample span or at stop/finish, never re-encoded for each audio sample.
assert "static uint8_t clip_ram[MI_REC_PCM7_BLOCKS][MI_REC_PCM7_BLOCK_BYTES];" in recorder
assert "static uint8_t clip_pool[MI_REC_TRACKS-1u][MI_REC_PCM7_BLOCKS]" in recorder
assert "return tr==0u?clip_ram[block]:clip_pool[tr-1u][block];" in recorder
# This explicitly reduces POOL usage without shortening existing DELAY,
# REVERB, DREAMY, SLICER, or REC's three 44,100-point tracks.
assert 329100-88200+2*690*56+16384 <= 344064
assert 33804+690*56 <= 98304
assert "mi_pcm7_encode_64" in recorder and "mi_pcm7_decode_64" in recorder
pcm7 = (root / "fm1/native/mi_rec_pcm7.c").read_text(encoding="utf-8")
pcm7_h = (root / "fm1/native/mi_rec_pcm7.h").read_text(encoding="utf-8")
assert "#define MI_REC_PCM7_BLOCK_BYTES 56u" in pcm7_h
assert "static const int16_t magnitude[64]" in pcm7
assert "bit=7u*i" in pcm7
assert "MI_REC_PCM7_STORE_BYTES==115920u" in (root / "fm1/native/test_mi_rec_pcm7.c").read_text()
assert "MI_REC_PCM7_STORE_BYTES==115920u" in (root / "fm1/native/test_mi_multirec.c").read_text()
assert "for(tr=0u;tr<MI_REC_TRACKS;tr++)" in recorder
assert "t->overdubbing=1u;t->recording=1u;t->playing=1u" in recorder
assert "if(t->overdubbing&&t->count>=4u)" in recorder
assert "static void begin_overdub(mi_rec_track_t *t)" in recorder
assert recorder.count("begin_overdub(t);") == 3
assert "t->write_pos=(uint32_t)t->playback_pos;" in recorder
assert "t->playback_pos=t->reverse?(float)(t->count-1u):0.f;" in recorder
assert "float capture_step=t->capture_step,play_step=t->play_step;" in recorder
recorder_test = (root / "fm1/native/test_mi_recorder.c").read_text()
for position in ("1234.25f", "2345.75f", "3765.5f"):
    assert position in recorder_test
assert "assert(mi_rec_sample(10)==old10);" in recorder_test
assert "assert(r.playback_pos==pos);" in recorder_test
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
assert 'MI_TILE_MHP2_OVERLAY_BYTES 73004u' in resource_header
assert 'MI_TILE_MHP2_CORE_BYTES 209067u' in resource_header
assert 'MI_TILE_VQ_MAP_STRIDE 288u' in resource_header
assert 'MI_TILE_VQ_FRAMES_PER_CLIP 27u' in resource_header
assert 'MI_TILE_SIDE 96u' in resource_header
assert 'MI_TILE_COUNT 628u' in resource_header
assert 'MI_TILE_BANKS 8u' in resource_header
assert 'MI_TILE_DATA_BYTES 282071u' in resource_header
assert 'test_miyako_resource96.py' in ci
assert '282103' in ci and '282071' in ci
assert '#include "mi_resource_media.h"' in ui and 'mi_photo_source(&length)' in ui
assert "mi_resource_media.h" in ci
assert "pack_miyako_resource.py" in ci
assert 'MIYAKO-FM1-NATIVE-DEV-res.bin' in ci
# User-supplied ENV/LFO/logo/start is generated into the native app tree
# by the existing manual CI job; the source-only closure allows that header.
# Prevent another 'missing native header mi_ui_media.h' at pipeline start.
assert '#include "mi_ui_media.h"' in ui
assert '"mi_ui_media.h"' in native_audit
assert "build_ui_anim_header.py --out /tmp/mi_ui_preflight.h" in ci
assert "--out /tmp/fm1-base/miyako/mi_ui_media.h" in ci
assert "test -s /tmp/fm1-base/miyako/mi_ui_media.h" in ci
assert "MI_UI_MOTION_COUNT 100u" in ci
assert (root / "fm1/native/tools/build_ui_anim_header.py").is_file()
assert (root / "fm1/native/tools/mi_ui_env_lfo_logo_start_16color.b64").is_file()
# 190 original-source alpha masks are keyed to their *own* frames,
# not to 16-colour palette indices shared with dark hair and clothing.
matte_asset = root / "fm1/native/tools/mi_ui_env_lfo_matte_3x3.b64"
assert matte_asset.is_file() and matte_asset.stat().st_size > 1000
assert 'MATTE_SHA256 = "cdaaf835271f85976ef2d3595e9df49f605468fbd374979c204bf3145cc457e0"' in (root / "fm1/native/tools/build_ui_anim_header.py").read_text()
assert "mi_ui_env_matte" in ui and "mi_ui_lfo_matte" in ui
assert "prior<16u?colors[prior]:C_BLACK" in ui
assert "if(!(matte[alpha_byte]" in ui
assert "mi_ui_draw_motion(0u,mi_ui_param_frame(0u));" in ui
assert "mi_ui_draw_motion(1u,mi_ui_param_frame(1u));" in ui
# The supplied ENV atlas has 9 columns x 10 rows, NOT 10x10. Never use
# the old 100 ENV slices as the visible animation again.
env90_pack = root / "fm1/native/tools/mi_ui_env_9x10_16color.b64"
env90_builder = (root / "fm1/native/tools/build_ui_anim_header.py").read_text()
assert env90_pack.is_file() and env90_pack.stat().st_size > 18000
assert 'ENV90_RAW_SHA256 = "d0074a610d8eb39aad0bbfe34b2c2584043aea966c8fcf3dbc2efc1234422d7d"' in env90_builder
assert 'verify_motion("env9x10", ebooks, ebases, eoffsets, edeltas, count=90)' in env90_builder
assert "MI_UI_ENV_COUNT 90u" in ci
assert "MI_UI_ENV_COUNT" in ui and "MI_UI_MOTION_COUNT" in ui
# Only the second LCD pass may render the original-frame girl sprites.
assert 'cv_rect(96+(int32_t)start,72+(int32_t)y,' in ui
assert 'mi_ui_draw_motion(0u,mi_ui_param_frame(0u));' in ui
assert 'mi_ui_draw_motion(1u,mi_ui_param_frame(1u));' in ui
assert 'check_ui_sprite_layout.py' in ci
assert 'MIYAKO_ENV.png fm1-dist/MIYAKO_LFO.png' in ci
assert (root / "fm1/native/tools/check_ui_sprite_layout.py").is_file()
# Photo bank requires the CRC-checked separate resource. Both SEL and FX
# must display a visible existing EFFECTS frame when FWSC is installed alone.
assert "mi_media_draw_scaled_half(half,px,py,size,size,clip_left,0);" in ui
assert "*length!=MI_TILE_DATA_BYTES" in ui
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
# The archived 628 user frames now use the MHP2 source-reconstruction layout
# with 24 SOURCE-RGB palettes, not the previous grayscale recolour.
assert "35ab028b0bb48659ecab47c36afe2a4c6873e6b91f29c0b46313606f41cb5a3f" in resource_packer
assert "RESOURCE_CAPACITY = 69 * 4096 - 32" in resource_packer
assert "PALETTE_CLIPS = 24" in resource_packer
assert "return data" in resource_packer
assert "transformed = bytearray(result)" not in resource_packer
manifest_photo = __import__("json").loads(
    (root / "Resources/fm1_photo_96_chunks/manifest.json").read_text())
assert manifest_photo["payload_sha256"] == (
    "df517e2d8c7902bed076610a07876dee660890799ba5dec6bc12bf366783bcf4")
assert manifest_photo["frames"] == 628 and manifest_photo["clips"] == 24
assert manifest_photo["palette_clip_count"] == 24
assert len(manifest_photo["files"]) == 8
assert "photo96_rebuild.py" in (
    root / "fm1/native/tools/PHOTO96_REBUILD.md").read_text()
assert "MI_TILE_RESOURCE_RGB_SOURCE 1u" in resource_header
assert "MI_TILE_MHP2_CORE_BYTES 209067u" in resource_header
assert "mi_photo_grayscale_palette" not in ui
assert "mi_photo_palette[i]=mi_photo_decoder.palette[i]" in ui
assert "mi_mhp2_decode(&mi_photo_decoder" in ui
assert "res[0u]!='M'" in ui and "res[3u]!='2'" in ui
assert "mi_mhp2_decode(&mi_photo_decoder" in ui
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
# Source-RGB photo resource holds a distinct real RGB565 palette for
# every 27-frame clip. Guard the actual native decoder, not the removed
# 8-bank grayscale recolouring that caused CircleCI #298's false failure.
assert "mi_photo_grayscale_palette" not in ui
assert "mi_photo_palette[i]=mi_photo_decoder.palette[i]" in ui
assert "mi_mhp2_decode(&mi_photo_decoder" in ui
assert "mi_mhp2_decode(&mi_photo_decoder" in ui
assert "mi_tile_bank_first[8]" in resource_header
# Repeated gamma, edge sharpening and blue-channel alterations harmed
# user JPEGs before the 16-colour/VQ stage. Verify every encoder now passes
# original 36-frame RGB pixels to the source-aware RGB565 palette generator.
for source in ("mi_palette.py", "pack_eureka_frames.py"):
    color_source = (root / "fm1/native/tools" / source).read_text()
    assert "return natural_pixels(frames)" in color_source
    assert "from mi_reference_palette import natural_pixels" in color_source
    for obsolete in ("ImageStat.", "ImageFilter.UnsharpMask(",
                     "blue_lut =", "gain = min(1.10,", "** 0.86"):
        assert obsolete not in color_source
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
assert '"LOOP 1","LOOP 2","LOOP 3"' in ui
assert '"REC":t->armed?"ARM":' in ui
assert "mi_rec_peak_track(tr,bucket,&lo,&hi);" in ui
# Vertical waveform magnification only; preserve the recorded TIME axis.
assert "for(col=0u;col<120u;col+=2u)" in ui
assert "bucket=col*224u/120u" in ui
assert "hi*10/(int32_t)peak" in ui
assert "lo*10/(int32_t)peak" in ui
assert "for(b=0u;b<224u;b++)" in ui
assert "mi_recorder.c" in (root / "fm1/native/fm1.toml").read_text()
# MI19: preserve four patch slots and migrate old MI17 and MI18 banks;
# historic MI16/15/14/13 single-slot layouts still migrate.
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
# LOAD DEFAULT: factory sound is a fifth *load choice*, not a writable
# slot. Persist active_slot=0 without changing MI20 bank layout or globals.
control_h = (root / "fm1/native/mi_control.h").read_text()
control_test = (root / "fm1/native/test_mi_control.c").read_text()
assert "MI_ACTION_DEFAULT" in control_h
assert "s->save_action==2u?MI_ACTION_DEFAULT:" in controls
assert "s->save_action>2u" in controls
assert "s->save_action,d,0,2" in controls
assert "r.action==MI_ACTION_DEFAULT && r.slot==2u" in control_test
assert "s.save_action=3u;assert(!mi_state_valid(&s))" in control_test
assert "#define SAVED_MAGIC 0x4D493230u" in ui  # no format bump
assert "saved_bank.active_slot<=4u" in ui
assert "if(saved_bank.active_slot>0u&&" in ui
assert "state.project_slot=saved_bank.active_slot?" in ui
assert "static void mi_load_default(void)" in ui
assert "saved_bank.active_slot=0u;" in ui
assert "state.midi_channel=midi;" in ui and "state.global[3]=clock;" in ui
assert 'app_say("DEFAULT LOADED","PRESETS 1-4 KEPT");' in ui
assert "action.action==MI_ACTION_DEFAULT" in ui
assert 'state.save_action==2u?"DEFAULT"' in ui
assert '"BOOT: DEFAULT"' not in ui and '"BOOT: THIS SLOT"' not in ui
assert '"boot_slot"' in ui
assert "expect boot_slot 0" in host_smoke and "expect boot_slot 1" in host_smoke
assert "expect save_mask 3" in host_smoke
assert "shot MIYAKO_DEFAULT_LOAD.png" in host_smoke
assert "test -s fm1-dist/MIYAKO_DEFAULT_LOAD.png" in ci
assert "DEFAULT restores the initial MIYAKO sound" in ci
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
# Manual FMDrum enters REC, but the FMDrum sequencer goes ONLY to final
# output. The order must be unambiguous and preserved across future changes.
assert "fm1/native/mi_rec_pcm7.c fm1/native/test_mi_rec_pcm7.c" in ci
assert "fm1/native/mi_recorder.c fm1/native/mi_rec_pcm7.c fm1/native/test_mi_multirec.c" in ci
assert "fm1/native/mi_rec_pcm7.c" in (root / "fm1/native/SOURCE_ALLOWLIST.txt").read_text()
assert "fm1/native/mi_slicer.c" in (root / "fm1/native/SOURCE_ALLOWLIST.txt").read_text()
assert "fm1/native/mi_slicer.c fm1/native/test_mi_slicer.c" in ci
assert "fm1/native/test_mi_piano_set1.c -o /tmp/mi_piano_set1_test" in ci
assert "fm1/native/test_mi_piano_set1_stress.c -o /tmp/mi_piano_set1_stress" in ci
assert "150000 SET1 flash sample/pitch/random-position stress cases" in (root / "fm1/native/test_mi_piano_set1_stress.c").read_text()
assert ci.index("/tmp/mi_piano_set1_stress") < ci.index("Reconstruct all 144 original")
assert "/tmp/mi_piano_set1_test" in ci
assert "PIANO: Wingie2 SET1 only" in ci
assert "MI_WAVE_PIANO" in (root / "fm1/native/mi_control.h").read_text()
assert 'return wave<=MI_WAVE_PIANO?names[wave]:"?";' in controls
assert 'mi_piano_set1_tick(v->piano_zone,&v->piano_pos' in audio
assert "drum_on(a,s,(uint8_t)note,a->held_velocity[note],0u)" in audio
assert "drum_on(a,s,a->arp_note,vel?vel:100u,0u)" in audio
piano_header=(root / "fm1/native/mi_piano_set1.h").read_text()
assert "#define MI_PIANO_SET1_SAMPLES 9600u" in piano_header
assert "51u,58u,65u" in piano_header
assert 'mi_piano_set1_engine.c' in manifest
# Verify the actual 9,600 bytes for each user-provided source sample.
# SHA in a C comment alone cannot detect accidental audio-byte corruption.
import hashlib
piano_sha={
    51:"7a89a36fe3e4909f7c988b7bdb535be147fb848f03e4de73c45e462e9f302ea1",
    58:"ea51f90718233a14d32d7a3e69b10892d05356b1425277b2066336f36424db70",
    65:"95658795abbd1cce5c471eb3307c55d30e8f80e175d79e709bc32d959c90d92c",
}
for note in (51,58,65):
    piano_path=root/f"fm1/native/mi_piano_set1_{note}.c"
    txt=piano_path.read_text(encoding="utf-8")
    hexbytes=re.findall(r"0x[0-9a-f]{2}",txt)
    assert len(hexbytes)==9600
    assert hashlib.sha256(bytes(int(n,16) for n in hexbytes)).hexdigest()==piano_sha[note]
    assert f"mi_piano_set1_{note}[9600]" in txt
    assert f"fm1/native/mi_piano_set1_{note}.c" in (root / "fm1/native/SOURCE_ALLOWLIST.txt").read_text()
assert "fm1/native/test_mi_dsp_stress.c -o /tmp/mi_dsp_stress_test" in ci
assert "/tmp/mi_dsp_stress_test" in ci
assert "80000u" in (root / "fm1/native/test_mi_dsp_stress.c").read_text()
assert ci.index("/tmp/mi_dsp_stress_test") < ci.index("Reconstruct all 144 original")
assert "mi_slicer_tick(&a->slicer,out_l,out_r,tempo_step" in audio
assert "uint8_t *parameters=m->rec_track?" in audio
assert "m->rec_extra[m->rec_track-1u]:m->seq" in audio
assert "#define SAVED_MAGIC_M18" in ui and "#define SAVED_MAGIC_M17" in ui
assert "sizeof(saved_mi18_t)" in ui and "sizeof(saved_mi17_t)" in ui
assert 'fx_options[MI_FX_STORAGE_COUNT][2]' in (root / "fm1/native/mi_control.h").read_text()
assert 'fx_extra[MI_FX_STORAGE_COUNT][4]' in (root / "fm1/native/mi_control.h").read_text()
assert "mi_fx_slicer" not in audio  # only one SLICER instance
assert ci.index("/tmp/mi_rec_pcm7_test") < ci.index("Reconstruct all 144 original")
assert "drum_on(a,s,notes[t],112u,1u)" in audio
assert "drum_on(a,s,note,velocity,0u)" in audio
assert "if(v->drum_from_step)drum_seq+=drum" in audio
assert "else drum_manual+=drum" in audio
assert "sample=effect(a,rs,sample);" in audio
assert "sample=mi_soft_limit(sample+drum_manual);" in audio
assert "mi_rec_tick_stereo(&a->recorder,sample,rs->seq,&rec_l,&rec_r)" in audio
# Approved gain change: LOOP playback is unity on BOTH channels.
# Keep oscillator ducking and the rest of the mixer exactly as before.
assert "rec_l*(1.0f*a->rec_monitor_mix)+drum_seq" in audio
assert "rec_r*(1.0f*a->rec_monitor_mix)+drum_seq" in audio
assert "rec_l*(0.75f*a->rec_monitor_mix)+drum_seq" not in audio
assert "rec_r*(0.75f*a->rec_monitor_mix)+drum_seq" not in audio
assert "sample*(1.f-0.45f*a->rec_monitor_mix)+" in audio
assert audio.index("sample=mi_soft_limit(sample+drum_manual);") < \
       audio.index("mi_rec_tick_stereo(&a->recorder,sample,rs->seq,&rec_l,&rec_r)") < \
       audio.index("rec_l*(1.0f*a->rec_monitor_mix)+drum_seq")
assert "if(t->armed && ((t->clock_ticks-1u)%96u)==0u)" in recorder
# Real host screenshots plus flash-reboot/load assertions.
for name in ("MIYAKO_GLOBAL.png","MIYAKO_GLOBAL_MIDI.png",
             "MIYAKO_SAVE_LOAD.png"):
    assert name in ci and name in host_smoke
assert "expect save_mask 3" in host_smoke
assert "expect midi_channel 2" in host_smoke
assert "expect clock_mode 1" in host_smoke
assert "expect wave 5" in host_smoke
# The 240x240 startup image must expire before host OSC SUB=0/127
# screenshots are taken. Previously both captures were the same splash,
# and its yellow logo happened to pass the mailbox/person ROI count.
smoke = (root / "fm1/native/host_smoke.txt").read_text(encoding="utf-8")
assert "mi_ui_splash_until=plat_ms()+3000u;" in ui
assert "master 4096\n# Show the original startup artwork for three seconds before normal UI.\nwait 3200" in smoke
# The original SUB image must be captured after the two early preset
# reboot cases. Later feature regressions intentionally add MORE reboots
# (e.g. DEFAULT boot). Never assert a global exact reboot count or use
# rindex() over the entire growing simulator script.
sub_zero=smoke.index("shot MIYAKO_OSC_SUB_0.png")
before_sub=smoke[:sub_zero]
assert before_sub.count("reboot\nwait 3200") >= 2, (
    "original boot-and-preset checks must run before OSC SUB screenshots")
assert before_sub.rfind("reboot\nwait 3200") < before_sub.index(
    "tap HOME\nspin K2 -100\nexpect sub 0"), (
    "the second startup wait must precede the SUB=0 setup")
# Verify the later DEFAULT coverage directly; unrelated extra reboot cases
# must not invalidate the original startup-display regression.
default_case=smoke[smoke.index(
    "# DEFAULT is a separately confirmed SAVE-menu action"):]
assert "expect boot_slot 0\nexpect save_mask 3" in default_case
assert "expect boot_slot 1\nexpect save_mask 3" in default_case
assert "reboot\nwait 3200\nexpect view 0\nexpect wave 0" in default_case
assert "reboot\nwait 3200\nexpect wave 2\nexpect boot_slot 1" in default_case
assert "reboot\nwait 3200\nexpect wave 0\nexpect boot_slot 0" in default_case
assert smoke.index("shot MIYAKO_OSC_SUB_0.png") < smoke.index(
    "shot MIYAKO_OSC_SUB_127.png")
# Source-derived 72-frame high-detail 160x72 paper-airplane display,
# not the previous illegible 80x40 4x4 16-colour source. The original
# screenshot is owned by the user; no AI-generated images replace it.
letter_script = (root / "fm1/native/tools/build_letter_anim_header.py").read_text()
letter_data = root / "fm1/native/tools/mi_letter_highdetail_160x72.b64"
assert letter_data.is_file() and letter_data.stat().st_size > 16000
assert 'W, H = 160, 72' in letter_script
assert 'MAP_BYTES = (W // 4) * (H // 4)' in letter_script
assert 'FRAMES_PER_CLIP = 9' in letter_script
assert 'RAW_SHA256 = "86b64e4affac514ed05057b31087ddb28e9e830979524a1effc05e5dc702de51"' in letter_script
assert 'DECODED_CRC32 = 0x3141B600' in letter_script
assert 'checksum = zlib.crc32(bits, checksum)' in letter_script
assert '#include "mi_letter_anim.h"' in ui
assert 'static void mi_letter_map_decode(unsigned frame)' in ui
assert 'mi_letter_bases+clip*MI_LETTER_MAP_SIZE' in ui
assert 'mi_letter_books[clip*256u+mi_letter_tile_map[idx]]' in ui
assert 'uint16_t mask=mi_letter_books' in ui
assert 'int32_t top=48+(int32_t)y;' in ui
assert 'old?YELLOW:C_BLACK' in ui
assert 'state.sub*(MI_LETTER_FRAMES-1u)+63u' in ui
assert 'mi_draw_sub_scene();' in ui
assert 'mi_draw_letter_waveform();' in ui
assert 'audio.osc_scope[idx]*18/127' in ui
assert '"sub_frame"' in ui and '"sub"' in ui
assert 'mi_letter_anim.h' in (root / "fm1/native/verify_standalone.py").read_text()
assert "build_letter_anim_header.py --out /tmp/mi_letter_preflight.h" in ci
assert '--out /tmp/fm1-base/miyako/mi_letter_anim.h' in ci
assert "test_miyako_dynamic_version.py" in ci
assert "MIYAKO_OSC_SUB_0.png" in ci and "MIYAKO_OSC_SUB_127.png" in ci
assert "check_letter_quality.py" in ci
assert (root / "fm1/native/tools/check_letter_quality.py").is_file()
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
# MI20 / requested features: no legacy control or image bank removed.
control_h = (root / "fm1/native/mi_control.h").read_text()
control_test = (root / "fm1/native/test_mi_control.c").read_text()
recorder_h = (root / "fm1/native/mi_recorder.h").read_text()
recorder_test = (root / "fm1/native/test_mi_recorder.c").read_text()
assert "uint8_t rec_reverse[3];" in control_h
assert "uint8_t rec_quant[3];" in control_h
assert "s->rec_page=(uint8_t)((s->rec_page+1u)%3u);" in controls
assert 'transport[4]={"REV","QUANT","TRACK",""}' in controls
assert "s->rec_reverse[i]>1u||s->rec_quant[i]>2u" in controls
assert 'saved_mi19_t old19;' in ui
assert 'old19.magic==SAVED_MAGIC_M19' in ui
assert 'offsetof(mi_state_t,rec_reverse)' in ui
assert "mi_rec_set_transport(&a->recorder,k" in audio
assert "mi_rec_internal_start(&a->recorder)" in audio
assert "void mi_rec_internal_start(mi_recorder_t *r)" in recorder
assert "mi_rec_internal_start(&r);" in recorder_test
assert "static void quant_boundary(" in recorder
assert "if(t->reverse)" in recorder
assert "t->pending_play=t->playing?2u:1u;" in recorder
assert "r->beat_phase+=1.f;" in recorder
assert "quant_boundary(r,(r->tracks[0].clock_ticks-1u)/24u)" in recorder
assert "mi_rec_set_transport(" in recorder_h
assert "mi_rec_external_tick(&r,97000u);" in recorder_test
# A first take manually stopped within bar 1 must loop for one bar with
# unrecorded remainder silent; overdub and MIDI-clock timing remain intact.
assert "uint8_t tempo_ready,one_bar_loop;" in recorder_h
assert "t->one_bar_loop=1u;" in recorder
assert "t->tempo_ready=1u;" in recorder
assert "mi_memzero(clip_block(tr,block),MI_REC_PCM7_BLOCK_BYTES);" in recorder
assert "t->one_bar_loop?96.f:192.f" in recorder
assert "assert(r.count==22050u&&r.tracks[0].one_bar_loop==1u);" in recorder_test
assert "assert(mi_rec_sample(17000u)==0);" in recorder_test
assert "assert(r.count==11025u&&r.tracks[0].one_bar_loop==1u);" in recorder_test
# #310: after a real audio-block REC stop, an early first take is now
# padded to a full 1-BAR loop. The old <=55-point post-STOP test is invalid.
audio_rec_case = audio_test[audio_test.index(
    "/* A clip recording command is consumed only on an audio block boundary. */"):
    audio_test.index("/* Test captured PCM, silence, and selected MULTI OSC2. */")]
assert audio_rec_case.count("a.recorder.count<=55u") == 1, (
    "only the DURING-recording block may expect the short captured length")
assert "a.recorder.count==MI_REC_CAPACITY/2u" in audio_rec_case
assert "a.recorder.tracks[0].one_bar_loop==1u" in audio_rec_case
assert "assert(mi_rec_sample(1000u)==0);" in audio_rec_case
assert audio_rec_case.index("a.recorder.count<=55u") < audio_rec_case.index(
    "mi_audio_rec_request(&a,1u);", audio_rec_case.index(
    "mi_audio_rec_request(&a,1u);")+1), (
    "the obsolete short-count assertion must never follow the STOP command")
assert "mi_rec_cc(&state,note,velocity)" in ui
assert "mi_rec_cc(&s,30u,127u)" in control_test
assert "loop->playback_pos" in ui
assert "MIYAKO_LOOP_VISUAL_SYNC.png" in host_smoke
assert "test -s fm1-dist/MIYAKO_LOOP_VISUAL_SYNC.png" in ci
assert "a.arp_clock==period-256u" in audio_test
# Original user-supplied two montage sheets: all 162 complete 9x9
# 72x72 frames are decoded without alterations, then drawn at 160x160
# with nearest-neighbour coverage centred above the bottom controls.
# A 48x48 base plus a facial patch removed eyes in the old implementation.
arp_asset=(root/"fm1/native/tools/mi_arp_72_detail_source.b64")
arp_generator=(root/"fm1/native/tools/build_arp_anim_header.py").read_text()
audio_h=(root/"fm1/native/mi_audio.h").read_text()
host_smoke=(root/"fm1/native/host_smoke.txt").read_text()
assert arp_asset.is_file() and arp_asset.stat().st_size==34309
assert 'RAW_SHA256 = "a5b51a9a7da0dd3bb9080c6bde282a032cdd0b0730e295e7f4bdf157c1711c12"' in arp_generator
assert 'SOURCE = Path(__file__).with_name("mi_arp_72_detail_source.b64")' in arp_generator
assert "FRAME_COUNT = 162" in arp_generator
assert "FRAMES_INTRO = 81" in arp_generator
assert "W = H = 72" in arp_generator
assert "BLOCKS = 18 * 18" in arp_generator
assert "PACK_BYTES = 25730" in arp_generator
assert "read_and_verify()" in arp_generator
assert '#include "mi_arp_media.h"' in ui
assert '"mi_arp_media.h"' in native_audit
assert "mi_arp_decode(frame);" in ui
assert "mi_arp_current_map" in ui and "mi_arp_books[index]" in ui
assert "mi_arp_current_map[(y>>2)*18u+(x>>2)]" in ui
# User-requested ARP-specific 240x240 layout: original logo at the top,
# central sprite 160x160 across LCD halves, parameters below at y=199/218.
# Keep the 72x72 two-colour/162-frame animation SOURCE unchanged.
assert "#define MI_ARP_DRAW_SIZE 160u" in ui
assert "#define MI_ARP_DRAW_X 40u" in ui
assert "#define MI_ARP_DRAW_Y 30u" in ui
assert "static unsigned mi_arp_scaled_edge(unsigned source,unsigned extent)" in ui
assert "return (source*MI_ARP_DRAW_SIZE+extent-1u)/extent;" in ui
assert "static void mi_graph_arp_half(unsigned half)" in ui
assert "mi_graph_arp_half(half);" in ui
assert "const int32_t clip_top=(int32_t)half*120;" in ui
assert "const int32_t clip_bottom=clip_top+120;" in ui
assert "if(bottom<=clip_top||top>=clip_bottom)continue;" in ui
assert "if(top<clip_top)top=clip_top;" in ui
assert "if(bottom>clip_bottom)bottom=clip_bottom;" in ui
assert "(int32_t)(right-left),bottom-top,YELLOW);" in ui
assert "if(half==0u)mi_ui_draw_logo();" in ui
assert "cv_rect(8,72,224,1,DIM);" in ui
assert "cv_text(x,79,&FONT_XS,mi_param_name(&state,i),GREY);" in ui
assert "cv_text(x,98,&FONT_S,val,YELLOW);" in ui
assert "if(state.view==MI_ARP){" in ui and "mi_draw_arp();" in ui
assert "cv_rect(8,31,state.view==MI_ARP?104:224,1,DIM);" not in ui
assert "cv_rect(84+(int32_t)start,39+(int32_t)y," not in ui
assert "mi_graph_arp();" not in ui
# Physical UI geometry; no split row may escape the two 120px buffers.
assert 40+160<=240 and 30>=28 and 30+160<192
assert 192+6<199 and 218+16<=240
for y in range(72):
    top=30+(y*160+71)//72
    bottom=30+((y+1)*160+71)//72
    assert 0<=top<bottom<=190
    spans=[]
    for half in (0,1):
        lo=max(top,half*120)
        hi=min(bottom,(half+1)*120)
        if lo<hi:
            spans.extend(range(lo,hi))
    assert spans==list(range(top,bottom))
for x in range(72):
    left=40+(x*160+71)//72
    right=40+((x+1)*160+71)//72
    assert 40<=left<right<=200
assert "YELLOW);" in ui and "mi_arp_pixel(" in ui
assert "mi_arp_face_decode" not in ui
assert "mi_arp_face_deltas" not in arp_generator
assert "uint16_t mi_arp_cached_frame=MI_ARP_FRAME_COUNT" in ui
assert "static uint32_t shown_arp_visual_event=0xffffffffu" in ui
assert "shown_arp_visual_event!=audio.arp_visual_event" in ui
assert "volatile uint16_t arp_visual_frame;" in audio_h
assert "volatile uint32_t arp_visual_event;" in audio_h
assert "a->arp_visual_frame=mode?0xffffu:0u;" in audio
assert "a->arp_visual_frame>=161u" in audio
assert "a->arp_visual_frame=81u;" in audio
assert "++a->arp_visual_event;" in audio
assert "assert(a.arp_visual_frame==81u" in audio_test
assert "expect arp_visual_frame >0" in host_smoke
assert "expect arp_visual_frame 0" in host_smoke
assert "build_arp_anim_header.py --out /tmp/mi_arp_preflight.h" in ci
assert "--out /tmp/fm1-base/miyako/mi_arp_media.h" in ci
assert 'test -s /tmp/fm1-base/miyako/mi_arp_media.h' in ci
assert "MI_ARP_FRAME_COUNT 162u" in ci
assert ci.index("build_arp_anim_header.py --out /tmp/mi_arp_preflight.h") < ci.index(
    "python3 fm1/native/test_pipeline_contract.py")
assert "MIYAKO-FM1-NATIVE-DEV-res.bin" in ci
# Repeat the manual gating and branch controls after introducing the assets.
assert "when: << pipeline.parameters.run_build >>" in ci
assert "only: " + checkout_branches[0] in ci
print("PASS: manual Phase 3 REC + EUREKA 2sec/576 frames + guarded dual-FX")
