# Internal SPI loopback

No MOSI-MISO jumper or external slave is needed. The SPI controller internally
returns transmitted data. **PA14/MOSI and PA15/SCK are still driven and PA25/MISO
is assigned to SPI**; do not connect conflicting outputs. No chip-select pad is
driven. After the checks, `SPI.end()` releases these three pins to floating GPIO.

Tests: all 256 byte values, a 16-bit word, an in-place buffer, four modes, both
bit orders, three requested clocks and one end/begin cycle (25 cases). SerialUSB
repeats the result with a build identifier; LED and automatic upload stay active.
Internal loopback is not an external wiring, signal timing or slave-device test.
