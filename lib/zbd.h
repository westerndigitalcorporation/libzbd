/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * SPDX-FileCopyrightText: 2020 Western Digital Corporation or its affiliates.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 *	    Ting Yao <tingyao@hust.edu.cn>
 */
#ifndef __LIBZBD_INTERNAL_H__
#define __LIBZBD_INTERNAL_H__

#define _GNU_SOURCE

#include "config.h"
#include "libzbd/zbd.h"

#include <stdio.h>
#include <stdlib.h>

/*
 * 512B sector size shift.
 */
#define SECTOR_SHIFT	9

/*
 * Handle kernel zone capacity support
 */
#ifndef HAVE_BLK_ZONE_REP_V2
#define BLK_ZONE_REP_CAPACITY	(1 << 0)

struct blk_zone_v2 {
	__u64	start;          /* Zone start sector */
	__u64	len;            /* Zone length in number of sectors */
	__u64	wp;             /* Zone write pointer position */
	__u8	type;           /* Zone type */
	__u8	cond;           /* Zone condition */
	__u8	non_seq;        /* Non-sequential write resources active */
	__u8	reset;          /* Reset write pointer recommended */
	__u8	resv[4];
	__u64	capacity;       /* Zone capacity in number of sectors */
	__u8	reserved[24];
};
#define blk_zone blk_zone_v2

struct blk_zone_report_v2 {
	__u64	sector;
	__u32	nr_zones;
	__u32	flags;
struct blk_zone zones[0];
};
#define blk_zone_report blk_zone_report_v2
#endif /* HAVE_BLK_ZONE_REP_V2 */

extern int zbd_get_sysfs_attr_int64(char *devname, const char *attr,
				    long long *val);
extern int zbd_get_sysfs_attr_str(char *devname, const char *attr,
				  char *val, int val_len);

/*
 * Library log level (per thread).
 */
extern __thread int zbd_log_level;

#define zbd_print(stream,format,args...)		\
	do {						\
		fprintf((stream), format, ## args);	\
		fflush(stream);				\
	} while (0)

#define zbd_print_level(l,stream,format,args...)		\
	do {							\
		if ((l) <= zbd_log_level)			\
			zbd_print((stream), "(libzbd) " format,	\
				  ## args);			\
	} while (0)

#define zbd_error(format,args...)	\
	zbd_print_level(ZBD_LOG_ERROR, stderr, "[ERROR] " format, ##args)

#define zbd_debug(format,args...)	\
	zbd_print_level(ZBD_LOG_DEBUG, stdout, format, ##args)

#define zbd_panic(format,args...)	\
	do {						\
		zbd_print_level(ZBD_LOG_ERROR,		\
				stderr,			\
				"[PANIC] " format,	\
				##args);		\
		assert(0);				\
	} while (0)

#define zbd_assert(cond)					\
	do {							\
		if (!(cond))					\
			zbd_panic("Condition %s failed\n",	\
				  # cond);			\
	} while (0)

#endif

/* __LIBZBD_INTERNAL_H__ */
