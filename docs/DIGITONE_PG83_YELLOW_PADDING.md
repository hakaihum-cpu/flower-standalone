# PG83 — Digitone EUREKA yellow letterbox (SOURCE ONLY)

- Uses the 574 user-provided tile crops (two exact repeats), 576 frames in 16 scenes, 96 seek records.
- The #04 HYBRID image algorithm, the bitwise XOR/RLE stream format and the device's EUREKA rendering/audio/MIDI code remain unchanged.
- All supplied JPEG crops are contained at 64 px full height with variable centered widths; their original in-image 1bit pixels are preserved **bit for bit**.
- The only visual difference is the outside columns: set all their LCD pixels to 1, i.e. **yellow light** rather than black. 2,142,720 border pixel positions are verified.
- The offline generator reads the existing PG82 packed stream and patches only the side columns according to an independently measured SHA-pinned width table; it builds new 96 seekpoints and verifies each result.
- Expected packed 1bit frame stream SHA-256: bd7577db4d789f3a6b5bbae819cd6f5d5604eb4851d0dcf0d7f6a2ac67d4a411; new reference frame SHA-256: 0fe423f513638566d8505ce6ae30fb09cd22c2335b13e297317fe9dedbd9bc64; new seek SHA-256: 1866938edb4abef475e01e20ffa8a096122637882369173b3c65585476fa8b6d.
- **No SYX build performed**. EUREKA sound FX still freezes in PG82 on device; this aspect/padding revision does not resolve that separate fault.
