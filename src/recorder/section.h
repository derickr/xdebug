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

#ifndef __HAVE_XDEBUG_RECORDER_SECTION_H__
#define __HAVE_XDEBUG_RECORDER_SECTION_H__

#define REF_LIST_VERSION                       0

#define SECTION_HEADER                         0x01
#define SECTION_HEADER_VERSION                 0
#define SECTION_FILE_REF_LIST                  0x02
#define SECTION_FILE_REF_LIST_VERSION          REF_LIST_VERSION
#define SECTION_FILE                           0x03
#define SECTION_FILE_VERSION                   0
#define SECTION_FUNC_REF_LIST                  0x04
#define SECTION_FUNC_REF_LIST_VERSION          REF_LIST_VERSION
#define SECTION_VAR_REF_LIST                   0x05
#define SECTION_VAR_REF_LIST_VERSION           REF_LIST_VERSION
#define SECTION_CALL                           0x06
#define SECTION_CALL_VERSION                   0
#define SECTION_EXIT                           0x07
#define SECTION_EXIT_VERSION                   0

#define SECTION_MASK                           0x3F /* 6 bits, 63 types */
#define SECTION_VERSION_SHIFT                  6


#define XDEBUG_RECORDER_AVG_UNUM_SIZE 3
#define XDEBUG_RECORDER_MAX_UNUM_SIZE 10

typedef struct _xdebug_recorder_section xdebug_recorder_section;

xdebug_recorder_section *xdebug_recorder_section_create(uint8_t type, uint8_t version, size_t initial_cap);
void xdebug_recorder_add_unum(xdebug_recorder_section *section, uint64_t value);
void xdebug_recorder_add_string(xdebug_recorder_section *section, size_t length, const char *str);
void xdebug_recorder_add_data(xdebug_recorder_section *section, size_t length, uint8_t *data);
void xdebug_recorder_write_section(FILE *file, xdebug_recorder_section *section);

#endif /* __HAVE_XDEBUG_RECORDER_SECTION_H__ */
