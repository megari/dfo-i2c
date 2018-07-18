#define _DEFAULT_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include "libcintelhex/include/cintelhex.h"

#define COMMAND_BLOCK 0x00
#define COMMAND_BYTE  0x80

#define EEPIP 0x40

static bool dfo_write_byte(int dev, unsigned char offset, unsigned char data)
{
	ssize_t ret;
	unsigned char cmd_buf[2] = { COMMAND_BYTE | (offset & 0x7f), data };
	if ((ret = write(dev, cmd_buf, sizeof(cmd_buf))) != sizeof(cmd_buf)) {
		if (ret < 0)
			perror("I2C write");
		fprintf(stderr, "ERROR: write() returned %zd\n", ret);
		return false;
	}

	return true;
}

static unsigned char dfo_read_byte(int dev, unsigned char offset)
{
	ssize_t ret;
	unsigned char data = 0x00;
	unsigned char cmd_buf[1] = { COMMAND_BYTE | (offset & 0x7f) };
	if ((ret = write(dev, cmd_buf, sizeof(cmd_buf))) != sizeof(cmd_buf)) {
		if (ret < 0)
			perror("I2C write");
		fprintf(stderr, "ERROR: write() returned %zd\n", ret);
		exit(EXIT_FAILURE);
	}

	if ((ret = read(dev, &data, sizeof(data))) != sizeof(data)) {
		if (ret < 0)
			perror("I2C read");
		fprintf(stderr, "ERROR: read() returned %zd\n", ret);
		exit(EXIT_FAILURE);
	}
	return data;
}

static bool dfo_write_eeprom(int dev)
{
	printf("Writing EEPROM...\n");
	if (!dfo_write_byte(dev, 0x06, 0x41))
		return false;

	while (!(dfo_read_byte(dev, 0x01) & EEPIP))
		usleep(100000);

	return true;
}

static bool dfo_write_defaults(int dev)
{
	static const unsigned char buf_generic[]
		= { 0x00, 0x01, 0xb4, 0x01, 0x02, 0x50 };

	static const unsigned char buf_pll1[]
		= { 0x00, 0x00, 0x00, 0x00, 0xed, 0x02, 0x01, 0x01,
		    0x00, 0x40, 0x02, 0x08, 0x00, 0x40, 0x02, 0x08 };

	printf("Writing default generic configuration...\n");
	for (size_t i = 0x01; i < sizeof(buf_generic); ++i)
		if (!dfo_write_byte(dev, i, buf_generic[i]))
			return false;

	printf("Writing default PLL1 configuration...\n");
	for (size_t i = 0x00; i < sizeof(buf_pll1); ++i)
		if (!dfo_write_byte(dev, i + 0x10, buf_pll1[i]))
			return false;

	return true;
}

static bool dfo_write_hexfile(int dev, const char *filename)
{
	size_t hex_data_len;
	unsigned char *buf;
	ihex_recordset_t *hex_data = ihex_rs_from_file(filename);
	if (!hex_data) {
		fprintf(stderr, "Unable to read hex data!\n");
		return false;
	}

	hex_data_len = ihex_rs_get_size(hex_data);
	buf = malloc(hex_data_len);
	if (!buf) {
		perror("malloc");
		ihex_rs_free(hex_data);
		return false;
	}

	if (ihex_mem_copy(hex_data, buf, hex_data_len, IHEX_WIDTH_8BIT, IHEX_ORDER_NATIVE)) {
		fprintf(stderr, "Unable to convert hex data to buffer!\n");
		free(buf);
		ihex_rs_free(hex_data);
		return false;
	}

	ihex_rs_free(hex_data);


#if 1
	printf("Values to write to generic configuration register:\n");
	for (unsigned char i = 0x01; i < 0x06; ++i)
		printf("\tByte 0x%.2x: 0x%.2x\n", i, buf[i]);

	printf("Values to write to PLL1 configuration register:\n");
	for (unsigned char i = 0x10; i < 0x20; ++i)
		printf("\tByte 0x%.2x: 0x%.2x\n", i, buf[i]);
#endif

	printf("Writing generic configuration...\n");
	for (unsigned char i = 0x01; i < 0x06; ++i)
		if (!dfo_write_byte(dev, i, buf[i])) {
			free(buf);
			return false;
		}

	printf("Writing PLL1 configuration...\n");
	for (unsigned char i = 0x10; i < 0x20; ++i)
		if (!dfo_write_byte(dev, i, buf[i])) {
			free(buf);
			return false;
		}

	free(buf);

	return true;
}


static struct {
	const char *device;
	const char *filename;
	unsigned char i2c_addr;
	bool write_defaults;
	bool write_eeprom;
} options;

bool parse_opts(int argc, char *const *argv)
{
	int c;

	options.device = NULL;
	options.filename = NULL;
	options.i2c_addr = 0x65;
	options.write_defaults = false;
	options.write_eeprom = false;

	while ((c = getopt(argc, argv, "a:def:")) != -1) {
		switch (c) {
		case 'a': {
			unsigned long addr;
			char *endp;

			errno = 0;
			addr = strtoul(optarg, &endp, 16);
			if (errno != 0 || *endp != '\0' ||
				addr > 0x7f)
			{
				fprintf(stderr, "Invalid I2C address "
				                "%s\n", optarg);
				return false;
			}
			options.i2c_addr = addr;
			}
			break;
		case 'd':
			options.write_defaults = true;
			break;
		case 'e':
			options.write_eeprom = true;
			break;
		case 'f':
			options.filename = optarg;
			break;
		default:
			fprintf(stderr, "Invalid option %c\n", c);
			return false;
		}
	}

	if (options.filename && options.write_defaults) {
		fprintf(stderr, "Options -d and -f are mutually exclusive\n");
		return false;
	}

	if (!argv[optind]) {
		fprintf(stderr, "Must specify device\n");
		return false;
	}
	else if (argc > optind + 1) {
		fprintf(stderr, "Extra arguments detected\n");
		return false;
	}
	options.device = argv[optind];

	return true;
}

int main(int argc, char *const *argv)
{
	int dev;
	int ret = EXIT_FAILURE;

	if (argc < 2 || !parse_opts(argc, argv)) {
		fprintf(stderr, "Usage: %s DEVICE [-a I2C_ADDR] [-d] [-e] [-f HEXFILE]\n", argv[0]);
		return EXIT_FAILURE;
	}

#if 1
	printf("Options:\n");
	printf("\tDevice: %s\n", options.device);
	printf("\tI2C address: 0x%.2x\n", options.i2c_addr);
	printf("\tWrite defaults: %s\n", options.write_defaults ? "true" : "false");
	printf("\tWrite EEPROM: %s\n", options.write_eeprom ? "true" : "false");
	printf("\tFilename: %s\n", options.filename);
#endif

	dev = open(options.device, O_RDWR);
        if (dev < 0) {
                perror("open");
                fprintf(stderr, "Failed to open the device node\n");
                return EXIT_FAILURE;
        }

        if (ioctl(dev, I2C_SLAVE, options.i2c_addr) < 0) {
                perror("ioctl");
                fprintf(stderr, "Unable to set I2C slave address\n");
                goto cleanup;
        }

	if (options.filename &&
		!dfo_write_hexfile(dev, options.filename))
	{
		fprintf(stderr, "Failed to write hex file!\n");
		goto cleanup;
	}
	else if (options.write_defaults && !dfo_write_defaults(dev)) {
		fprintf(stderr, "Failed to write default values!\n");
		goto cleanup;
	}

	if (options.write_eeprom && !dfo_write_eeprom(dev)) {
		fprintf(stderr, "Failed to write EEPROM!\n");
		goto cleanup;
	}

	printf("Generic configuration register:\n");
	for (unsigned char i = 0x00; i < 0x07; ++i) {
		unsigned char data = dfo_read_byte(dev, i);
		printf("\tByte 0x%.2x: 0x%.2x\n", i, data);
	}

	printf("PLL1 configuration register:\n");
	for (unsigned char i = 0x10; i < 0x20; ++i) {
		unsigned char data = dfo_read_byte(dev, i);
		printf("\tByte 0x%.2x: 0x%.2x\n", i, data);
	}

	ret = EXIT_SUCCESS;

cleanup:
	close(dev);
        return ret;
}
