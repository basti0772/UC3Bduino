# Register read template

This is a protocol template, not a tested sensor driver. Change device address,
register address and read length for the attached device. Requires external
pull-ups on PA09/SCL and PA10/SDA. `endTransmission(false)` sends/acknowledges
the actual write, holds SCL LOW, and `requestFrom()` issues a repeated START.
Its final received byte is NACKed, then the bus is released with STOP.

Software I2C master only; external hardware behavior remains untested.
