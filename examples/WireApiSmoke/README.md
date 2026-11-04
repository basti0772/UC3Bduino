# Wire API smoke without a device

Runs 43 initialization, invalid-argument and buffer checks. `Wire.begin()`
releases PA09/PA10 as floating GPIO; no START or address is sent. External
pull-ups/device are not needed. CDC repeats an exact build identity and result;
the LED blinks and automatic upload remains available when configured.

A PASS demonstrates firmware/API behavior only. It does not validate physical
I2C communication, ACKs, repeated START, pull-ups or actual bus timing.
