# Local Arduino platform sources

`hardware/uc3bduino/avr32` contains the platform recipes, minimal core and
variant. Install from the complete repository with
`scripts/install.ps1`; copying this directory alone is not
sufficient. The installer also copies the canonical `src/hal`, `src/usb`, examples, startup and
linker sources into the installed package. There is no second maintained copy
of those components in Git.

See [Windows installation](../docs/INSTALL_WINDOWS.md) and
[package layout](../docs/PACKAGING.md) and the
[actual test matrix](../docs/TESTING.md). The installer includes the canonical
[DFU upload scripts](../docs/DFU_UPLOAD.md); obtain dfu-programmer separately.
No Boards Manager distribution is supplied yet.
