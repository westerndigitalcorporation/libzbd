// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * SPDX-FileCopyrightText: 2021 Western Digital Corporation or its affiliates.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 */
#ifndef _ZBD_TOOL_H_
#define _ZBD_TOOL_H_

#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>

#include "libzbd/zbd.h"

enum zbd_cmd {
	ZBD_REPORT,
	ZBD_RESET,
	ZBD_OPEN,
	ZBD_CLOSE,
	ZBD_FINISH,
	ZBD_DUMP,
	ZBD_RESTORE,
};

/*
 * Command line options and device information.
 */
struct zbd_opts {
	/* Common options */
	char			*dev_path;
	char			*dump_path;
	char			*dump_prefix;
	struct zbd_info		dev_info;
	enum zbd_cmd		cmd;
	long long		ofst;
	long long		len;
	size_t			unit;

	/* Report zones options */
	bool			rep_csv;
	bool			rep_num_zones;
	bool			rep_capacity;
	bool			rep_dump;
	enum zbd_report_option	rep_opt;
};

/*
 * Zone information dump file header.
 */
struct zbd_dump {
	struct zbd_info		dev_info;	/* 128 */

	unsigned int		zstart;		/* 132 */
	unsigned int		zend;		/* 136 */

	uint8_t			reserved[56];	/* 192 */
} __attribute__((packed));

int zbd_open_dump(struct zbd_opts *opts);
int zbd_dump_report_zones(int fd, struct zbd_opts *opts,
			  struct zbd_zone *zones, unsigned int *nr_zones);
int zbd_dump(int fd, struct zbd_opts *opts);
int zbd_restore(int fd, struct zbd_opts *opts);

#endif /* _ZBD_TOOL_H_ */
