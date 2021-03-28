/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 *	    Ting Yao <tingyao@hust.edu.cn>
 */

#ifndef _LIBZBD_H_
#define _LIBZBD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/blkzoned.h>

/**
 * @brief Zone types
 *
 * @ZBD_ZONE_TYPE_CNV: The zone has no write pointer and can be writen
 *		       randomly. Zone reset has no effect on the zone.
 * @ZBD_ZONE_TYPE_SWR: The zone must be written sequentially
 * @ZBD_ZONE_TYPE_SWP: The zone can be written randomly
 */
enum zbd_zone_type {
	ZBD_ZONE_TYPE_CNV	= BLK_ZONE_TYPE_CONVENTIONAL,
	ZBD_ZONE_TYPE_SWR	= BLK_ZONE_TYPE_SEQWRITE_REQ,
	ZBD_ZONE_TYPE_SWP	= BLK_ZONE_TYPE_SEQWRITE_PREF,
};

/**
 * @brief Zone conditions (state)
 *
 * @ZBD_ZONE_COND_NOT_WP: The zone has no write pointer, it is conventional.
 * @ZBD_ZONE_COND_EMPTY: The zone is empty.
 * @ZBD_ZONE_COND_IMP_OPEN: The zone is open, but not explicitly opened.
 * @ZBD_ZONE_COND_EXP_OPEN: The zones was explicitly opened by an
 *			    OPEN ZONE command.
 * @ZBD_ZONE_COND_CLOSED: The zone was [explicitly] closed after writing.
 * @ZBD_ZONE_COND_FULL: The zone is marked as full, possibly by a zone
 *			FINISH ZONE command.
 * @ZBD_ZONE_COND_READONLY: The zone is read-only.
 * @ZBD_ZONE_COND_OFFLINE: The zone is offline (dead).
 */
enum zbd_zone_cond {
	ZBD_ZONE_COND_NOT_WP	= BLK_ZONE_COND_NOT_WP,
	ZBD_ZONE_COND_EMPTY	= BLK_ZONE_COND_EMPTY,
	ZBD_ZONE_COND_IMP_OPEN	= BLK_ZONE_COND_IMP_OPEN,
	ZBD_ZONE_COND_EXP_OPEN	= BLK_ZONE_COND_EXP_OPEN,
	ZBD_ZONE_COND_CLOSED	= BLK_ZONE_COND_CLOSED,
	ZBD_ZONE_COND_FULL	= BLK_ZONE_COND_FULL,
	ZBD_ZONE_COND_READONLY	= BLK_ZONE_COND_READONLY,
	ZBD_ZONE_COND_OFFLINE	= BLK_ZONE_COND_OFFLINE,
};

/**
 * @brief Zone flags
 *
 * @ZBD_ZONE_RWP_RECOMMENDED: The zone should be reset.
 * @ZBD_ZONE_NON_SEQ: The zone is using non-sequential write resources.
 */
enum zbd_zone_flags {
	ZBD_ZONE_RWP_RECOMMENDED	= (1U << 0),
	ZBD_ZONE_NON_SEQ_RESOURCES	= (1U << 1),
};

/**
 * @brief Zone descriptor data structure
 *
 * Provide information on a zone with all position and size values in bytes.
 */
struct zbd_zone {
	unsigned long long	start;		/* Zone start */
	unsigned long long	len;		/* Zone length */
	unsigned long long	capacity;	/* Zone capacity */
	unsigned long long	wp;		/* Zone write pointer */
	unsigned int		flags;		/* Zone state flags */
	unsigned int		type;		/* Zone type */
	unsigned int		cond;		/* Zone condition */
	uint8_t			reserved[20];	/* Padding to 64B */
} __attribute__((packed));

/**
 * @brief Library log levels
 */
enum zbd_log_level {
	ZBD_LOG_NONE = 0,	/* Disable all messages */
	ZBD_LOG_ERROR,		/* Output details about errors */
	ZBD_LOG_DEBUG,		/* Debug-level messages */
};

/**
 * @brief Set the library log level
 * @param[in] log_level	Library log level
 *
 * Set the library log level using the level name specified by \a log_level.
 * Log level are incremental: each level includes the levels preceding it.
 * Valid log level names are:
 * "none"    : Silent operation (no messages)
 * "warning" : Print device level standard compliance problems
 * "error"   : Print messages related to unexpected errors
 * "info"    : Print normal information messages
 * "debug"   : Verbose output decribing internally executed commands
 * The default level is "warning".
 */
extern void zbd_set_log_level(enum zbd_log_level);

/**
 * @brief Block device zone models.
 */
enum zbd_dev_model {
	ZBD_DM_HOST_MANAGED = 1,
	ZBD_DM_HOST_AWARE,
	ZBD_DM_NOT_ZONED,
};

/**
 * @brief Device information vendor id string maximum length.
 */
#define ZBD_VENDOR_ID_LENGTH	32

/**
 * @brief Device information data structure
 *
 * Provide information on a device open using the \a zbd_open function.
 */
struct zbd_info {

	/**
	 * Device vendor, model and firmware revision string.
	 */
	char			vendor_id[ZBD_VENDOR_ID_LENGTH];

	/**
	 * Total number of 512B sectors of the device.
	 */
	unsigned long long	nr_sectors;

	/**
	 * Total number of logical blocks of the device.
	 */
	unsigned long long	nr_lblocks;

	/**
	 * Total number of physical blocks of the device.
	 */
	unsigned long long	nr_pblocks;

	/**
	 * Size in bytes of a zone.
	 */
	unsigned long long	zone_size;

	/**
	 * Size in 512B sectors of a zone.
	 */
	unsigned int		zone_sectors;

	/**
	 * Size in bytes of the device logical blocks.
	 */
	unsigned int		lblock_size;

	/**
	 * Size in bytes of the device physical blocks.
	 */
	unsigned int		pblock_size;

	/**
	 * Number of zones.
	 */
	unsigned int		nr_zones;

	/**
	 * Maximum number of explicitely open zones. A value of 0 means that
	 * the device has no limit. A value of -1 means that the value is
	 * unknown.
	 */
	unsigned int		max_nr_open_zones;

	/**
	 * Maximum number of active zones. A value of 0 means that the device
	 * has no limit. A value of -1 means that the value is unknown.
	 */
	unsigned int		max_nr_active_zones;

	/**
	 * Device zone model.
	 */
	unsigned int		model;

	/**
	 * Padding to 128B.
	 */
	uint8_t			reserved[36];
} __attribute__((packed));

/**
 * @brief Test if a device is a zoned block device
 * @param[in] filename	Path to the device file
 * @param[in] info	Address where to store the device information
 *
 * Test if a device supports the ZBD/ZAC command set. If \a fake is false,
 * only test physical devices. Otherwise, also test regular files and
 * regular block devices that may be in use with the fake backend driver
 * to create an emulated host-managed zoned block device.
 * If \a info is not NULL and the device is identified as a zoned
 * block device, the device information is returned at the address
 * specified by \a info.
 *
 * @return Returns a negative error code if the device test failed.
 * 1 is returned if the device is identified as a zoned zoned block device.
 * Otherwise, 0 is returned.
 */
extern int zbd_device_is_zoned(const char *filename);


/**
 * @brief Open a ZBD device
 * @param[in] filename	Path to a device file
 * @param[in] flags	Device file open flags
 * @param[out] info	Device information
 *
 * Opens the device specified by \a filename, and returns a file descriptor
 * number similarly to the regular open() system call. If @info is non-null,
 * information on the device is returned at the specified address.
 *
 * @return If the device is not a zoned block device, -ENXIO is returned.
 * Any other error code returned by open(2) can be returned as well.
 */
extern int zbd_open(const char *filename, int flags, struct zbd_info *info);

/**
 * @brief Close a ZBD device
 * @param[in] fd	File descriptor obtained with \a zbd_open
 *
 * Performs the equivalent to close(2) for a zoned block device open
 * using \a zbd_open.
 */
extern void zbd_close(int fd);

/**
 * @brief Get a device information
 * @param[in] fd	File descriptor obtained with \a zbd_open
 * @param[in] info	Address of the information structure to fill
 *
 * Get information about an open device. The \a info parameter is used to
 * return a device information. \a info must be allocated by the caller.
 */
extern int zbd_get_info(int fd, struct zbd_info *info);

/**
 * @brief Reporting options definitions
 *
 * Used to filter the zone information returned by the execution of a
 * REPORT ZONES command. Filtering is based on the value of the reporting
 * option and on the condition of the zones at the time of the execution of
 * the REPORT ZONES command.
 *
 * ZBD_RO_PARTIAL is not a filter: this reporting option can be combined
 * (or'ed) with any other filter option to limit the number of reported
 * zone information to the size of the REPORT ZONES command buffer.
 */
enum zbd_report_option {
	/* Report all zones */
	ZBD_RO_ALL		= 0x00,

	/* Report only empty zones */
	ZBD_RO_EMPTY		= 0x01,

	/* Report only implicitly open zones */
	 ZBD_RO_IMP_OPEN	= 0x02,

	/* Report only explicitly open zones */
	ZBD_RO_EXP_OPEN		= 0x03,

	/* Report only closed zones */
	ZBD_RO_CLOSED		= 0x04,

	/* Report only full zones */
	ZBD_RO_FULL		= 0x05,

	/* Report only read-only zones */
	ZBD_RO_RDONLY		= 0x06,

	/* Report only offline zones */
	ZBD_RO_OFFLINE		= 0x07,

	/* Report only zones with reset recommended flag set */
	ZBD_RO_RWP_RECOMMENDED	= 0x10,

	/* Report only zones with the non-sequential resource used flag set */
	ZBD_RO_NON_SEQ		= 0x11,

	/* Report only conventional zones (non-write-pointer zones) */
	ZBD_RO_NOT_WP		= 0x3f,
};

/**
 * @brief Get zone information
 * @param[in] fd	File descriptor obtained with \a zbd_open
 * @param[in] ofst	Byte offset from which to report zones
 * @param[in] len	Maximum length in bytes from \a ofst of the device
 *                      capacity range to inspect for the report
 * @param[in] ro	Reporting options
 * @param[in] zones	Pointer to the array of zone information to fill
 * @param[out] nr_zones	Number of zones in the array \a zones
 *
 * Get zone information of at most \a nr_zones zones in the range
 * [ofst..ofst+len] and matching the \a ro option. If \a len is 0,
 * at most \a nr_zones zones starting from \a ofst up to the end on the device
 * capacity will be reported.
 * Return the zone information obtained in the array \a zones and the number
 * of zones reported at the address specified by \a nr_zones.
 * The array \a zones must be allocated by the caller and \a nr_zones
 * must point to the size of the allocated array (number of zone information
 * structures in the array). The first zone reported will be the zone
 * containing or after \a ofst. The last zone reported will be the zone
 * containing or before \a ofst + \a len.
 *
 * @return Returns 0 on success and -1 otherwise.
 */
extern int zbd_report_zones(int fd, off_t ofst, off_t len,
			    enum zbd_report_option ro,
			    struct zbd_zone *zones, unsigned int *nr_zones);

/**
 * @brief Get number of zones
 * @param[in] fd	File descriptor obtained with \a zbd_open
 * @param[in] ofst	Byte offset from which to report zones
 * @param[in] len	Maximum length in bytes from \a ofst of the device
 *                      capacity range to inspect for the report
 * @param[in] ro	Reporting options
 * @param[out] nr_zones	The number of matching zones
 *
 * Similar to \a zbd_report_zones, but returns only the number of zones that
 * \a zbd_report_zones would have returned. This is useful to determine the
 * number of zones of a device to allocate an array of zone information
 * structures for use with \a zbd_report_zones.
 *
 * @return Returns 0 on success and -1 otherwise.
 */
static inline int zbd_report_nr_zones(int fd, off_t ofst, off_t len,
					enum zbd_report_option ro,
					unsigned int *nr_zones)
{
	return zbd_report_zones(fd, ofst, len, ro, NULL, nr_zones);
}

/**
 * @brief Get zone information
 * @param[in] fd	File descriptor obtained with \a zbd_open
 * @param[in] ofst	Byte offset from which to report zones
 * @param[in] len	Maximum length in bytes from \a ofst of the device
 *                      capacity range to inspect for the report
 * @param[in] ro	Reporting options
 * @param[out] zones	The array of zone information filled
 * @param[out] nr_zones	Number of zones in the array \a zones
 *
 * Similar to \a zbd_report_zones, but also allocates an appropriatly sized
 * array of zone information structures and return the address of the array
 * at the address specified by \a zones. The size of the array allocated and
 * filled is returned at the address specified by \a nr_zones. Freeing of the
 * memory used by the array of zone information structures allocated by this
 * function is the responsability of the user.
 *
 * @return Returns 0 on success and -1 otherwise.
 * Returns -ENOMEM if memory could not be allocated for \a zones.
 */
extern int zbd_list_zones(int fd, off_t ofst, off_t len,
			  enum zbd_report_option ro,
			  struct zbd_zone **zones, unsigned int *nr_zones);

/**
 * @brief Zone management operations.
 */
enum zbd_zone_op {
	/**
	 * Reset zones write pointer.
	 */
	ZBD_OP_RESET	= 0x01,
	/**
	 * Explicitly open zones.
	 */
	ZBD_OP_OPEN	= 0x02,
	/**
	 * Close opened zones.
	 */
	ZBD_OP_CLOSE	= 0x03,
	/**
	 * Transition zones to full state.
	 */
	ZBD_OP_FINISH	= 0x04,
};

/**
 * @brief Execute an operation on a range of zones
 * @param[in] fd	File descriptor obtained with \a zbd_open
 * @param[in] ofst	Byte offset identifying the first zone to operate on
 * @param[in] len	Maximum length in bytes from \a ofst of the set of zones
 *                      to operate on
 * @param[in] op	The operation to perform
 *
 * Exexcute an operation on the range of zones defined by [ofst..ofst+len]
 * If \a len is 0, all zones from \a ofst will be processed.
 * The validity of the operation (reset, open, close or finish) depends on the
 * type and condition of the target zones.
 *
 * @return Returns 0 on success and -1 otherwise.
 */
extern int zbd_zones_operation(int fd, enum zbd_zone_op op,
			       off_t ofst, off_t len);

/**
 * @brief Reset the write pointer of a range of zones
 * @param[in] fd	File descriptor obtained with \a zbd_open
 * @param[in] ofst	Byte offset identifying the first zone to reset
 * @param[in] len	Maximum length in bytes from \a ofst of the set of zones
 *                      to reset
 *
 * Resets the write pointer of the zones in the range [ofst..ofst+len].
 * If \a len is 0, all zones from \a ofst will be processed.
 *
 * @return Returns 0 on success and -1 otherwise.
 */
static inline int zbd_reset_zones(int fd, off_t ofst, off_t len)
{
	return zbd_zones_operation(fd, ZBD_OP_RESET, ofst, len);
}

/**
 * @brief Explicitly open a range of zones
 * @param[in] fd	File descriptor obtained with \a zbd_open
 * @param[in] ofst	Byte offset identifying the first zone to open
 * @param[in] len	Maximum length in bytes from \a ofst of the set of zones
 *                      to open
 *
 * Explicitly open the zones in the range [ofst..ofst+len]. If \a len is 0,
 * all zones from \a ofst will be processed.
 *
 * @return Returns 0 on success and -1 otherwise.
 */
static inline int zbd_open_zones(int fd, off_t ofst, off_t len)
{
	return zbd_zones_operation(fd, ZBD_OP_OPEN, ofst, len);
}

/**
 * @brief Close a range of zones
 * @param[in] fd	File descriptor obtained with \a zbd_open
 * @param[in] ofst	Byte offset identifying the first zone to close
 * @param[in] len	Maximum length in bytes from \a ofst of the set of zones
 *                      to close
 *
 * Close the zones in the range [ofst..ofst+len].
 * If \a len is 0, all zones from \a ofst will be processed.
 *
 * @return Returns 0 on success and -1 otherwise.
 */
static inline int zbd_close_zones(int fd, off_t ofst, off_t len)
{
	return zbd_zones_operation(fd, ZBD_OP_CLOSE, ofst, len);
}

/**
 * @brief Finish a range of zones
 * @param[in] fd	File descriptor obtained with \a zbd_open
 * @param[in] ofst	Byte offset identifying the first zone to finish
 * @param[in] len	Maximum length in bytes from \a ofst of the set of zones
 *                      to finish
 *
 * Finish the zones in the range [ofst..ofst+len].
 * If \a len is 0, all zones from \a ofst will be processed.
 *
 * @return Returns 0 on success and -1 otherwise.
 */
static inline int zbd_finish_zones(int fd, off_t ofst, off_t len)
{
	return zbd_zones_operation(fd, ZBD_OP_FINISH, ofst, len);
}

/**
 * Accessors
 */
#define zbd_zone_type(z)	((z)->type)
#define zbd_zone_cnv(z)		((z)->type == ZBD_ZONE_TYPE_CNV)
#define zbd_zone_swr(z)		((z)->type == ZBD_ZONE_TYPE_SWR)
#define zbd_zone_swp(z)		((z)->type == ZBD_ZONE_TYPE_SWP)
#define zbd_zone_seq(z)		(zbd_zone_swr(z) || zbd_zone_swp(z))

#define zbd_zone_cond(z)	((z)->cond)
#define zbd_zone_not_wp(z)	((z)->cond == ZBD_ZONE_COND_NOT_WP)
#define zbd_zone_empty(z)	((z)->cond == ZBD_ZONE_COND_EMPTY)
#define zbd_zone_imp_open(z)	((z)->cond == ZBD_ZONE_COND_IMP_OPEN)
#define zbd_zone_exp_open(z)	((z)->cond == ZBD_ZONE_COND_EXP_OPEN)
#define zbd_zone_is_open(z)	(zbd_zone_imp_open(z) || zbd_zone_exp_open(z))
#define zbd_zone_closed(z)	((z)->cond == ZBD_ZONE_COND_CLOSED)
#define zbd_zone_is_active(z)	(zbd_zone_is_open(z) || zbd_zone_closed(z))
#define zbd_zone_full(z)	((z)->cond == ZBD_ZONE_COND_FULL)
#define zbd_zone_rdonly(z)	((z)->cond == ZBD_ZONE_COND_READONLY)
#define zbd_zone_offline(z)	((z)->cond == ZBD_ZONE_COND_OFFLINE)

#define zbd_zone_start(z)	((z)->start)
#define zbd_zone_len(z)		((z)->len)
#define zbd_zone_capacity(z)	((z)->capacity)
#define zbd_zone_wp(z)		((z)->wp)

#define zbd_zone_flags(z)	((z)->flags)
#define zbd_zone_rwp_recommended(z)   ((z)->flags & ZBD_ZONE_RWP_RECOMMENDED)
#define zbd_zone_non_seq_resources(z) ((z)->flags & ZBD_ZONE_NON_SEQ_RESOURCES)

/**
 * @brief Returns a string describing a device zone model
 * @param[in] model	Device model
 * @param[in] s		Get abbreviated name
 *
 * Return a string (long or abbreviated) describing a device zone model.
 *
 * @return Device model string or NULL for an invalid model.
 */
extern const char *zbd_device_model_str(enum zbd_dev_model model, bool s);

/**
 * @brief Returns a string describing a zone type
 * @param[in] z		Zone descriptor
 * @param[in] s		Get abbreviated zone type name
 *
 * Return a string (long or abbreviated) describing a zone type.
 *
 * @return Zone type string or NULL for an invalid zone type.
 */
extern const char *zbd_zone_type_str(struct zbd_zone *z, bool s);

/**
 * @brief Returns a string describing a zone condition
 * @param[in] z		Zone descriptor
 * @param[in] s		Get abbreviated zone condition name
 *
 * Return a string (long or abbreviated) describing a zone condition.
 *
 * @return Zone type string or NULL for an invalid zone condition.
 */
extern const char *zbd_zone_cond_str(struct zbd_zone *z, bool s);

#ifdef __cplusplus
}
#endif

#endif /* _LIBZBD_H_ */
