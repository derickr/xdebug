#include <stdio.h>
#include <stdbool.h>

static const char alphabet[256] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, 63,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

static inline bool decode_unit(const char u, unsigned char *jump_part, bool *continued, bool *end_same)
{
	char v = alphabet[u];

	if (v < 0) {
		return false;
	}

	*jump_part = v & 0x0F;
	*end_same  = v & 0x10;
	*continued = v & 0x20;

	return true;
}

static inline bool decode_units(char **line_ptr, unsigned long *jump_val, bool *end_same)
{
	unsigned char jump_part;
	bool continued, ok;

	*jump_val = 0;

	ok = decode_unit(**line_ptr, &jump_part, &continued, end_same);
	if (!ok) {
		goto fail;
	}
	*jump_val = jump_part;
	(*line_ptr)++;

	while (continued)
	{
		if (**line_ptr == '\0') {
			goto fail;
		}

		*jump_val = *jump_val << 4;
		ok = decode_unit(**line_ptr, &jump_part, &continued, end_same);
		if (!ok) {
			goto fail;
		}
		*jump_val += jump_part;
		(*line_ptr)++;
	}

	return true;

fail:
	return false;
}

bool xdebug_lines_decoder(const char *remote_line, const char *local_line)
{
	char   *r_ptr   = (char*) remote_line;
	char   *l_ptr   = (char*) local_line;
	size_t  i       = 0;
	size_t  cur_remote_line = 0;
	size_t  cur_local_line = 0;

	do {
		unsigned long jump_val = 0;
		bool end_same, skip, ok;

		/* Remote Line */
		if (*r_ptr == '_') {
			/* fail, not allowed */
			goto fail;
		}

		ok = decode_units(&r_ptr, &jump_val, &end_same);
		if (!ok) {
			goto fail;
		}

		if (end_same) {
			cur_remote_line += jump_val;
			printf("%ld", cur_remote_line);
		} else {
			cur_remote_line += jump_val;
			printf("%ld-", cur_remote_line);

			ok = decode_units(&r_ptr, &jump_val, &end_same);
			if (!ok) {
				goto fail;
			}

			cur_remote_line += jump_val;
			printf("%ld", cur_remote_line);
		}

		/* Local Line */
		if (*l_ptr == '_') {
			printf("%s", "=SKIP ");
			l_ptr++;
			continue;
		}

		ok = decode_units(&l_ptr, &jump_val, &end_same);
		if (!ok) {
			goto fail;
		}

		if (end_same) {
			cur_local_line += jump_val;
			printf("=%ld ", cur_local_line);
		} else {
			cur_local_line += jump_val;
			printf("=%ld,", cur_local_line);

			ok = decode_units(&l_ptr, &jump_val, &end_same);
			if (!ok) {
				goto fail;
			}

			cur_local_line += jump_val;
			printf("%ld ", cur_local_line);
		}

	} while (*l_ptr != '\0' && *r_ptr != '\0');

	if (*r_ptr != '\0' || *l_ptr != '\0') {
		goto fail;
	}

fail:
	return false;
}

bool xdebug_lines_encoder(void)
{
	return false;
}
