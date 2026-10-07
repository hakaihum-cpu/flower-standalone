# Speed Editor HID Check v0.2

This branch no longer treats the DaVinci Resolve Speed Editor HID protocol as unknown.
It implements the published reverse-engineered protocol and only validates it on Android USB Host / RG Rotate before EP-SAMPLE integration.

## Expected protocol
- USB VID `0x1EDB`, PID `0xDA0E`
- Feature Report 6: authentication
- Input Report 3: Search Dial (`u8 mode`, signed little-endian `i32 value`, trailing byte)
- Input Report 4: six little-endian `u16` held-key codes
- Input Report 7: charging state and battery percentage
- Complete 43-key map embedded in the app

Reference implementation: Sylvain Munaut, `blackmagic-misc/bmd.py` and `speed-editor-demo.py` (Apache-2.0).

## Test flow
1. Connect Speed Editor by USB-C and approve Android USB permission.
2. Confirm `AUTH: OK`.
3. Press any representative keys; the app should show their known names immediately.
4. Release SHTL/JOG/SCRL or use the on-screen mode buttons, then turn the Search Dial.
5. Confirm the dial mode/value changes.
6. Export TXT only when a mismatch occurs or after a short representative test for review.

Known demo dial mapping is reproduced:
- SHTL -> mode 2 `RELATIVE_2`
- JOG -> mode 1 `ABSOLUTE_CONTINUOUS`
- SCRL -> mode 3 `ABSOLUTE_DEADZERO`

The probe uses a separate application ID and does not replace EP-SAMPLE.