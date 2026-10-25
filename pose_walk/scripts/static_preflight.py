#!/usr/bin/env python3
"""POSE WALK build preflight; pure stdlib, runs before any SDK/Gradle download."""
from pathlib import Path
import base64
import json
import re
import struct
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parent.parent
assets = root / "app/src/main/assets/index.html"
html = assets.read_text(encoding="utf-8")

assert len(html) > 40000, "The game HTML appears truncated"
assert html.startswith("<!doctype html>"), "Missing HTML doctype"
assert '<canvas id="stage"' in html, "Missing stage canvas"
assert "window.poseWalkDebug" in html, "Missing game control hook"
assert "requestAnimationFrame(loop)" in html, "Missing drawing loop"
assert all(f'id="{x}"' in html for x in (
    "move-left", "move-right", "previous", "next", "random", "reset",
    "pose-strip", "stage", "frame-status")), "Missing one or more game controls"

block = re.search(r"<script>window\.POSES=(\[.*?\]);</script>", html, flags=re.S)
assert block, "Missing embedded POSES data"
poses = json.loads(block.group(1))
assert len(poses) == 28, f"Expected 28 poses, found {len(poses)}"
assert [p["id"] for p in poses] == list(range(28)), "Nonsequential pose IDs"
assert all(p["group"] in {"stand", "sit", "floor", "walk"} for p in poses)
assert len({p["name"] for p in poses}) == 28, "Duplicate pose names"
for pose in poses:
    name = pose["name"]
    src = pose["src"]
    assert src.startswith("data:image/png;base64,"), f"Not embedded PNG: {name}"
    raw = base64.b64decode(src.split(",", 1)[1], validate=True)
    assert raw[:8] == b"\x89PNG\r\n\x1a\n", f"Invalid PNG header: {name}"
    assert raw[12:16] == b"IHDR", f"Invalid PNG IHDR: {name}"
    w, h = struct.unpack(">II", raw[16:24])
    assert w == pose["width"] and h == pose["height"], f"PNG dimension mismatch: {name}"
    assert w > 0 and h > 0, f"Zero sized PNG: {name}"

script_blocks = re.findall(r"<script[^>]*>(.*?)</script>", html, flags=re.I | re.S)
assert len(script_blocks) == 3, f"Expected 3 game script blocks, found {len(script_blocks)}"
assert all(s.strip() for s in script_blocks)
cycle_match = re.search(r"<script>window\.WALK_CYCLE=(\[.*?\]);</script>", html, flags=re.S)
assert cycle_match, "Missing pre-rendered WALK_CYCLE frames"
cycle = json.loads(cycle_match.group(1))
assert len(cycle) == 12, f"Expected 12 walking frames, got {len(cycle)}"
for i, pose in enumerate(cycle):
    assert pose["width"] == 44 and pose["height"] == 96, f"Wrong walking dimensions at {i}"
    raw = base64.b64decode(pose["src"].split(",", 1)[1], validate=True)
    assert raw[:8] == b"\x89PNG\r\n\x1a\n", f"Invalid walking PNG at {i}"
    w, h = struct.unpack(">II", raw[16:24])
    assert (w, h) == (44, 96), f"Walking PNG dimensions incorrect at {i}"
assert len({p["src"] for p in cycle}) >= 7, "Generated interpolation images are missing"
assert "let flip=moving?(state.dir<0)" in html, "Walking images not mirrored only when moving left"
assert "im=moving?walkFrames[frameIndex]:imgs[poseIndex]" in html, "Walking animation is not using all interpolated images"
assert "Math.floor(state.walkClock*12)%walkFrames.length" in html

java = (root / "app/src/main/java/com/posewalk/studio/MainActivity.java").read_text()
assert "package com.posewalk.studio;" in java
assert 'browser.loadUrl("file:///android_asset/index.html")' in java
assert "webSettings.setJavaScriptEnabled(true);" in java
assert "browser.destroy();" in java
manifest = ET.parse(root / "app/src/main/AndroidManifest.xml").getroot()
app = manifest.find("application")
assert app is not None
android = "{http://schemas.android.com/apk/res/android}"
assert app.attrib.get(android + "label") == "POSE WALK"
activity = app.find("activity")
assert activity is not None and activity.attrib.get(android + "exported") == "true"
assert manifest.find("uses-permission") is None, "Unexpected Android permission"

build = (root / "build.gradle").read_text()
app_gradle = (root / "app/build.gradle").read_text()
settings = (root / "settings.gradle").read_text()
assert "version '8.7.3'" in build, "Unpinned or changed Android Gradle Plugin"
assert "compileSdk 35" in app_gradle and "targetSdk 35" in app_gradle
assert "minSdk 24" in app_gradle
assert "applicationId 'com.posewalk.studio'" in app_gradle
assert "include ':app'" in settings
assert not (root / "app/src/main/cpp").exists(), "Unwanted JNI/C++ source"
print("[PASS] 28 original poses + 12 interpolated walk frames / directions / Android configuration")
