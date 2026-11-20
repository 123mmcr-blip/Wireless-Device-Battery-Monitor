# HyperX Pulsefire Dart battery protocol

Read by `PulsefireDart` in `src/drivers/hyperx.cpp`. The protocol comes from community reverse engineering of
NGENUITY:

- [santeri3700/hyperx_pulsefire_dart_reverse_engineering](https://github.com/santeri3700/hyperx_pulsefire_dart_reverse_engineering) (CC0), `protocol/index.md`
- [radoslavchobanov/PlasmaNGenuity](https://github.com/radoslavchobanov/PlasmaNGenuity) (MIT), an implementation based on those notes, `plasmangenuity/drivers/hyperx/protocol.py`

Read on hardware (Windows, 2026-11-20): a `0951:16E1` dongle answered the heartbeat with 74%. The level has not
yet been compared with NGENUITY, and the charging flag has not yet been observed.

## Device

| Product | USB ID | Interface | Usage page : usage | Input / output / feature (Windows) |
|---|---|---|---|---|
| Pulsefire Dart, 2.4 GHz dongle | `0951:16E1` | 2 | `FF00:0001` | 65 / 65 / 65 bytes |
| Pulsefire Dart, USB cable | `0951:16E2` | | `FF13` (not handled) | |

The 65-byte Windows lengths are the 64-byte reports plus the report ID byte. The dongle also exposes the standard
keyboard (interface 1), mouse (interface 0) and consumer-control collections. The driver only opens the `FF00`
collection.

## Query

| Request | Reply | Meaning |
|---|---|---|
| `51` (heartbeat) | `51 .. .. .. <percent> <charging> ..` | `[4]` battery 0..100, `[5]` 1 = charging |

The driver writes report ID `00` followed by `51`, so the mouse receives `51 00 00 ...`. The reply is read
with a one-second limit and unrelated reports are skipped. Bytes `[6..10]` of the reply are undocumented
(the notes guess that `[7..8]` might be a voltage), so no voltage is reported.

The reply carries no connection or sleep state (the notes list a separate connectivity report `46` and a
sleep event `FF`, with no documented request). The sources don't say what the dongle answers while the mouse is
off or asleep: without a reply nothing is listed, but a dongle that answers with a cached level would show it.

## Still to check on hardware

- That `[4]` matches the level NGENUITY shows.
- That `[5]` is 1 while the mouse is on its charger.
- What the dongle answers while the mouse is switched off or asleep (no reply, or a stale level).
