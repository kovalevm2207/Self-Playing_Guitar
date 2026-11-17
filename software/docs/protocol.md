# USB score protocol v1

Serial: 115200 baud, 8 data bits, no parity, one stop bit, no flow control. ASCII lines terminated with LF; CR is ignored. Commands and responses are case sensitive. Host sends one command and waits for its response before sending the next. No automatic retries: reconnect after a timeout.

| Host → Uno | Uno → host | Meaning |
|---|---|---|
| `HELLO 1` | `READY 1 96 SIMULATED 1` | Version, capacity, mode, supported-string bitmask; stops execution and invalidates previous score |
| `BEGIN <count> <crc>` | `OK BEGIN` | Starts a new score; invalidates previous score |
| `NOTE <index> <string> <fret> <duration>` | `OK <index>` | Zero-based sequential index; string 1–6; fret 0–24; duration 50–10000 ms |
| `END` | `OK END` | Marks score loaded only if count and CRC match |
| `ABORT` | `OK ABORT` | Discards score |
| `PLAY` | `OK PLAY` | Validates the complete motion plan and starts simulated execution |
| `STOP` | `OK STOP` | Stops execution and releases outputs; preserves the loaded score |
| `STATUS` | `STATUS <state> <index> <count> SIMULATED` | State and zero-based current note index |

All integers are unsigned decimal. Count is 1–96. CRC is CRC-16/CCITT-FALSE: polynomial `0x1021`, initial value `0xffff`, no reflection, xorout 0. For each note append four bytes in this order: string, fret, duration low byte, duration high byte. Index is not part of the CRC. Reference string `123456789` yields `0x29b1`.

The optional final READY field is a decimal bitmask in 1–63: bit 0 is string 1 (high E4), bit 5 is string 6. The current bench advertises 1, so every note must use string 1. A value of 63 represents all six strings. Missing masks from older firmware are interpreted as 63 for compatibility, without verification of the physical bench. Unsupported strings are rejected by the app before BEGIN and independently by the firmware with `ERR STRING`, invalidating the upload.

Uno emits `BOOT 1` on reset. Host opens the port, asserts DTR and waits 2.2 seconds for reset before HELLO. Each command has a host timeout of 3 seconds. Uno discards an incomplete transfer or line after 5 seconds without incoming bytes. A line exceeding 79 bytes is discarded through LF. Invalid commands, count/order/ranges/CRC produce `ERR <reason>` and invalidate the score. Receiver memory is fixed; no heap allocation is used.

The old `READY 1 96 DRYRUN` and four-field `READY 1 96 SIMULATED` receivers remain supported by the desktop app; playback is enabled only for the SIMULATED capability. An older app which rejects the additional string mask needs updating for this firmware.

An incomplete upload expires after the receiver timeout or is discarded by the next HELLO. Cancel during upload closes the port. During playback, STOP is explicit; the app waits for OK STOP before a requested disconnect. Closing USB alone does not guarantee a stop or reset. Application shutdown sends STOP on a best-effort basis without waiting for its acknowledgement.

The controller emits `EVENT DONE` once on completion or `EVENT FAULT TIMING` once if the execution loop misses a deadline. STATUS states are EMPTY, LOADED, PLAYING, DONE and FAULT. STOP is idempotent. PLAY without a verified score returns `ERR NOT_LOADED`; an infeasible sequence returns `ERR PLAN` without discarding the score. While playing, commands other than HELLO/STOP/ABORT/STATUS return `ERR BUSY` and preserve the active score. Line overflow or an input timeout stops execution and invalidates the score.

SIMULATED uses virtual STEP/DIR, clamp and pick lines and never configures or writes physical GPIO. The profile's positions and times are artificial. The actuator bank retains six virtual channels for testing, but the default bench profile enables only string 1. The virtual channels are not a wiring map for Uno.

Note duration is the interval between scheduled pick onsets; the final note's duration determines completion time. A preparation pre-roll precedes the first pick. Later release/move/settle/clamp phases consume the tail of the preceding interval. This model does not promise full-duration sustain or physical damping. PLAY preflights each transition, including the preceding pick pulse, before enabling any output. Deadlines use wrap-safe 32-bit microsecond arithmetic; late execution faults instead of replaying missed pulses in a burst.

This baseline uses constant-speed stepping serviced by the main loop. Physical playback still requires a calibrated machine profile, homing and travel limits, acceleration/deceleration, a hardware timer pulse backend, measured actuator delays and duty-cycle limits, and real pin allocation. Streaming and chords are not implemented; the whole score still fits in the 96-note receiver buffer.
