/*
   +----------------------------------------------------------------------+
   | Xdebug                                                               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2002-2023 Derick Rethans                               |
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
#include "php_xdebug.h"

#include "map.h"

#include "lib/lib.h"
#include "lib/log.h"
#include "lib/usefulstuff.h"

ZEND_EXTERN_MODULE_GLOBALS(xdebug)

const char *xdebug_base_source_map(const char *in)
{
	void *entry;

	if (!XG_BASE(source_map_rev) || XG_BASE(source_map_rev)->size == 0) {
		return in;
	}
	if (xdebug_hash_find(XG_BASE(source_map_rev), in, strlen(in), &entry)) {
		return (const char*) entry;
	}
	return in;
}


#define SRC_PREFIX "source_prefix: "
#define DST_PREFIX "target_prefix: "

typedef struct _parser_context
{
	char *source_prefix;
	char *target_prefix;
} parser_context;

static bool handle_line(parser_context *ctxt, int line_number, const char *line)
{
	char *trimmed_line = NULL;
	const char *equals = NULL;

	if (strrchr(line, '\n') == 0) {
		zend_throw_error(NULL, "Line %d is too long: '%s'", line_number, line);
		goto cleanup;
	}

	trimmed_line = xdebug_trim(line);

	/* Empty line */
	if (strlen(trimmed_line) == 0) {
		goto cleanup;
	}

	/* Commented line */
	if (trimmed_line[0] == '#') {
		/* do nothing */
		goto cleanup;
	}

	/* Line starts with "source_prefix: " */
	if (strstr(trimmed_line, SRC_PREFIX) == trimmed_line) {
		xdebug_log(XLOG_CHAN_DEBUG, XLOG_DEBUG, "New source prefix: '%s'", trimmed_line + strlen(SRC_PREFIX));
		if (ctxt->source_prefix) {
			xdfree(ctxt->source_prefix);
		}
		ctxt->source_prefix = xdstrdup(trimmed_line + strlen(SRC_PREFIX));
		goto cleanup;
	}

	/* Line starts with "target_prefix: " */
	if (strstr(trimmed_line, DST_PREFIX) == trimmed_line) {
		xdebug_log(XLOG_CHAN_DEBUG, XLOG_DEBUG, "New target prefix: '%s'", trimmed_line + strlen(DST_PREFIX));
		if (ctxt->target_prefix) {
			xdfree(ctxt->target_prefix);
		}
		ctxt->target_prefix = xdstrdup(trimmed_line + strlen(DST_PREFIX));
		goto cleanup;
	}

	/* There is a " = " sequence in the line, and it doesn't start with it */
	if ((equals = strstr(trimmed_line, " = ")) != NULL) {
		char *from;
		char *to;

		if (!ctxt->source_prefix) {
			zend_throw_error(NULL, "No source prefix defined yet in line %d", line_number);
			goto cleanup;
		}
		if (!ctxt->target_prefix) {
			zend_throw_error(NULL, "No target prefix defined yet in line %d", line_number);
			goto cleanup;
		}
		
		from = xdebug_sprintf("%s/%s", ctxt->source_prefix, trimmed_line);
		from[strlen(ctxt->source_prefix) + 1 + (equals - trimmed_line)] = '\0';
		to = xdebug_sprintf("%s/%s", ctxt->target_prefix, equals + strlen(" = "));

		xdebug_log(XLOG_CHAN_DEBUG, XLOG_DEBUG, "New map: '%s' → '%s'", from, to);

		xdebug_hash_add(XG_BASE(source_map), from, strlen(from), xdstrdup(to));
		xdebug_hash_add(XG_BASE(source_map_rev), to, strlen(to), xdstrdup(from));

		xdfree(from);
		xdfree(to);

		goto cleanup;
	}

cleanup:
	if (trimmed_line) {
		xdfree(trimmed_line);
	}

	return true;
}

static void destroy_parser_context(parser_context *ctxt)
{
	if (ctxt->source_prefix) {
		xdfree(ctxt->source_prefix);
	}
	if (ctxt->target_prefix) {
		xdfree(ctxt->target_prefix);
	}
}

static bool parse_map_file(zend_string *map_file)
{
	FILE *map = fopen(ZSTR_VAL(map_file), "r");
	char buffer[4096];
	const char *line;
	int line_number = 0;
	parser_context ctxt = { NULL };

	/* Open file */
	if (!map) {
		zend_throw_error(NULL, "Could not open map file '%s'", ZSTR_VAL(map_file));
		return false;
	}

	/* Read through all lines */
	do {
		line = fgets(buffer, sizeof(buffer), map);
		if (line) {
			line_number++;
			if (!handle_line(&ctxt, line_number, line)) {
				destroy_parser_context(&ctxt);
				fclose(map);
				return false;
			}
		}
	} while (line);

	destroy_parser_context(&ctxt);
	fclose(map);
	return true;
}

/* {{{ proto void xdebug_set_source_map(string file)
   This function configures adds definitions from the 'file' to the internal source mapping */
PHP_FUNCTION(xdebug_set_source_map)
{
	zend_string *map_file;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(map_file)
	ZEND_PARSE_PARAMETERS_END();

	if (!parse_map_file(map_file)) {
		RETURN_THROWS();
	}

	RETURN_TRUE;
}
/* }}} */
