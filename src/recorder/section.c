/*
   +----------------------------------------------------------------------+
   | Xdebug                                                               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2020-2021 Derick Rethans                               |
   +----------------------------------------------------------------------+
   | This source file is subject to version 1.01 of the Xdebug license,   |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | https://xdebug.org/license.php                                       |
   | If you did not receive a copy of the Xdebug license and are unable   |
   | to obtain it through the world-wide-web, please send a note to       |
   | derick@xdebug.org so we can mail you a copy immediately.             |
   +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include "php_xdebug.h"

#include "section.h"
#include "var_export_binary.h"

struct _xdebug_recorder_section {
	size_t   capacity;
	size_t   size;
	uint8_t *data;
};

#define INITIAL_CAPACITY  128
#define EXTRA_CAPACITY    256

static void print_hex_data(size_t size, uint8_t *data);
static void print_hex(xdebug_recorder_section *section);

static void ensure_size(xdebug_recorder_section *section, size_t size)
{
	if (section->size + size >= section->capacity) {
		size_t new_size = section->capacity + size + EXTRA_CAPACITY;

fprintf(stderr, "Realloc for %p from %ld to %ld (%ld + %ld + %d)\n", section, section->capacity, new_size, section->capacity, size, EXTRA_CAPACITY);
		section->data = xdrealloc(section->data, new_size);
		section->capacity = new_size;
	}
}


static void xdebug_recorder_section_free(xdebug_recorder_section* section)
{
	xdfree(section->data);
	xdfree(section);
}


static void xdebug_recorder_add_unum_ex(xdebug_recorder_section *section, uint64_t value)
{
	uint64_t x = value;

fprintf(stderr, "ADD_UNUM(%ld): ", value);
	do {
		section->data[section->size] = x & 0x7FU;
		if (x >>= 7) {
			section->data[section->size] |= 0x80U;
		}
fprintf(stderr, "%02X", section->data[section->size]);
		++section->size;
	} while (x);
fprintf(stderr, "\n");
}

void xdebug_recorder_add_unum(xdebug_recorder_section *section, uint64_t value)
{
	ensure_size(section, XDEBUG_RECORDER_MAX_UNUM_SIZE);

	xdebug_recorder_add_unum_ex(section, value);
}

void xdebug_recorder_add_string(xdebug_recorder_section *section, size_t length, const char *str)
{
	ensure_size(section, XDEBUG_RECORDER_MAX_UNUM_SIZE + length);

	xdebug_recorder_add_unum_ex(section, length);

	memcpy(&section->data[section->size], str, length);
	section->size += length;
}

void xdebug_recorder_add_data(xdebug_recorder_section *section, size_t length, uint8_t *data)
{
	ensure_size(section, length);

printf("ADD_DATA: ");
print_hex_data(length, data);
	memcpy(&section->data[section->size], data, length);
	section->size += length;
}

void xdebug_recorder_add_zval(xdebug_recorder_section *section, zval data)
{
	xdebug_recorder_section *tmp = xdebug_recorder_section_create(SECTION_VARIABLE, SECTION_VARIABLE_VERSION, 0);
	xdebug_get_zval_binary(tmp, &data, NULL);
	xdebug_recorder_add_data(section, tmp->size, tmp->data);

	xdebug_recorder_section_free(tmp);
}

xdebug_recorder_section *xdebug_recorder_section_create(uint8_t type, uint8_t version, size_t initial_cap)
{
	xdebug_recorder_section *tmp = xdmalloc(sizeof(xdebug_recorder_section));
	uint64_t section_id;

	tmp->capacity = initial_cap ? initial_cap : INITIAL_CAPACITY;
	tmp->capacity += XDEBUG_RECORDER_MAX_UNUM_SIZE; /* For type */
	tmp->data = xdmalloc(tmp->capacity);
	tmp->size = 0;

fprintf(stderr, "Alloc for %p [%d:%d] as %ld\n", tmp, type, version, tmp->capacity);

	section_id = (version << SECTION_VERSION_SHIFT) + type;

	xdebug_recorder_add_unum(tmp, section_id);

	return tmp;
}

void xdebug_recorder_write_section(FILE *recorder_file, xdebug_recorder_section *section)
{
	xdebug_recorder_add_unum(section, 0x7F);

	print_hex(section);

	fwrite(section->data, section->size, 1, recorder_file);
	fflush(recorder_file);

	xdebug_recorder_section_free(section);
}

static void print_hex_data(size_t size, uint8_t *data)
{
	int i;

	fprintf(stderr, "%4ld: ", size);
	fprintf(stderr, "[");
	for (i = 0; i < size; i++) {
		fprintf(stderr, "%02X", data[i]);
	}
	fprintf(stderr, "]\n");
}

static void print_hex(xdebug_recorder_section *section)
{
	int i;

	fprintf(stderr, "%4ld: ", section->size);
	fprintf(stderr, "[");
	for (i = 0; i < section->size; i++) {
		fprintf(stderr, "%02X", section->data[i]);
	}
	fprintf(stderr, "]\n");
}
