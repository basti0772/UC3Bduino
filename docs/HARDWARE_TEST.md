# Hardware acceptance checklist

**Not yet executed on a physical AT32UC3B0256.** All boxes below are open.
Record date, tester, board/MCU revision, source commit/version, supply measurements,
OSC0, D0 pad, authorized VID/PID, tool versions and logs for each run. Do not
mark host/mock tests as physical passes. See [INSTALL_WINDOWS](INSTALL_WINDOWS.md).

- [ ] Verify schematic, MCU marking, supplies, decoupling, safe GPIO/LED resistor,
  USB wiring, oscillator and accessible reset/DFU entry. Board powers up normally.
- [ ] Enter factory DFU; one **03EB:2FF6** device appears with a usable driver.
  Record the boot-pin/reset procedure and retain the factory bootloader.
- [ ] Run diagnosis; record version/path results. PnP-only diagnosis does not
  verify exact silicon or boot protection.
- [ ] Compile installed Blink for `uc3bduino:avr32:uc3b0256`; inspect HEX/map
  for the application origin `0x80002000` and untouched protected regions.
- [ ] Explicitly upload Blink; save identity/protection/flash/launch log.
- [ ] Blink executes after launch and after power cycling. Measure nominal
  500 ms high + 500 ms low on D0; record frequency, tolerance and clock source.
- [ ] Compile USBSerialBlink with verified OSC0 and authorized VID/PID.
- [ ] Re-enter DFU and explicitly upload USBSerialBlink. Save the transition log.
- [ ] Application enumerates without a descriptor error; CDC COM port appears
  with the configured VID/PID and the expected Windows CDC driver.
- [ ] Serial Monitor asserts DTR and receives repeated uptime messages while
  D0 continues blinking; closing/reopening the monitor works.
- [ ] USBSerialEcho works with short, long and 64-byte-boundary transfers.
  Verify contents/counts, backpressure and recovery after a paused reader.
- [ ] USB unplug/replug, bus reset and repeated DFU -> application cycles work.
- [ ] Suspend/resume works; measure USB suspend current before any compliance claim.
- [ ] Regression: SerialHello/SerialMillis with a safe 3.3 V UART adapter,
  GPIO read/write and timing continue to work alongside SerialUSB.
- [ ] Validate the installed examples and normal Upload in Arduino IDE 2.x,
  recording IDE version, then repeat from a clean installation of the source ZIP.

Do not publish a stable release until the relevant checks pass and unresolved
failures are documented. Even a development release needs review of the physical
upload/launch/CDC path; Stage 7 only prepares an unpublished `0.7.0-dev` archive.
