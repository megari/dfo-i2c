dfo-i2c: a utility for reading/writing to CDCE(L)913 through I2C
================================================================

Author
------

Ari Sundholm <megari@iki.fi>

License
-------

Public domain for the utility itself.
Please also see the libcintelhex license.

If you can, credit me appropriately.

General
-------

The project gets its name from the use of the chip in the
[DFO project](https://nfggames.com/forum2/index.php?topic=5744.0).

It can both read and write the registers on a CDCE913 or CDCEL913
chip through an I2C interface. The CDCE(L)925, CDCE(L)937 and
CDCE(L)949 chips may or may not work, but probably only configuring the
first PLL is possible. Support for these chips should be quite easy
to add, and patches/PRs for this are welcome.

It incorporates the
[libcintelhex](https://github.com/martin-helmich/libcintelhex) library
as a submodule for reading Intel HEX files into memory for writing to
a supported chip.

Building
--------

### Requirements

See the build requirements for libcintelhex. Otherwise, just a working
build environment for the C99 language is required, as well as having
the Linux I2C development headers in place.

Don't forget to initialize the libcintelhex submodule:

	git submodule update --init

### Compilation

	make

Usage
-----

	dfo-i2c DEVICE [-a I2C_ADDR] [-d] [-e] [-f HEXFILE]

	-a	The I2C address of the chip.
	-d	Write the factory default register values to the chip
	-e	Write to EEPROM. Makes the written values stick.
	-f	Filename of a Intel HEX file to write to the chip.
