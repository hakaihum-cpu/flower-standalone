#!/usr/bin/env python3
from pathlib import Path
import csv,sys,xml.etree.ElementTree as ET
R=Path(__file__).resolve().parents[1]
required=['RealtimeChordFX_Standalone.jucer','Source/PluginProcessor.cpp','Source/PluginEditor.cpp','Source/TheoryEngine.cpp','Source/YinPitchDetector.cpp','Source/GranularPitchBank.cpp','Source/FramePack.cpp','Resources/classroom_frames.pack','Resources/classroom_frames_manifest.csv','PROJECT_CONTRACT.md','BUILD_HISTORY.csv']
for x in required:
    if not (R/x).is_file(): raise SystemExit(f'[FAIL] missing {x}')
# Exact supplied-frame contract.
rows=list(csv.DictReader((R/'Resources/classroom_frames_manifest.csv').open(encoding='utf-8')))
if len(rows)!=300: raise SystemExit(f'[FAIL] expected 300 supplied frames, got {len(rows)}')
pack=(R/'Resources/classroom_frames.pack').read_bytes()
if pack[:4]!=b'CRF1': raise SystemExit('[FAIL] frame pack magic mismatch')
# No synth path: this product transforms input audio only.
src='\n'.join(p.read_text(errors='ignore') for p in (R/'Source').glob('*') if p.suffix in {'.h','.cpp'})
for bad in ['SynthVoice','juce::Synthesiser','SineVoice','FLOWER','MIYAKO']:
    if bad in src: raise SystemExit(f'[FAIL] forbidden cross-project/synth token in Source: {bad}')
for need in ['COMPLEX','BAR','WIDTH','LENGTH','iRig Streamer','DETECTED','● REC']:
    if need not in src: raise SystemExit(f'[FAIL] UI/requirement token missing: {need}')

for need in [
    'setResizable (true, false)',
    'getAndroidPhysicalInputNames',
    'android/media/AudioManager',
    'android/media/AudioDeviceInfo',
    'getProductName',
    'GET_DEVICES_INPUTS',
    'physicalInputNames',
]:
    if need not in src:
        raise SystemExit(f'[FAIL] Android input/fullscreen contract missing: {need}')
root=ET.parse(R/'RealtimeChordFX_Standalone.jucer').getroot()
if root.attrib.get('pluginFormats')!='buildStandalone': raise SystemExit('[FAIL] standalone-only contract broken')
if root.attrib.get('pluginIsSynth')!='0': raise SystemExit('[FAIL] synth flag must be off')
if root.attrib.get('pluginWantsMidiIn')!='1': raise SystemExit('[FAIL] MIDI input required for clock/control')
if 'Resources/classroom_frames.pack' not in (R/'RealtimeChordFX_Standalone.jucer').read_text(): raise SystemExit('[FAIL] supplied frame pack not embedded')
ci=(R/'.circleci/config.yml').read_text()
for need in ['run_build','default: false','CMAKE_BUILD_PARALLEL_LEVEL=2','--max-workers=2','patch_android_native_parallelism.py']:
    if need not in ci: raise SystemExit(f'[FAIL] CI guard missing: {need}')
print('[PASS] 300 supplied images are the embedded visual bank')
print('[PASS] no synth implementation or FLOWER/MIYAKO source dependency')
print('[PASS] manual CircleCI gate and native parallelism controls present')
print('[PASS] required effect/UI/config contract tokens present')
print('[PASS] Android physical-input probe and fullscreen sizing contract present')