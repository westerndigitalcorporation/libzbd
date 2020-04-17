/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * SPDX-FileCopyrightText: 2020 Western Digital Corporation or its affiliates.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 *          Ting Yao <tingyao@hust.edu.cn>
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
 * Library log level (per thread).
 */
extern __thread int zbd_log_level;

#define zbd_print(stream,format,args...)		\
	do {						\
		fprintf((stream), format, ## args);     \
		fflush(stream);                         \
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
				"[PANIC] " format,      \
				##args);                \
		assert(0);                              \
	} while (0)

#define zbd_assert(cond)					\
	do {							\
		if (!(cond))					\
			zbd_panic("Condition %s failed\n",	\
				  # cond);			\
	} while (0)

#endif

/* __LIBZBD_INTERNAL_H__ */
