# Replay input log format

`neschan` now supports deterministic input replay via:

```bash
neschan <rom_file_path> --replay <input_log> --headless --max-frames <n>
```

Input log format is plain text, one sample per line:

```
<frame_index> <button_flags>
```

- `frame_index`: decimal frame number (0-based)
- `button_flags`: 8-bit mask (`0x00` - `0xFF`, hex or decimal)

Button bit layout follows `nes_button_flags` (MSB to LSB):

- `0x80` A
- `0x40` B
- `0x20` Select
- `0x10` Start
- `0x08` Up (W)
- `0x04` Down (S)
- `0x02` Left (A)
- `0x01` Right (D)

Example:

```
# frame flags
0   0x00
1   0x08   # hold UP (W)
2   0x08
10  0x01   # tap RIGHT (D)
```

Frames not explicitly listed default to `0x00`.
