# Local Arduino platform sources

`hardware/uc3bduino/avr32` contains the platform recipes, minimal core and
variant. Install from the complete repository with
`scripts/install-arduino-hardware.ps1`; copying this directory alone is not
sufficient. The installer also copies the canonical `src/hal`, startup and
linker sources into the installed package. There is no second maintained copy
of those components in Git.

See [local installation and compilation](../docs/ARDUINO_IDE.md) and the
[actual test matrix](../docs/TESTING.md). No upload tool or board-manager
distribution is supplied.
