# SPI master example

Check the schematic and attached device's voltage/protocol before uploading.
This sketch drives PA14 (MOSI), PA15 (SCK) and PA16 (software SS, active low),
and reads PA25 (MISO). It sends 0x55 once per second in mode 0 at up to 1 MHz.
There is no assumed sensor command or expected response. No external device
has been validated by this example. LED and CDC/automatic upload remain active.

See `docs/SPI.md` for pin conflicts, electrical limits and error recovery.
