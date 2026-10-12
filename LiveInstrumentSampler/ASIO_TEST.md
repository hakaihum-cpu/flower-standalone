# ASIO Verification Checklist

A build is not ASIO-verified until all five checks pass on the target Windows machine and interface.

1. **Driver enumerate** — ASIO appears and the intended driver is listed.
2. **48 kHz open** — selected input/output channels open at 48000 Hz and the requested buffer.
3. **Input signal** — during the three-second test the user plays the instrument and the app detects non-zero real input.
4. **Output tone** — the app plays JUCE's device test tone through the selected output pair and the user confirms it was physically heard.
5. **Reopen** — the device is closed and reopened with the same selected setup successfully.

Record the driver name, interface name, input pair, output pair, buffer, and PASS/FAIL result when testing a release build.
