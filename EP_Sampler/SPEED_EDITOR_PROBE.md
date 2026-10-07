# SpeedEditor Probe v0.1

Diagnostic-only branch for DaVinci Resolve Speed Editor USB-HID discovery before EP-SAMPLE integration.

- Based on Golden `1799e92fae7ff1b0b890c78af640d204d9eb67b0`.
- Golden/main are not changed.
- Separate application id: `com.example.epmodel.speededitorprobe`, so it installs alongside EP-SAMPLE.
- Guides 43 physical buttons, then JOG/SCRL/SHTL Search Dial clockwise/counter-clockwise tests.
- Records USB descriptors, interface/endpoints, auth feature reports, all raw HID reports, parsed keys, dial mode/value, battery reports, PASS/SKIP results.
- EXPORT TXT uses Android Storage Access Framework. Return that TXT to ChatGPT for mapping analysis.

Protocol attribution: Android implementation derived from Sylvain Munaut's Apache-2.0 `blackmagic-misc/bmd.py` reverse engineering, with official Blackmagic Speed Editor control labels/manual used for the guided order.