#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: patch_juce_android_gamepad_keys.py <JUCE root>")

juce_root = Path(sys.argv[1]).resolve()
target = juce_root / "modules/juce_gui_basics/native/juce_Windowing_android.cpp"

if not target.is_file():
    raise SystemExit(f"[FAIL] JUCE Android windowing source not found: {target}")

text = target.read_text(encoding="utf-8")

case_anchor = """        case 22:  return KeyPress::rightKey;          // KEYCODE_DPAD_RIGHT
"""
case_insert = """        case 22:  return KeyPress::rightKey;          // KEYCODE_DPAD_RIGHT
        case 96:  return KeyPress::F13Key;            // KEYCODE_BUTTON_A
        case 97:  return KeyPress::F14Key;            // KEYCODE_BUTTON_B
        case 99:  return KeyPress::F15Key;            // KEYCODE_BUTTON_X
        case 100: return KeyPress::F16Key;            // KEYCODE_BUTTON_Y
        case 102: return KeyPress::F17Key;            // KEYCODE_BUTTON_L1
        case 103: return KeyPress::F18Key;            // KEYCODE_BUTTON_R1
"""

if "KEYCODE_BUTTON_A" not in text:
    if case_anchor not in text:
        raise SystemExit("[FAIL] Android key translation anchor changed")
    text = text.replace(case_anchor, case_insert, 1)

old_callbacks = """    static bool handleKeyDownCallback (JNIEnv*, AndroidComponentPeer& t, int k, int kc, int kbFlags)
    {
        ModifierKeys::currentModifiers = ModifierKeys::currentModifiers.withOnlyMouseButtons()
                                                                       .withFlags (translateAndroidKeyboardFlags (kbFlags));
        return t.handleKeyPress (translateAndroidKeyCode (k), static_cast<juce_wchar> (kc));
    }

    static void handleKeyUpCallback (JNIEnv*, [[maybe_unused]] AndroidComponentPeer& t, [[maybe_unused]] int k, [[maybe_unused]] int kc)
    {
    }
"""

new_callbacks = """    static bool handleKeyDownCallback (JNIEnv*, AndroidComponentPeer& t, int k, int kc, int kbFlags)
    {
        ModifierKeys::currentModifiers = ModifierKeys::currentModifiers.withOnlyMouseButtons()
                                                                       .withFlags (translateAndroidKeyboardFlags (kbFlags));

        bool used = false;
        used |= t.handleKeyUpOrDown (true);
        used |= t.handleKeyPress (translateAndroidKeyCode (k), static_cast<juce_wchar> (kc));
        return used;
    }

    static void handleKeyUpCallback (JNIEnv*, AndroidComponentPeer& t, [[maybe_unused]] int k, [[maybe_unused]] int kc)
    {
        t.handleKeyUpOrDown (false);
    }
"""

if "used |= t.handleKeyUpOrDown (true);" not in text:
    if old_callbacks not in text:
        raise SystemExit("[FAIL] Android key callback anchor changed")
    text = text.replace(old_callbacks, new_callbacks, 1)

target.write_text(text, encoding="utf-8")

for required in [
    "KEYCODE_BUTTON_A",
    "KEYCODE_BUTTON_B",
    "KEYCODE_BUTTON_X",
    "KEYCODE_BUTTON_Y",
    "KEYCODE_BUTTON_L1",
    "KEYCODE_BUTTON_R1",
    "used |= t.handleKeyUpOrDown (true);",
    "t.handleKeyUpOrDown (false);",
]:
    if required not in text:
        raise SystemExit(f"[FAIL] JUCE Android key patch missing: {required}")

print("[PASS] JUCE Android dpad/gamepad key bridge patched")
