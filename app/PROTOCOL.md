# USB score protocol v1

Serial: 115200 baud, 8 data bits, no parity, one stop bit, no flow control. ASCII lines terminated with LF; CR is ignored. Commands and responses are case sensitive. Host sends one command and waits for its response before sending the next. No automatic retries: reconnect after a timeout.

| Host → Uno | Uno → host | Meaning |
|---|---|---|
| `HELLO 1` | `READY 1 96 DRYRUN` | Protocol version, note capacity, receiver-only mode; invalidates previous score |
| `BEGIN <count> <crc>` | `OK BEGIN` | Starts a new score; invalidates previous score |
| `NOTE <index> <string> <fret> <duration>` | `OK <index>` | Zero-based sequential index; string 1–6; fret 0–24; duration 50–10000 ms |
| `END` | `OK END` | Marks score loaded only if count and CRC match |
| `ABORT` | `OK ABORT` | Discards score |

All integers are unsigned decimal. Count is 1–96. CRC is CRC-16/CCITT-FALSE: polynomial `0x1021`, initial value `0xffff`, no reflection, xorout 0. For each note append four bytes in this order: string, fret, duration low byte, duration high byte. Index is not part of the CRC. Reference string `123456789` yields `0x29b1`.

Uno emits `BOOT 1` on reset. Host opens the port, asserts DTR and waits 2.2 seconds for reset before HELLO. Each command has a host timeout of 3 seconds. Uno discards an incomplete transfer or line after 5 seconds without incoming bytes. A line exceeding 79 bytes is discarded through LF. Invalid commands, count/order/ranges/CRC produce `ERR <reason>` and invalidate the score. Receiver memory is fixed; no heap allocation is used.

The app's cancel closes the port and stops sending. An incomplete score expires after the receiver timeout or is discarded by the next HELLO. Closing USB alone does not guarantee an immediate board reset.

This version intentionally has no PLAY command. DRYRUN reports verified note storage, not physical playback. Before adding playback, define a calibrated motion scheduler, channel capability negotiation, travel limits and actuator timing.
