// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * SPDX-FileCopyrightText: 2020 Western Digital Corporation or its affiliates.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 *	    Ting Yao <tingyao@hust.edu.cn>
 */
#include "zbd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

/*
 * Log level.
 */
__thread int zbd_log_level = ZBD_LOG_NONE;

void zbd_set_log_level(enum zbd_log_level level)
{
	switch (level) {
	case ZBD_LOG_NONE:
	case ZBD_LOG_ERROR:
	case ZBD_LOG_DEBUG:
		zbd_log_level = level;
		break;
	default:
		fprintf(stderr, "libzbd: Invalid log level %d\n",
			level);
		break;
	}
}

/*
 * To handle string conversions.
 */
struct zbd_str {
	unsigned int	val;
	const char	*str;
	const char	*str_short;
};

static const char *zbd_get_str(struct zbd_str *str, unsigned int val, bool s)
{
	unsigned int i = 0;

	while (str[i].val != UINT_MAX) {
		if (str[i].val == val)
			break;
		i++;
	}

	if (s)
		return str[i].str_short;

	return str[i].str;
}

static struct zbd_str zbd_dm_str[] = {
	{ ZBD_DM_HOST_MANAGED,	"host-managed",	"HM"	},
	{ ZBD_DM_HOST_AWARE,	"host-aware",	"HA"	},
	{ ZBD_DM_NOT_ZONED,	"not-zoned",	"NZ"	},
	{ UINT_MAX,		"unknown",	"??"	},
};

/**
 * zbd_device_model_str - Returns a device zone model name
 */
const char *zbd_device_model_str(enum zbd_dev_model model, bool s)
{
	return zbd_get_str(zbd_dm_str, model, s);
}

static struct zbd_str zbd_ztype_str[] = {
	{ ZBD_ZONE_TYPE_CNV,	"conventional",		"cnv"	},
	{ ZBD_ZONE_TYPE_SWR,	"seq-write-required",	"swr"	},
	{ ZBD_ZONE_TYPE_SWP,	"seq-write-preferred",	"swp"	},
	{ UINT_MAX,		"unknown",		"???"	}
};

/**
 * zbd_zone_type_str - returns a string describing a zone type
 */
const char *zbd_zone_type_str(struct zbd_zone *z, bool s)
{
	return zbd_get_str(zbd_ztype_str, z->type, s);
}

static struct zbd_str zbd_zcond_str[] = {
	{ ZBD_ZONE_COND_NOT_WP,		"not-write-pointer",	"nw"	},
	{ ZBD_ZONE_COND_EMPTY,		"empty",		"em"	},
	{ ZBD_ZONE_COND_FULL,		"full",			"fu"	},
	{ ZBD_ZONE_COND_IMP_OPEN,	"open-implicit",	"oi"	},
	{ ZBD_ZONE_COND_EXP_OPEN,	"open-explicit",	"oe"	},
	{ ZBD_ZONE_COND_CLOSED,		"closed",		"cl"	},
	{ ZBD_ZONE_COND_READONLY,	"read-only",		"ro"	},
	{ ZBD_ZONE_COND_OFFLINE,	"offline",		"ol"	},
	{ UINT_MAX,			"unknown",		"??"	}
};

/**
 * zbd_zone_cond_str - Returns a string describing a zone condition
 */
const char *zbd_zone_cond_str(struct zbd_zone *z, bool s)
{
	return zbd_get_str(zbd_zcond_str, z->cond, s);
}

/*
 * Strip a string of trailing spaces and carriage return.
 */
static int zbd_str_strip(char *str)
{
	int i = strlen(str) - 1;

	while (i >= 0) {
		if (str[i] == ' ' ||
		    str[i] == '\t' ||
		    str[i] == '\r' ||
		    str[i] == '\n') {
			str[i] = '\0';
			i--;
		} else {
			break;
		}
	}

	return i + 1;
}

static int zbd_get_sysfs_attr(char *devname, const char *attr,
			      char *str, int str_len)
{
	char attr_path[128];
	FILE *file;
	int ret = 0;

	snprintf(attr_path, sizeof(attr_path), "/sys/block/%s/%s",
		 devname, attr);
	file = fopen(attr_path, "r");
	if (!file)
		return -ENOENT;

	if (!fgets(str, str_len, file)) {
		ret = -EINVAL;
		goto close;
	}

	if (!zbd_str_strip(str))
		ret = -EINVAL;

close:
	fclose(file);

	return ret;
}

int zbd_get_sysfs_attr_int64(char *devname, const char *attr, long long *val)
{
	char str[128];
	int ret;

	ret = zbd_get_sysfs_attr(devname, attr, str, 128);
	if (ret)
		return ret;

	*val = atoll(str);

	return 0;
}

int zbd_get_sysfs_attr_str(char *devname, const char *attr,
			   char *val, int val_len)
{
	return zbd_get_sysfs_attr(devname, attr, val, val_len);
}
