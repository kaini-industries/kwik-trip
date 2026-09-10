# Infrared transfers

The transmitter uses GPIO44 and the ESP32-S3 RMT peripheral. The carrier is 1.25 MHz at 50% duty; the envelope clock is 10 MHz. Four RMT memory blocks hold each complete frame, including its closing burst and quiet interval. No software carrier loop or mid-frame memory refill is needed.

PP4 sends the least-significant pair first, with 40 µs bursts. PP16 sends the least-significant nibble first, with 21 µs bursts and a `00 00 00 40` prefix. The prefix is outside the CRC.

## Image sequence

1. Repeat the `0x17` wake frame for at least 4.2 seconds of waveform time.
2. Wait 50 ms, send the image parameters, and wait another 50 ms.
3. Send indexed 20-byte data packets. PP16 sends each packet four times in Reliable mode or three times in Fast mode with at least 1 ms of quiet time after every copy, including the last copy before a new index.
4. Wait 50 ms and send the refresh command.

The parameter frame declares the padded byte count, compression, page, and native display dimensions. The current refresh format has a 23-byte command payload and is 30 bytes including addressing and CRC.

## Image data

Pixels are serialized in native row order. Red and yellow tags carry a black/white plane followed by the accent plane. A zero bit selects black in the first plane or the accent in the second. Four-color profiles use both planes together.

Compression type 2 starts with the first pixel value, followed by alternating run lengths. A length with N binary digits is preceded by N−1 zero bits. Every run, including a final single pixel, has a length code. Payloads are padded with zeroes to a multiple of 20 bytes. The 16-bit byte count limits a transfer to 65,520 padded bytes.

Text is rendered in strips and encoded in wire order, including rotated compositions. Browser artwork is encoded before upload; the device validates its size and run lengths before staging it. The browser waits for each upload acknowledgement before sending the next chunk.

## Verification

`bash scripts/test.sh` checks fixed frame vectors, all PP16 byte values, PP4 symbols, prefix order, packet gaps, wake duration, invalid sizes, and raw/RLE round trips. The image fixtures include the 152×152, 208×112, and 400×300 two-plane profiles. C++ tests run with address and undefined-behavior sanitizers.

For a hardware check, send text, a red/black/white pattern, and a dithered image to each tag. Repeat from sleep and immediately after a previous update, both from the device and from the browser. Record accepted updates and elapsed time. A logic analyzer can verify the carrier and envelope at GPIO44; an optical receiver is needed to measure the emitted signal.
