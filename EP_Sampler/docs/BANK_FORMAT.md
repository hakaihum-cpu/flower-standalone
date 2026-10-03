# EPBANK1 format

The app does not need 2,112 individual WAV files at runtime. `tools/build_bank.py` packs the captured material into one memory-mapped `epbank.bin` so MIDI note-on does not open or decode files on the realtime audio thread.

Little-endian header (40 bytes):

- `char magic[8]` = `EPBANK1\0`
- `uint32 version` = 1
- `uint32 sampleRate`
- `uint32 channels` = 2
- `uint32 bitsPerSample` = 16 or 24
- `uint32 entryCount`
- `uint32 reserved`
- `uint64 dataOffset`

Each index entry is 24 bytes:

- `uint8 note`
- `uint8 velocity`
- `uint8 rr` (1..3)
- `uint8 part` (0 sustain, 1 release)
- `uint32 frames`
- `uint64 offset`
- `uint64 bytes`

Raw interleaved stereo PCM follows the index. The Android native engine `mmap()`s the bank and reads only the currently sounding voices.
