# FLOWER Android Diagnostics

This diagnostic path is intentionally designed not to disturb the real-time audio path.

## Runtime policy

- No file I/O from `processBlock`.
- No per-sample or per-audio-block logging.
- No custom signal handler.
- No watchdog thread.
- No continuous high-rate touch logging.
- App breadcrumbs are emitted only from the UI/message thread at low frequency:
  - editor ready,
  - touch down,
  - touch up,
  - root / BPM / scale physical-key changes,
  - HOLD,
  - STOP.

On Android, JUCE `Logger::writeToLog()` falls back to the platform debug output when no custom logger is installed. JUCE 9.0.2 maps that Android debug output to logcat.

This means the APK does not continuously write its own diagnostic file. Android's existing log buffers remain the primary crash/ANR source.

## Windows collection BAT

Run:

`tools\diagnostics\COLLECT_FLOWER_ANDROID.bat`

The BAT:

1. verifies `adb`,
2. waits for an Android device,
3. clears old logcat content,
4. lets the tester reproduce the issue,
5. collects:
   - full logcat,
   - Android crash buffer,
   - app exit-info,
   - Activity state,
   - device properties,
   - a FLOWER-focused filtered view.

Logs are stored under:

`tools\diagnostics\logs\YYYYMMDD_HHMMSS\`

## Performance impact

The diagnostic design deliberately avoids the audio thread.

Expected steady-state effect during normal playback is effectively zero except for the existing atomic state operations. A log write occurs only on discrete UI events, not while the finger is continuously dragging and not from the DSP loop.

If a timing-sensitive issue later requires more detail, add a temporary higher-detail diagnostic mode on a separate branch rather than making normal builds verbose.
