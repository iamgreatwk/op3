// SPDX-License-Identifier: GPL-2.0-only
/*
 * Read a bounded region from the OP3 rear-camera EEPROM.
 *
 * This utility intentionally addresses only the source-backed 7-bit address
 * 0x50 on a caller-selected I2C adapter.  The two-byte offset is sent as an
 * EEPROM address-pointer transaction; no EEPROM data-write command is sent.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define OP3_EEPROM_ADDR 0x50
#define OP3_EEPROM_DEFAULT_OFFSET 0x0000
#define OP3_EEPROM_DEFAULT_LENGTH 0x30
#define OP3_EEPROM_MAX_LENGTH 0x100

static void usage(const char *program)
{
	fprintf(stderr,
		"usage: %s /dev/i2c-X [offset_hex] [length_hex]\n"
		"defaults: offset=0x0000 length=0x30 address=0x50\n",
		program);
}

static int parse_number(const char *text, unsigned long max,
			unsigned long *value)
{
	char *end;
	unsigned long parsed;

	errno = 0;
	parsed = strtoul(text, &end, 0);
	if (errno || end == text || *end != '\0' || parsed > max)
		return -1;

	*value = parsed;
	return 0;
}

static void print_hex(const uint8_t *data, size_t length, unsigned int offset)
{
	size_t line;

	for (line = 0; line < length; line += 16) {
		size_t i;
		size_t end = line + 16;

		if (end > length)
			end = length;

		printf("%04x:", offset + (unsigned int)line);
		for (i = line; i < end; ++i)
			printf(" %02x", data[i]);
		for (; i < line + 16; ++i)
			printf("   ");
		printf("  |");
		for (i = line; i < end; ++i)
			putchar(data[i] >= 0x20 && data[i] <= 0x7e ? data[i] : '.');
		printf("|\n");
	}
}

int main(int argc, char **argv)
{
	unsigned long offset = OP3_EEPROM_DEFAULT_OFFSET;
	unsigned long length = OP3_EEPROM_DEFAULT_LENGTH;
	uint8_t address_bytes[2];
	uint8_t data[OP3_EEPROM_MAX_LENGTH];
	struct i2c_msg messages[2];
	struct i2c_rdwr_ioctl_data transaction;
	int fd;
	int result;

	if (argc < 2 || argc > 4) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	if (argc >= 3 && parse_number(argv[2], 0xffff, &offset)) {
		fprintf(stderr, "invalid EEPROM offset: %s\n", argv[2]);
		return EXIT_FAILURE;
	}
	if (argc >= 4 && parse_number(argv[3], OP3_EEPROM_MAX_LENGTH, &length)) {
		fprintf(stderr, "invalid EEPROM length: %s\n", argv[3]);
		return EXIT_FAILURE;
	}
	if (!length) {
		fprintf(stderr, "EEPROM length must be non-zero\n");
		return EXIT_FAILURE;
	}
	if (offset + length > 0x10000UL) {
		fprintf(stderr, "EEPROM read crosses 16-bit address space\n");
		return EXIT_FAILURE;
	}

	fd = open(argv[1], O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "open %s: %s\n", argv[1], strerror(errno));
		return EXIT_FAILURE;
	}

	address_bytes[0] = (uint8_t)(offset >> 8);
	address_bytes[1] = (uint8_t)offset;
	memset(data, 0, sizeof(data));
	memset(messages, 0, sizeof(messages));
	messages[0].addr = OP3_EEPROM_ADDR;
	messages[0].flags = 0;
	messages[0].len = sizeof(address_bytes);
	messages[0].buf = address_bytes;
	messages[1].addr = OP3_EEPROM_ADDR;
	messages[1].flags = I2C_M_RD;
	messages[1].len = (uint16_t)length;
	messages[1].buf = data;
	transaction.msgs = messages;
	transaction.nmsgs = 2;

	result = ioctl(fd, I2C_RDWR, &transaction);
	if (result != 2) {
		if (result < 0)
			fprintf(stderr, "I2C_RDWR addr=0x%02x offset=0x%04lx length=0x%lx: %s\n",
				OP3_EEPROM_ADDR, offset, length, strerror(errno));
		else
			fprintf(stderr, "I2C_RDWR completed %d/2 messages\n", result);
		close(fd);
		return EXIT_FAILURE;
	}

	printf("adapter=%s address=0x%02x offset=0x%04lx length=0x%lx\n",
		argv[1], OP3_EEPROM_ADDR, offset, length);
	print_hex(data, length, (unsigned int)offset);
	if (offset <= 0x24 && offset + length >= 0x28) {
		unsigned int base = (unsigned int)(0x24 - offset);
		unsigned int raw_24 = data[base] | ((unsigned int)data[base + 1] << 8);
		unsigned int raw_26 = data[base + 2] |
			((unsigned int)data[base + 3] << 8);

		printf("candidate_le16_raw_0x24=0x%04x signed=%d\n",
			raw_24, (int16_t)raw_24);
		printf("candidate_le16_raw_0x26=0x%04x signed=%d\n",
			raw_26, (int16_t)raw_26);
	}

	close(fd);
	return EXIT_SUCCESS;
}
