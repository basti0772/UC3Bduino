# Wire scanner (software I2C)

Requires appropriate external pull-ups and compatible device voltage on
PA09/SCL and PA10/SDA. Scans only unicast addresses 0x08–0x77 using genuine
address-only writes, never a dummy data byte. CDC prints addresses in decimal.
Check attached devices' probe behavior before scanning. Bus faults/timeouts
stop the scan and are reported separately from an address NACK.

This example is compile-tested; no external I2C device was available for testing.
See `docs/WIRE.md` for the software backend, timing and compatibility limits.
