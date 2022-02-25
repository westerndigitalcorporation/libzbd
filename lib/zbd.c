// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * SPDX-FileCopyrightText: 2020 Western Digital Corporation or its affiliates.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 *	    Ting Yao <tingyao@hust.edu.cn>
 */
#include "zbd.h"

#include <errno.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <libgen.h>

/*
 * Per fd device information.
 */
#define ZBD_FD_MAX	1024

static struct zbd_info *zbd_fdi[ZBD_FD_MAX];

static inline struct zbd_info *zbd_get_fd(int fd)
{
	if (fd < 0 || fd >= ZBD_FD_MAX)
		return NULL;

	return zbd_fdi[fd];
}

static inline void zbd_put_fd(int fd)
{
	free(zbd_fdi[fd]);
	zbd_fdi[fd] = NULL;
}

static int zbd_dev_path(const char *filename, char **path, char **devname)
{
	char *p;

	/* Follow symlinks (required for device mapped devices) */
	p = realpath(filename, NULL);
	if (!p) {
		zbd_error("%s: Failed to get real path %d (%s)\n",
			  filename, errno, strerror(errno));
		return -1;
	}

	*path = p;
	*devname = basename(p);

	return 0;
}

/*
 * Get a block device zone model.
 */
static enum zbd_dev_model zbd_get_dev_model(char *devname)
{
	char str[128];
	int ret;

	/* Check that this is a zoned block device */
	ret = zbd_get_sysfs_attr_str(devname, "queue/zoned",
				     str, sizeof(str));
	if (ret) {
		long long val;

		/*
		 * Assume old kernel or kernel without ZBD support enabled: try
		 * a sysfs file that must exist for all block devices. If it is
		 * found, then this is a regular non-zoned device.
		 */
		ret = zbd_get_sysfs_attr_int64(devname,
					       "queue/logical_block_size",
					       &val);
		if (ret == 0)
			return ZBD_DM_NOT_ZONED;
		return -1;
	}

	if (strcmp(str, "host-aware") == 0)
		return ZBD_DM_HOST_AWARE;
	if (strcmp(str, "host-managed") == 0)
		return ZBD_DM_HOST_MANAGED;
	if (strcmp(str, "none") == 0)
		return ZBD_DM_NOT_ZONED;

	return -1;
}



/*
 * Get max number of open/active zones.
 */
static void zbd_get_max_resources(char *devname, struct zbd_info *zbdi)
{
	long long val;
	int ret;

	/*
	 * According to max_open_zones/max_active_zones sysfs documentation,
	 * a sysfs value of 0 means no limit.
	 *
	 * While the ZAC/ZBC standard has special treatment for unknown,
	 * unknown is exported to sysfs as 0.
	 *
	 * Default both to unlimited, and set a limit if we managed to read
	 * a limit from sysfs successfully.
	 */
	ret = zbd_get_sysfs_attr_int64(devname, "queue/max_open_zones", &val);
	if (ret)
		val = 0;
	zbdi->max_nr_open_zones = val;

	ret = zbd_get_sysfs_attr_int64(devname, "queue/max_active_zones", &val);
	if (ret)
		val = 0;
	zbdi->max_nr_active_zones = val;
}

/*
 * Get vendor ID.
 */
static int zbd_get_vendor_id(char *devname, struct zbd_info *zbdi)
{
	char str[128];
	int ret, n = 0;

	ret = zbd_get_sysfs_attr_str(devname, "device/vendor",
				     str, sizeof(str));
	if (!ret)
		n = snprintf(zbdi->vendor_id, ZBD_VENDOR_ID_LENGTH,
			     "%s ", str);

	ret = zbd_get_sysfs_attr_str(devname, "device/model",
				     str, sizeof(str));
	if (!ret)
		n += snprintf(&zbdi->vendor_id[n], ZBD_VENDOR_ID_LENGTH - n,
			      "%s ", str);

	ret = zbd_get_sysfs_attr_str(devname, "device/rev",
				     str, sizeof(str));
	if (!ret)
		n += snprintf(&zbdi->vendor_id[n], ZBD_VENDOR_ID_LENGTH - n,
			      "%s", str);

	return n > 0;
}

/*
 * Get zone size in 512B sector unit.
 */
#ifdef BLKGETZONESZ
static int zbd_get_zone_sectors(int fd, char *devname, __u32 *zone_sectors)
{
	int ret;

	ret = ioctl(fd, BLKGETZONESZ, zone_sectors);
	if (ret) {
		zbd_error("ioctl BLKGETZONESZ failed %d (%s)\n",
			  errno, strerror(errno));
		return -1;
	}

	return 0;
}
#else
static int zbd_get_zone_sectors(int fd, char *devname, __u32 *zone_sectors)
{
	long long zs;
	int ret;

	ret = zbd_get_sysfs_attr_int64(devname, "queue/chunk_sectors", &zs);
	if (ret) {
		zbd_error("Get zone size from sysfs failed\n");
		return -1;
	}

	if (zs >= UINT_MAX) {
		zbd_error("Invalid zone sectors %lld\n", zs);
		return -1;
	}

	*zone_sectors = zs;

	return 0;
}
#endif

static int zbd_get_zone_size(int fd, char *devname, struct zbd_info *zbdi)
{
	__u32 zone_sectors;
	int ret;

	ret = zbd_get_zone_sectors(fd, devname, &zone_sectors);
	if (ret)
		return ret;

	if (!zone_sectors) {
		zbd_error("Invalid 0 zone size\n");
		return -1;
	}

	zbdi->zone_sectors = zone_sectors;
	zbdi->zone_size = (unsigned long long)zone_sectors << SECTOR_SHIFT;

	return 0;
}

/*
 * Get total number of zones.
 */
static int zbd_get_nr_zones(int fd, char *devname, struct zbd_info *zbdi)
{
	__u32 nr_zones;

#ifdef BLKGETNRZONES
	int ret = ioctl(fd, BLKGETNRZONES, &nr_zones);
	if (ret != 0) {
		zbd_error("ioctl BLKGETNRZONES failed %d (%s)\n",
			  errno, strerror(errno));
		return -1;
	}
#else
	nr_zones = (zbdi->nr_sectors + zbdi->zone_sectors - 1)
		/ zbdi->zone_sectors;
#endif

	if (!nr_zones) {
		zbd_error("Invalid 0 number of zones\n");
		return -1;
	}

	zbdi->nr_zones = nr_zones;

	return 0;
}

static struct zbd_info *zbd_do_get_info(int fd, char *devname)
{
	unsigned long long size64;
	struct zbd_info *zbdi;
	int ret, size32;

	zbdi = malloc(sizeof(struct zbd_info));
	if (!zbdi)
		return NULL;

	/* Get zone model */
	zbdi->model = zbd_get_dev_model(devname);
	if (zbdi->model != ZBD_DM_HOST_AWARE &&
	    zbdi->model != ZBD_DM_HOST_MANAGED) {
		zbd_error("Invalid device zone model\n");
		goto err;
	}

	/* Get logical block size */
	ret = ioctl(fd, BLKSSZGET, &size32);
	if (ret != 0) {
		zbd_error("ioctl BLKSSZGET failed %d (%s)\n",
			  errno, strerror(errno));
		goto err;
	}
	zbdi->lblock_size = size32;
	if (zbdi->lblock_size <= 0) {
		zbd_error("invalid logical sector size %d\n",
			  size32);
		goto err;
	}

	/* Get physical block size */
	ret = ioctl(fd, BLKPBSZGET, &size32);
	if (ret != 0) {
		zbd_error("ioctl BLKPBSZGET failed %d (%s)\n",
			  errno, strerror(errno));
		goto err;
	}
	zbdi->pblock_size = size32;
	if (zbdi->pblock_size <= 0) {
		zbd_error("Invalid physical sector size %d\n",
			  size32);
		goto err;
	}

	/* Get capacity (Bytes) */
	ret = ioctl(fd, BLKGETSIZE64, &size64);
	if (ret != 0) {
		zbd_error("ioctl BLKGETSIZE64 failed %d (%s)\n",
			  errno, strerror(errno));
		goto err;
	}
	zbdi->nr_sectors = size64 >> SECTOR_SHIFT;

	zbdi->nr_lblocks = size64 / zbdi->lblock_size;
	if (!zbdi->nr_lblocks) {
		zbd_error("Invalid capacity (logical blocks)\n");
		goto err;
	}

	zbdi->nr_pblocks = size64 / zbdi->pblock_size;
	if (!zbdi->nr_pblocks) {
		zbd_error("Invalid capacity (physical blocks)\n");
		goto err;
	}

	/* Get zone size */
	ret = zbd_get_zone_size(fd, devname, zbdi);
	if (ret)
		goto err;

	/* Get number of zones */
	ret = zbd_get_nr_zones(fd, devname, zbdi);
	if (ret)
		goto err;

	/* Get max number of open/active zones */
	zbd_get_max_resources(devname, zbdi);

	/* Finish setting */
	if (!zbd_get_vendor_id(devname, zbdi))
		strncpy(zbdi->vendor_id,
			"Unknown", ZBD_VENDOR_ID_LENGTH - 1);

	return zbdi;
err:
	free(zbdi);
	return NULL;
}

/**
 * zbd_device_is_zoned - Test if a physical device is zoned.
 */
int zbd_device_is_zoned(const char *filename)
{
	char *path = NULL, *devname = NULL;
	enum zbd_dev_model model;
	struct stat st;
	int ret;

	ret = zbd_dev_path(filename, &path, &devname);
	if (ret)
		return ret;

	/* Check device */
	if (stat(path, &st) != 0) {
		zbd_error("Stat device file failed %d (%s)\n",
			  errno, strerror(errno));
		free(path);
		return 0;
	}

	if (!S_ISBLK(st.st_mode)) {
		free(path);
		return 0;
	}

	model = zbd_get_dev_model(devname);

	free(path);

	return model == ZBD_DM_HOST_AWARE ||
		model == ZBD_DM_HOST_MANAGED;
}

/**
 * zbd_open - open a ZBD device
 */
int zbd_open(const char *filename, int flags, struct zbd_info *info)
{
	char *path = NULL, *devname = NULL;
	struct zbd_info *zbdi;
	int ret, fd;

	if (!zbd_device_is_zoned(filename)) {
		zbd_error("Device %s is not a zoned block device\n",
			  filename);
		return -1;
	}

	ret = zbd_dev_path(filename, &path, &devname);
	if (ret)
		return ret;

	/* Open block device */
	fd = open(path, flags | O_LARGEFILE); //direct
	if (fd < 0) {
		zbd_error("open %s failed %d (%s)\n",
			  filename, errno, strerror(errno));
		goto err;
	}

	/* Get device information */
	zbdi = zbd_do_get_info(fd, devname);
	if (!zbdi)
		goto err;

	zbd_fdi[fd] = zbdi;
	if (info)
		memcpy(info, zbdi, sizeof(struct zbd_info));

	free(path);

	return fd;

err:
	if (fd >= 0) {
		close(fd);
		fd = -1;
	}

	free(path);

	return fd;
}

/**
 * zbd_close - close a ZBD Device
 */
void zbd_close(int fd)
{
	struct zbd_info *zbdi = zbd_get_fd(fd);

	if (!zbdi) {
		zbd_error("Invalid file descriptor %d\n\n", fd);
		return;
	}

	close(fd);
	zbd_put_fd(fd);
}

/**
 * zbd_get_info - Get a ZBD device information
 */
int zbd_get_info(int fd, struct zbd_info *info)
{
	struct zbd_info *zbdi = zbd_get_fd(fd);

	if (!zbdi) {
		zbd_error("Invalid file descriptor %d\n\n", fd);
		return -1;
	}

	if (!info)
		return -1;

	memcpy(info, zbdi, sizeof(struct zbd_info));

	return 0;
}

/*
 * zbd_should_report_zone - Test if a zone must be reported.
 */
static bool zbd_should_report_zone(struct zbd_zone *zone,
				   enum zbd_report_option ro)
{
	switch (ro) {
	case ZBD_RO_ALL:
		return true;
	case ZBD_RO_NOT_WP:
		return zbd_zone_not_wp(zone);
	case ZBD_RO_EMPTY:
		return zbd_zone_empty(zone);
	case ZBD_RO_IMP_OPEN:
		return zbd_zone_imp_open(zone);
	case ZBD_RO_EXP_OPEN:
		return zbd_zone_exp_open(zone);
	case ZBD_RO_CLOSED:
		return zbd_zone_closed(zone);
	case ZBD_RO_FULL:
		return zbd_zone_full(zone);
	case ZBD_RO_RDONLY:
		return zbd_zone_rdonly(zone);
	case ZBD_RO_OFFLINE:
		return zbd_zone_offline(zone);
	case ZBD_RO_RWP_RECOMMENDED:
		return zbd_zone_rwp_recommended(zone);
	case ZBD_RO_NON_SEQ:
		return zbd_zone_non_seq_resources(zone);
	default:
		return false;
	}
}

/*
 * zbd_parse_zone - Fill a zone descriptor
 */
static inline void zbd_parse_zone(struct zbd_zone *zone, struct blk_zone *blkz,
				  struct blk_zone_report *rep)
{
	zone->start = blkz->start << SECTOR_SHIFT;
	zone->len = blkz->len << SECTOR_SHIFT;
	if (rep->flags & BLK_ZONE_REP_CAPACITY)
		zone->capacity = blkz->capacity << SECTOR_SHIFT;
	else
		zone->capacity = zone->len;
	zone->wp = blkz->wp << SECTOR_SHIFT;

	zone->type = blkz->type;
	zone->cond = blkz->cond;
	zone->flags = 0;
	if (blkz->reset)
		zone->flags |= ZBD_ZONE_RWP_RECOMMENDED;
	if (blkz->non_seq)
		zone->flags |= ZBD_ZONE_NON_SEQ_RESOURCES;
}

#define ZBD_REPORT_MAX_NR_ZONE	8192

/**
 * zbd_report_zones - Get zone information
 */
int zbd_report_zones(int fd, off_t ofst, off_t len, enum zbd_report_option ro,
		     struct zbd_zone *zones, unsigned int *nr_zones)
{
	struct zbd_info *zbdi = zbd_get_fd(fd);
	unsigned long long zone_size_mask, end;
	struct blk_zone_report *rep;
	size_t rep_size;
	unsigned int rep_nr_zones;
	unsigned int nrz, n = 0, i = 0;
	struct blk_zone *blkz;
	struct zbd_zone z;
	int ret = 0;

	if (!zbdi) {
		zbd_error("Invalid file descriptor %d\n\n", fd);
		return -1;
	}

	/*
	 * To get zone reports, we need zones and nr_zones.
	 * To get only the number of zones, we need only nr_zones.
	 */
	if ((!zones && !nr_zones) || (zones && !nr_zones))
		return -1;

	/*
	 * When reporting only the number of zones (zones == NULL case),
	 * ignore the value pointed by nr_zones.
	 */
	if (zones) {
		nrz = *nr_zones;
		if (!nrz)
			return 0;
	} else {
		nrz = 0;
	}

	zone_size_mask = zbdi->zone_size - 1;
	if (len == 0)
		len = zbdi->nr_sectors << SECTOR_SHIFT;

	end = ((ofst + len + zone_size_mask) & (~zone_size_mask))
		>> SECTOR_SHIFT;
	if (end > zbdi->nr_sectors)
		end = zbdi->nr_sectors;

	ofst = (ofst & (~zone_size_mask)) >> SECTOR_SHIFT;
	if ((unsigned long long)ofst >= zbdi->nr_sectors) {
		*nr_zones = 0;
		return 0;
	}

	/* Get all zones information */
	rep_nr_zones = ZBD_REPORT_MAX_NR_ZONE;
	if (nrz && nrz < rep_nr_zones)
		rep_nr_zones = nrz;
	rep_size = sizeof(struct blk_zone_report) +
		sizeof(struct blk_zone) * rep_nr_zones;
	rep = (struct blk_zone_report *)malloc(rep_size);
	if (!rep) {
		zbd_error("%d: No memory for array of zones\n\n", fd);
		return -ENOMEM;
	}

	blkz = (struct blk_zone *)(rep + 1);
	while ((!nrz || n < nrz) && (unsigned long long)ofst < end) {

		memset(rep, 0, rep_size);
		rep->sector = ofst;
		rep->nr_zones = rep_nr_zones;

		ret = ioctl(fd, BLKREPORTZONE, rep);
		if (ret != 0) {
			ret = -errno;
			zbd_error("%d: ioctl BLKREPORTZONE at %llu failed %d (%s)\n",
				  fd, (unsigned long long)ofst,
				  errno, strerror(errno));
			goto out;
		}

		if (!rep->nr_zones)
			break;

		for (i = 0; i < rep->nr_zones; i++) {
			if ((nrz && (n >= nrz)) ||
			    ((unsigned long long)ofst >= end))
				break;

			zbd_parse_zone(&z, &blkz[i], rep);
			if (zbd_should_report_zone(&z, ro)) {
				if (zones)
					memcpy(&zones[n], &z, sizeof(z));
				n++;
			}

			ofst = blkz[i].start + blkz[i].len;
		}
	}

	/* Return number of zones */
	*nr_zones = n;

out:
	free(rep);

	return ret;
}

/**
 * zbd_list_zones - Get zone information
 */
int zbd_list_zones(int fd, off_t ofst, off_t len,
		   enum zbd_report_option ro,
		   struct zbd_zone **pzones, unsigned int *pnr_zones)
{
	struct zbd_info *zbdi = zbd_get_fd(fd);
	struct zbd_zone *zones = NULL;
	unsigned int nr_zones = 0;
	int ret;

	if (!zbdi) {
		zbd_error("Invalid file descriptor %d\n\n", fd);
		return -1;
	}

	/* Get number of zones */
	ret = zbd_report_nr_zones(fd, ofst, len, ro, &nr_zones);
	if (ret < 0)
		return ret;

	if (!nr_zones)
		goto out;

	/* Allocate zone array */
	zones = (struct zbd_zone *) calloc(nr_zones, sizeof(struct zbd_zone));
	if (!zones)
		return -ENOMEM;

	/* Get zones information */
	ret = zbd_report_zones(fd, ofst, len, ro, zones, &nr_zones);
	if (ret != 0) {
		zbd_error("%d: zbd_report_zones failed %d\n",
			  fd, ret);
		free(zones);
		return ret;
	}

out:
	*pzones = zones;
	*pnr_zones = nr_zones;
	return 0;
}

/*
 * BLKOPENZONE, BLKCLOSEZONE and BLKFINISHZONE ioctl commands
 * were introduced with kernel 5.5. If they are not defined on the
 * current system, manually define these operations here to generate
 * code portable to newer kernels.
 */
#ifndef BLKOPENZONE
#define BLKOPENZONE	_IOW(0x12, 134, struct blk_zone_range)
#endif
#ifndef BLKCLOSEZONE
#define BLKCLOSEZONE	_IOW(0x12, 135, struct blk_zone_range)
#endif
#ifndef BLKFINISHZONE
#define BLKFINISHZONE	_IOW(0x12, 136, struct blk_zone_range)
#endif

#ifndef ENOIOCTLCMD
#define ENOIOCTLCMD	515
#endif

/**
 * zbd_zone_operation - Execute an operation on a zone
 */
int zbd_zones_operation(int fd, enum zbd_zone_op op, off_t ofst, off_t len)
{
	struct zbd_info *zbdi = zbd_get_fd(fd);
	unsigned long long zone_size_mask, end;
	struct blk_zone_range range;
	const char *ioctl_name;
	unsigned long ioctl_op;
	int ret;

	if (!zbdi) {
		zbd_error("Invalid file descriptor %d\n\n", fd);
		return -1;
	}

	zone_size_mask = zbdi->zone_size - 1;
	if (len == 0)
		len = zbdi->nr_sectors << SECTOR_SHIFT;

	end = ((ofst + len + zone_size_mask) & (~zone_size_mask))
		>> SECTOR_SHIFT;
	if (end > zbdi->nr_sectors)
		end = zbdi->nr_sectors;

	/* Check the operation */
	switch (op) {
	case ZBD_OP_RESET:
		ioctl_name = "BLKRESETZONE";
		ioctl_op = BLKRESETZONE;
		break;
	case ZBD_OP_OPEN:
		ioctl_name = "BLKOPENZONE";
		ioctl_op = BLKOPENZONE;
		break;
	case ZBD_OP_CLOSE:
		ioctl_name = "BLKCLOSEZONE";
		ioctl_op = BLKCLOSEZONE;
		break;
	case ZBD_OP_FINISH:
		ioctl_name = "BLKFINISHZONE";
		ioctl_op = BLKFINISHZONE;
		break;
	default:
		zbd_error("Invalid zone operation 0x%x\n", op);
		errno = EINVAL;
		return -1;
	}

	ofst = (ofst & (~zone_size_mask)) >> SECTOR_SHIFT;
	if ((unsigned long long)ofst >= zbdi->nr_sectors ||
	    end == (unsigned long long)ofst)
		return 0;

	/* Execute the operation */
	range.sector = ofst;
	range.nr_sectors = end - ofst;
	ret = ioctl(fd, ioctl_op, &range);
	if (ret != 0) {
		if (errno == ENOIOCTLCMD || errno == ENOTTY) {
			zbd_error("ioctl %s is not supported\n",
				  ioctl_name);
			errno = ENOTSUP;
		} else {
			zbd_error("ioctl %s failed %d (%s)\n",
				  ioctl_name, errno, strerror(errno));
		}
		return -1;
	}

	return 0;
}
