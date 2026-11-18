# DARKRIM SHADOW / 寸渊影弑 receiver

Hardware-tested on `3654:6901`, manufacturer `Jieli Technology`, release `0100`.
Use interface **3**, usage `FF02:0002`, unnumbered 64-byte input/output payloads
(65-byte Windows HID report sizes). Interface 4 also has `FF02:0002`, but uses
report ID 2 and did not answer this protocol; it is deliberately excluded.

Protocol source: the official [PC Hub download](https://app.darkrim.net/hub/zh.html),
`DARKRIM-0.1.26-x64-setup.rar`, downloaded and statically unpacked without
installing or launching the vendor software. The extracted `DARKRIM.exe` SHA-256:
`f20802587632dd3b36d0d602324931bc62174d55524d71277ab7a3c3ebecd86d`.

Useful addresses in that binary:

- `140124C80`: `sendGetBasicInfo(infoType)` sends command `0010`, Request `01`,
  payload `[infoType, 00]`. Battery selector is `01`.
- `140118D30`, `140119200`: frame header and CRC generation.
- `140126DB0`: device-info TLV parser; TLV `01` is battery.
- `1400F5660`, `1400FA700`: battery consumers; low seven bits give percentage,
  high bit is charging. The same battery value also arrives in `0011` Notify `03`.

Header: `FE seq cmdLE type index count lengthLE`, followed by payload and
CRC-16/CCITT-FALSE (polynomial `1021`, initial `FFFF`, stored big-endian).
The sequence is a nonzero four-bit counter; encryption is unused.

Actual request/reply payloads, before HID zero padding:

```text
GET battery:  FE 03 10 00 01 00 01 02 00 01 00 9B 15
Response:     FE 03 10 00 02 00 00 03 00 01 01 28 96 0D
Notification: FE 00 11 00 03 00 00 03 00 01 01 28 C6 61
```

The response TLV `01 01 28` gives **40%, not charging**. The production C++ driver
returned that value on three consecutive queries without vendor software.
The receiver emits startup notifications before the matching response, measured
at about 406 ms. Use one 800 ms response deadline (plus at most 200 ms for writing),
without restarting it for unrelated notifications. No heartbeat is required.

The driver sends only the battery GET. It accepts only an unencrypted response
with the matching sequence/command/type, index zero, count zero or one (the
hardware uses zero), valid length/CRC, and a single well-formed battery TLV.
Fragmented, unsolicited, malformed and out-of-range replies are ignored.

Charging transitions, low/full boundaries, headset power-off behavior, Bluetooth
and USB cable mode have not been hardware-tested. No claim of support is made
for those modes. No firmware updates, configuration changes, EQ commands,
pairing changes, resets or vendor binaries are included in this implementation.

Test: `g++ -std=c++17 tests/darkrim_driver_test.cpp src/drivers/darkrim.cpp -o build/darkrim_driver_test.exe`.
