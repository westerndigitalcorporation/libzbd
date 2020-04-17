// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * SPDX-FileCopyrightText: 2020 Western Digital Corporation or its affiliates.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 *	    Ting Yao <tingyao@hust.edu.cn>
 */
#include "zbd.h"

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
        FILE *file;

        /* Check that this is a zoned block device */
        snprintf(str, sizeof(str),
                 "/sys/block/%s/queue/zoned",
                 devname);
        file = fopen(str, "r");
        if (!file) {
		/*
		 * Assume old kernel or kernel without ZBD support enabled: try
		 * a sysfs file that must exist for all block devices. If it is
		 * found, then this is a regular non-zoned device.
		 */
		snprintf(str, sizeof(str),
			 "/sys/block/%s/queue/logical_block_size",
			 devname);
		file = fopen(str, "r");
		if (file) {
			fclose(file);
			return ZBD_DM_NOT_ZONED;
		}
		return -1;
	}

        memset(str, 0, sizeof(str));
        fscanf(file, "%s", str);
        fclose(file);

        if (strcmp(str, "host-aware") == 0)
		return ZBD_DM_HOST_AWARE;
	if (strcmp(str, "host-managed") == 0)
		return ZBD_DM_HOST_MANAGED;
	if (strcmp(str, "none") == 0)
		return ZBD_DM_NOT_ZONED;

	return -1;
}

/*
 * Get a string in a file and strip it of trailing
 * spaces and carriage return.
 */
static int zbd_get_str(FILE *file, char *str)
{
        int len = 0;

        if (fgets(str, 128, file)) {
                len = strlen(str) - 1;
                while (len > 0) {
                        if (str[len] == ' ' ||
                            str[len] == '\t' ||
                            str[len] == '\r' ||
                            str[len] == '\n') {
                                str[len] = '\0';
                                len--;
                        } else {
                                break;
                        }
                }
        }

        return len;
}

/*
 * Get vendor ID.
 */
static int zbd_get_vendor_id(char *devname, struct zbd_info *zbdi)
{
        char str[128];
        FILE *file;
        int n = 0, len;

        snprintf(str, sizeof(str),
                 "/sys/block/%s/device/vendor",
                 devname);
        file = fopen(str, "r");
        if (file) {
                len = zbd_get_str(file, str);
                if (len)
                        n = snprintf(zbdi->vendor_id,
                                     ZBD_VENDOR_ID_LENGTH,
                                     "%s ", str);
                fclose(file);
        }

        snprintf(str, sizeof(str),
                 "/sys/block/%s/device/model",
                 devname);
        file = fopen(str, "r");
        if (file) {
                len = zbd_get_str(file, str);
                if (len)
                        n += snprintf(&zbdi->vendor_id[n],
                                      ZBD_VENDOR_ID_LENGTH - n,
                                      "%s ", str);
                fclose(file);
        }

        snprintf(str, sizeof(str),
                 "/sys/block/%s/device/rev",
                 devname);
        file = fopen(str, "r");
        if (file) {
                len = zbd_get_str(file, str);
                if (len)
                        n += snprintf(&zbdi->vendor_id[n],
                                      ZBD_VENDOR_ID_LENGTH - n,
                                      "%s", str);
                fclose(file);
        }

        return n > 0;
}

static struct zbd_info *zbd_do_get_info(int fd, char *devname)
{
	unsigned int zone_sectors, nr_zones;
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
	ret = ioctl(fd, BLKGETZONESZ, &zone_sectors);
	if (ret != 0) {
		zbd_error("ioctl BLKGETZONESZ failed %d (%s)\n",
			  errno, strerror(errno));
		goto err;
	}
	if (!zone_sectors) {
		zbd_error("Invalid 0 zone size\n");
		goto err;
	}
	zbdi->zone_sectors = zone_sectors;
	zbdi->zone_size = (unsigned long long)zone_sectors << SECTOR_SHIFT;

	/* Get number of zones */
	ret = ioctl(fd, BLKGETNRZONES, &nr_zones);
	if (ret != 0) {
		zbd_error("ioctl BLKGETNRZONES failed %d (%s)\n",
			  errno, strerror(errno));
		goto err;
	}
	if (!nr_zones) {
		zbd_error("Invalid 0 number of zones\n");
		goto err;
	}
	zbdi->nr_zones = nr_zones;

	/* Set limits to unknown for now */
	zbdi->max_nr_open_zones = -1;
	zbdi->max_nr_active_zones = -1;

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

        return fd;

err:
	if (fd >= 0)
                close(fd);
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
static bool zbd_should_report_zone(struct blk_zone *blkz,
				   enum zbd_report_option ro)
{
	switch (ro) {
	case ZBD_RO_ALL:
		return true;
	case ZBD_RO_NOT_WP:
		return zbd_zone_not_wp(blkz);
	case ZBD_RO_EMPTY:
		return zbd_zone_empty(blkz);
	case ZBD_RO_IMP_OPEN:
		return zbd_zone_imp_open(blkz);
	case ZBD_RO_EXP_OPEN:
		return zbd_zone_exp_open(blkz);
	case ZBD_RO_CLOSED:
		return zbd_zone_closed(blkz);
	case ZBD_RO_FULL:
		return zbd_zone_full(blkz);
	case ZBD_RO_RDONLY:
		return zbd_zone_rdonly(blkz);
	case ZBD_RO_OFFLINE:
		return zbd_zone_offline(blkz);
	case ZBD_RO_RWP_RECOMMENDED:
		return blkz->reset;
	case ZBD_RO_NON_SEQ:
		return blkz->non_seq;
	default:
		return false;
	}
}

/*
 * zbd_report_zone - Fill zone report
 */
static inline void zbd_report_zone(struct blk_zone *blkz,
				   struct blk_zone *zone)
{
	zone->start = blkz->start << SECTOR_SHIFT;
	zone->len = blkz->len << SECTOR_SHIFT;
	zone->wp = blkz->wp << SECTOR_SHIFT;
	zone->type = blkz->type;
	zone->cond = blkz->cond;
	zone->non_seq = blkz->non_seq;
	zone->reset = blkz->reset;
}

#define ZBD_NR_ZONE 8192

/**
 * zbd_report_zones - Get zone information
 */
int zbd_report_zones(int fd, off_t ofst, off_t len,
		     enum zbd_report_option ro,
		     struct blk_zone *zones, unsigned int *nr_zones)
{
	struct zbd_info *zbdi = zbd_get_fd(fd);
	unsigned long long zone_size_mask, end;
	struct blk_zone_report *rep;
	size_t rep_size;
	unsigned int n = 0, i = 0;
	struct blk_zone *blkz;
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

	ofst = (ofst & (~zone_size_mask)) >> SECTOR_SHIFT;
	if ((unsigned long long)ofst >= zbdi->nr_sectors) {
                *nr_zones = 0;
                return 0;
        }

        /* Get all zones information */
	rep_size = sizeof(struct blk_zone_report) +
		sizeof(struct blk_zone) * (ZBD_NR_ZONE);
	rep = (struct blk_zone_report *)malloc(rep_size);
	if (!rep) {
		zbd_error("%d: No memory for array of zones\n\n", fd);
		return -1;
	}

	blkz = (struct blk_zone *)(rep + 1);
	while (((!*nr_zones) || (n < *nr_zones)) &&
               ((unsigned long long)ofst < end)) {

		memset(rep, 0, rep_size);
		rep->sector = ofst;
		rep->nr_zones = ZBD_NR_ZONE;

		ret = ioctl(fd, BLKREPORTZONE, rep);
		if (ret != 0) {
			ret = -errno;
                     	zbd_error("%d: ioctl BLKREPORTZONE at %llu failed %d (%s)\n",
                                  fd,
                                  (unsigned long long)ofst,
                                  errno,
                                  strerror(errno));
                        goto out;
		}

		if (!rep->nr_zones)
                        break;

		for (i = 0; i < rep->nr_zones; i++) {

                        if ((*nr_zones && (n >= *nr_zones)) ||
			    ((unsigned long long)ofst >= end))
                                break;

			if (zbd_should_report_zone(&blkz[i], ro)) {
				if (zones)
					zbd_report_zone(&blkz[i], &zones[n]);
				n++;
			}

			ofst = blkz[i].start + blkz[i].len;
                }
        }

        /* Return number of zones */
        *nr_zones = n;

out:
	free(rep);

	return 0;
}

/**
 * zbd_list_zones - Get zone information
 */
int zbd_list_zones(int fd, off_t ofst, off_t len,
		   enum zbd_report_option ro,
		   struct blk_zone **pzones, unsigned int *pnr_zones)
{
	struct zbd_info *zbdi = zbd_get_fd(fd);
	struct blk_zone *zones = NULL;
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
	zones = (struct blk_zone *) calloc(nr_zones, sizeof(struct blk_zone));
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

/**
 * zbd_zone_operation - Execute an operation on a zone
 */
int zbd_zones_operation(int fd, enum zbd_zone_op op, off_t ofst, off_t len)
{
	struct zbd_info *zbdi = zbd_get_fd(fd);
	unsigned long long zone_size_mask, end;
	struct blk_zone_range range;
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

	ofst = (ofst & (~zone_size_mask)) >> SECTOR_SHIFT;
	if ((unsigned long long)ofst >= zbdi->nr_sectors ||
	    end == (unsigned long long)ofst) {
		printf("zone op %llu/%llu + %llu\n",
	       (unsigned long long)ofst,
	       (unsigned long long)zbdi->nr_sectors,
	       (unsigned long long)(end - ofst));
                return 0;
	}

	range.sector = ofst;
	range.nr_sectors = end - ofst;

	/* Execute the operation */
        switch (op) {
        case ZBD_OP_RESET:
          	ret = ioctl(fd, BLKRESETZONE, &range);
          	if (ret != 0){
			zbd_error("zone operation 0x%x failed %d (%s)\n",
				  op, errno, strerror(errno));
			return -1;
		}
		break;

	case ZBD_OP_OPEN:
#ifdef BLKOPENZONE
		ret = ioctl(fd, BLKOPENZONE, &range);
		if (ret != 0){
			zbd_error("zone operation 0x%x failed %d (%s)\n",
				  op, errno, strerror(errno));
			return -1;
		}
#else
		zbd_error("BLKCLOSEZONE ioctl is not supported\n");
		errno = -ENOTSUP;
		return -1;
#endif
		break;

	case ZBD_OP_CLOSE:
#ifdef BLKCLOSEZONE
		ret = ioctl(fd, BLKCLOSEZONE, &range);
		if (ret != 0){
			zbd_error("zone operation 0x%x failed %d (%s)\n",
				  op, errno, strerror(errno));
			return -1;
		}
#else
		zbd_error("BLKCLOSEZONE ioctl is not supported\n");
		errno = -ENOTSUP;
		return -1;
#endif
		break;

        case ZBD_OP_FINISH:
#ifdef BLKFINISHZONE
		ret = ioctl(fd, BLKFINISHZONE, &range);
		if (ret != 0){
			zbd_error("zone operation 0x%x failed %d (%s)\n",
				  op, errno, strerror(errno));
			return -1;
		}
#else
		zbd_error("BLKCLOSEZONE ioctl is not supported\n");
		errno = -ENOTSUP;
		return -1;
#endif
		break;

	default:
		zbd_error("Invalid zone operation 0x%x\n", op);
		errno = -EINVAL;
		return -1;
	}

	return 0;
}
