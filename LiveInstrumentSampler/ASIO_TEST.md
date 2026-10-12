# ASIO Verification Checklist

A build is not ASIO-verified until all five checks pass on the target Windows machine and interface.

1. **Driver enumerate** — ASIO appears and the intended driver is listed.
2. **48 kHz open** — selected input/output channels open at 48000 Hz and the requested buffer.
3. **Input signal** — during the three-second test the user plays the instrument and the app detects non-zero real input.
4. **Output tone** — the app plays JUCE's device test tone through the selected output pair and the user confirms it was physically heard.
5. **Reopen** — the device is closed and reopened with the same selected setup successfully.
6. **Recording integrity** — make a real REC/STOP take. Writer FIFO dropouts must be 0 and device xruns during the take must be 0; otherwise SET must remain disabled and RETAKE must be required.

Record the driver name, interface name, MONO/STEREO input selection, MONO/STEREO output selection, buffer, and PASS/FAIL result when testing a release build.
