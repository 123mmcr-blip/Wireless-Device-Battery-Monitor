# HyperX Pulsefire Haste Wireless (original)

This is **not** the Pulsefire Haste 2 wireless protocol. The original device uses NGENUITY2 64-byte unnumbered HID interrupt reports, rather than Haste 2's `50 02` request / `51 02` reply.

## Hardware identification

- `03F0:028E`: original Haste Wireless 2.4 GHz receiver (identified by the OpenMouse project; not yet tested on this PC).
- `03F0:048E`: original Haste Wireless over USB cable (observed on LaNae, Windows, 2026-11-19).
- On `048E`, `BatteryHub.exe --dump` reported interface 2, usage `FF00:0001`, input/output lengths 65 bytes, feature length 0; USB reported product `HyperX Pulsefire Haste Wireless`.

## Battery query

On the vendor collection (`FF00:0001`), send a 64-byte **output** payload with opcode `0x51` followed by zeros and wait for a 64-byte **input** payload echoing `0x51`:

- Response byte 4: battery percentage (`1..100`, with `0`/`0xFF` treated as unknown).
- Response byte 5: `0` discharging, `1` charging, `2` fully charged on cable.
- `HidDev::Write` follows hidapi conventions, so the driver passes `{ 0x00, 0x51 }`: the leading `0` is the unnumbered HID report ID, **not** an extra payload byte.
- Only the read-only getter `0x51` is sent. Never send profile/DPI/settings commands during battery polling.
- If there is no matching reply within 800 ms, do not claim the device.

## Sources and validation

- [OpenMouse protocol codec](https://github.com/OpenMouse-Project/mouse-protocol/blob/main/src/hyperx/index.ts) and [HID client](https://github.com/OpenMouse-Project/mouse-protocol/blob/main/src/drivers/hyperx/hid.ts), with attribution there to [open-pulsefire-haste](https://github.com/evan-razzaque/open-pulsefire-haste).
- Real hardware confirmed: Windows device detection and HID report descriptors for `048E`. The battery command itself needs a live response and comparison with NGENUITY before submitting upstream.
- Test both receiver and wired modes; the vendor protocol may return no battery percentage while connected by USB cable.
