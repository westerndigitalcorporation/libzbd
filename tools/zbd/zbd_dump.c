// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * SPDX-FileCopyrightText: 2020 Western Digital Corporation or its affiliates.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 */
#include "./zbd.h"

static ssize_t zbd_rw(int fd, bool read, void *buf, size_t count, off_t offset)
{
	size_t remaining = count;
	off_t ofst = offset;
	ssize_t ret;

	while (remaining) {
		if (read)
			ret = pread(fd, buf, remaining, ofst);
		else
			ret = pwrite(fd, buf, remaining, ofst);
		if (ret < 0) {
			fprintf(stderr, "%s failed %d (%s)\n",
				read ? "read" : "write",
				errno, strerror(errno));
			return -1;
		}
		if (!ret)
			break;

		remaining -= ret;
		ofst += ret;
	}

	return count - remaining;
}

static inline ssize_t zbd_read(int fd, void *buf, size_t count, off_t offset)
{
	return zbd_rw(fd, true, buf, count, offset);
}

static inline ssize_t zbd_write(int fd, void *buf, size_t count, off_t offset)
{
	return zbd_rw(fd, false, buf, count, offset);
}

int zbd_open_dump(struct zbd_opts *opts)
{
	struct zbd_dump dump;
	struct stat st;
	int dev_fd = 0;
	ssize_t ret;

	ret = stat(opts->dev_path, &st);
	if (ret) {
		fprintf(stderr, "stat %s failed\n", opts->dev_path);
		return -1;
	}

	if (!S_ISREG(st.st_mode))
		return 0;

	printf("Regular file specified: assuming dump file\n");

	dev_fd = open(opts->dev_path, O_RDONLY | O_LARGEFILE);
	if (dev_fd < 0) {
		fprintf(stderr, "Open %s failed (%s)\n",
			opts->dev_path, strerror(errno));
		return -1;
	}

	ret = zbd_read(dev_fd, &dump, sizeof(struct zbd_dump), 0);
	if (ret != sizeof(struct zbd_dump)) {
		fprintf(stderr, "Read dump header failed\n");
		close(dev_fd);
		return -1;
	}

	memcpy(&opts->dev_info, &dump.dev_info, sizeof(struct zbd_info));
	opts->rep_dump = true;

	return dev_fd;
}

static bool zbd_dump_should_report_zone(struct zbd_zone *zone,
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

int zbd_dump_report_zones(int fd, struct zbd_opts *opts,
			  struct zbd_zone *zones, unsigned int *nr_zones)
{
	unsigned int i, nz = 0, zstart, zend;
	struct zbd_zone zone;
	loff_t ofst;
	ssize_t ret;

	zstart = opts->ofst / opts->dev_info.zone_size;
	zend = (opts->ofst + opts->len + opts->dev_info.zone_size - 1)
		/ opts->dev_info.zone_size;

	ofst = sizeof(struct zbd_dump) + zstart * sizeof(struct zbd_zone);
	for (i = zstart; i < zend; i++) {
		ret = zbd_read(fd, &zone, sizeof(struct zbd_zone), ofst);
		if (ret != sizeof(struct zbd_zone)) {
			fprintf(stderr, "Read zone information failed\n");
			return -1;
		}

		if (zbd_dump_should_report_zone(&zone, opts->rep_opt)) {
			memcpy(&zones[nz], &zone, sizeof(struct zbd_zone));
			nz++;
		}

		ofst += sizeof(struct zbd_zone);
	}

	*nr_zones = nz;

	return 0;
}

#define ZBD_DUMP_IO_SIZE	(1024 * 1024)

static ssize_t zbd_dump_one_zone(int fd, struct zbd_opts *opts,
				 struct zbd_zone *zone, int dump_fd, void *buf)
{
	long long ofst, end;
	ssize_t ret, iosize;

	/* Ignore offline zones */
	if (zbd_zone_offline(zone))
		return 0;

	/* Copy zone data */
	ofst = zbd_zone_start(zone);
	if (zbd_zone_seq(zone) && !zbd_zone_full(zone))
		end = zbd_zone_wp(zone);
	else
		end = ofst + zbd_zone_capacity(zone);

	while (ofst < end) {

		if (ofst + ZBD_DUMP_IO_SIZE > end)
			iosize = end - ofst;
		else
			iosize = ZBD_DUMP_IO_SIZE;

		ret = zbd_read(fd, buf, iosize, ofst);
		if (ret != iosize) {
			fprintf(stderr, "Read zone data failed\n");
			return -1;
		}

		ret = zbd_write(dump_fd, buf, iosize, ofst);
		if (ret != iosize) {
			fprintf(stderr, "Write zone data failed\n");
			return -1;
		}

		ofst += iosize;
	}

	return end - zbd_zone_start(zone);
}

static int zbd_dump_zone_data(int fd, struct zbd_opts *opts,
			      struct zbd_zone *zones, struct zbd_dump *dump)
{
	long long dumped_bytes = 0;
	unsigned int dumped_zones = 0;
	char *data_path = NULL;
	int data_fd = 0;
	unsigned int i;
	ssize_t ret;
	void *buf;

	/* Get an IO buffer */
	ret = posix_memalign(&buf, sysconf(_SC_PAGESIZE), ZBD_DUMP_IO_SIZE);
	if (ret) {
		fprintf(stderr, "No memory\n");
		return -1;
	}

	/* Dump zone data */
	ret = asprintf(&data_path, "%s/%s_zone_data.dump",
		       opts->dump_path, opts->dump_prefix);

	if (ret < 0) {
		fprintf(stderr, "No memory\n");
		goto out;
	}

	printf("    Dumping zones [%u..%u] data to %s (this may take a while)...\n",
	       dump->zstart, dump->zend - 1, data_path);

	data_fd = open(data_path, O_WRONLY | O_LARGEFILE | O_TRUNC | O_CREAT,
		       0644);
	if (data_fd < 0) {
		fprintf(stderr, "Create data file %s failed %d (%s)\n",
			data_path, errno, strerror(errno));
		ret = -1;
		goto out;
	}

	/*
	 * Make sure that the zone data dump file size is always equal
	 * to the device capacity, even for partial dumps.
	 */
	ret = ftruncate(data_fd, opts->dev_info.nr_sectors << 9);
	if (ret) {
		fprintf(stderr, "Truncate data file %s failed %d (%s)\n",
			data_path, errno, strerror(errno));
		goto out;
	}

	for (i = dump->zstart; i < dump->zend; i++) {
		ret = zbd_dump_one_zone(fd, opts, &zones[i], data_fd, buf);
		if (ret < 0)
			goto out;
		if (ret) {
			dumped_bytes += ret;
			dumped_zones++;
		}
	}

	printf("    Dumped %lld B from %u zones\n",
	       dumped_bytes, dumped_zones);

	ret = fsync(data_fd);
	if (ret)
		fprintf(stderr, "fsync data file %s failed %d (%s)\n",
			data_path, errno, strerror(errno));

out:
	if (data_fd > 0)
		close(data_fd);
	free(data_path);
	free(buf);

	return ret;
}

static int zbd_dump_zone_info(int fd, struct zbd_opts *opts,
			      struct zbd_zone *zones, struct zbd_dump *dump)
{
	char *info_path = NULL;
	int info_fd = 0;
	ssize_t ret, sz;

	/* Dump zone information */
	ret = asprintf(&info_path, "%s/%s_zone_info.dump",
		       opts->dump_path, opts->dump_prefix);
	if (ret < 0) {
		fprintf(stderr, "No memory\n");
		return -1;
	}

	printf("    Dumping zone information to %s\n", info_path);

	info_fd = open(info_path, O_WRONLY | O_LARGEFILE | O_TRUNC | O_CREAT,
		       0644);
	if (info_fd < 0) {
		fprintf(stderr, "Create file %s failed %d (%s)\n",
			info_path, errno, strerror(errno));
		ret = -1;
		goto out;
	}

	ret = zbd_write(info_fd, dump, sizeof(struct zbd_dump), 0);
	if (ret != (ssize_t)sizeof(struct zbd_dump)) {
		fprintf(stderr, "Write dump header failed\n");
		ret = -1;
		goto out;
	}

	sz = sizeof(struct zbd_zone) * opts->dev_info.nr_zones;
	ret = zbd_write(info_fd, zones, sz, sizeof(struct zbd_dump));
	if (ret != sz) {
		fprintf(stderr, "Write zone information failed\n");
		ret = -1;
		goto out;
	}

	ret = fsync(info_fd);
	if (ret)
		fprintf(stderr, "fsync zone information file %s failed %d (%s)\n",
			info_path, errno, strerror(errno));

out:
	if (info_fd > 0)
		close(info_fd);
	free(info_path);

	return ret;
}

static void zbd_dump_prep_path(struct zbd_opts *opts)
{
	if (!opts->dump_path) {
		opts->dump_path = get_current_dir_name();
		if (!opts->dump_path)
			opts->dump_path = ".";
	}

	if (!opts->dump_prefix)
		opts->dump_prefix = basename(opts->dev_path);
}

int zbd_dump(int fd, struct zbd_opts *opts)
{
	struct zbd_zone *zones = NULL;
	struct zbd_dump dump;
	unsigned int nz;
	int ret;

	zbd_dump_prep_path(opts);

	/* Setup dump header */
	memset(&dump, 0, sizeof(struct zbd_dump));
	memcpy(&dump.dev_info, &opts->dev_info, sizeof(struct zbd_info));
	dump.zstart = opts->ofst / opts->dev_info.zone_size;
	dump.zend = (opts->ofst + opts->len + opts->dev_info.zone_size - 1)
		/ opts->dev_info.zone_size;

	/* Get zone information */
	ret = zbd_list_zones(fd, 0, 0, ZBD_RO_ALL, &zones, &nz);
	if (ret != 0) {
		fprintf(stderr, "zbd_list_zones() failed %d\n", ret);
		return 1;
	}
	if (nz != opts->dev_info.nr_zones) {
		fprintf(stderr,
			"Invalid number of zones: expected %u, got %u\n",
			opts->dev_info.nr_zones, nz);
		ret = 1;
		goto out;
	}

	printf("%s: %u zones\n", opts->dev_path, opts->dev_info.nr_zones);

	/* Dump zone information and zone data */
	ret = zbd_dump_zone_data(fd, opts, zones, &dump);
	if (ret)
		goto out;

	ret = zbd_dump_zone_info(fd, opts, zones, &dump);

out:
	free(zones);
	return ret;
}

struct zbd_restore {
	int		data_fd;
	struct zbd_info	dev_info;
	struct zbd_zone *dump_zones;
	struct zbd_zone *dev_zones;
	unsigned int zstart;
	unsigned int zend;
	void *buf;
	long long restored_bytes;
	unsigned int restored_zones;
};

static int zbd_load_zone_info(struct zbd_restore *ropts,
			      struct zbd_opts *opts)
{
	struct zbd_dump dump;
	struct zbd_zone *devz, *dumpz;
	unsigned int nr_open_zones = 0;
	unsigned int nr_active_zones = 0;
	char *info_path = NULL;
	ssize_t ret, sz;
	unsigned int i;
	int info_fd;

	/* Dump zone information */
	ret = asprintf(&info_path, "%s/%s_zone_info.dump",
		       opts->dump_path, opts->dump_prefix);
	if (ret < 0) {
		fprintf(stderr, "No memory\n");
		return -1;
	}

	printf("    Getting zone information from %s\n", info_path);

	info_fd = open(info_path, O_RDONLY | O_LARGEFILE);
	if (info_fd < 0) {
		fprintf(stderr,
			"Open zone information dump file %s failed %d (%s)\n",
			info_path, errno, strerror(errno));
		ret = -1;
		goto out;
	}

	/* Read dump header */
	ret = zbd_read(info_fd, &dump, sizeof(struct zbd_dump), 0);
	if (ret != (ssize_t)sizeof(struct zbd_dump)) {
		fprintf(stderr, "Read dump header failed\n");
		ret = -1;
		goto out;
	}
	memcpy(&ropts->dev_info, &dump.dev_info, sizeof(struct zbd_info));
	ropts->zstart = dump.zstart;
	ropts->zend = dump.zend;

	/* Check device information against target device */
	ret = -1;
	if (ropts->dev_info.nr_sectors != opts->dev_info.nr_sectors) {
		fprintf(stderr, "Incompatible capacity\n");
		goto out;
	}
	if (ropts->dev_info.lblock_size != opts->dev_info.lblock_size) {
		fprintf(stderr, "Incompatible logical block size\n");
		goto out;
	}
	if (ropts->dev_info.pblock_size != opts->dev_info.pblock_size) {
		fprintf(stderr, "Incompatible physical block size\n");
		goto out;
	}
	if (ropts->dev_info.nr_zones != opts->dev_info.nr_zones) {
		fprintf(stderr, "Incompatible number of zones\n");
		goto out;
	}
	if (ropts->dev_info.zone_size != opts->dev_info.zone_size) {
		fprintf(stderr, "Incompatible zone size\n");
		goto out;
	}

	/* Read dumped zone information */
	ropts->dump_zones = calloc(ropts->dev_info.nr_zones,
				  sizeof(struct zbd_zone));
	if (!ropts->dump_zones) {
		fprintf(stderr, "No memory\n");
		goto out;
	}

	sz = sizeof(struct zbd_zone) * ropts->dev_info.nr_zones;
	ret = zbd_read(info_fd, ropts->dump_zones, sz, sizeof(struct zbd_dump));
	if (ret != sz) {
		fprintf(stderr, "Read zone information failed %zd %zd\n",
			ret, sz);
		ret = -1;
		goto out;
	}

	/* Check zones against target device zones */
	for (i = 0; i < ropts->dev_info.nr_zones; i++) {
		dumpz = &ropts->dump_zones[i];
		devz = &ropts->dev_zones[i];

		ret = -1;
		if (zbd_zone_type(dumpz) != zbd_zone_type(devz)) {
			fprintf(stderr, "Incompatible zone %u type\n", i);
			goto out;
		}
		if (zbd_zone_start(dumpz) != zbd_zone_start(devz)) {
			fprintf(stderr, "Incompatible zone %u start\n", i);
			goto out;
		}
		if (zbd_zone_len(dumpz) != zbd_zone_len(devz)) {
			fprintf(stderr, "Incompatible zone %u start\n", i);
			goto out;
		}
		if (zbd_zone_capacity(dumpz) != zbd_zone_capacity(devz)) {
			fprintf(stderr, "Incompatible zone %u start\n", i);
			goto out;
		}
		if (zbd_zone_offline(devz) && !zbd_zone_offline(dumpz)) {
			fprintf(stderr, "Incompatible offline zone %u\n", i);
			goto out;
		}
		if (zbd_zone_rdonly(devz)) {
			fprintf(stderr, "Incompatible read-only zone %u\n", i);
			goto out;
		}

		/* Count open and active zones */
		if (zbd_zone_is_open(dumpz))
			nr_open_zones++;
		if (zbd_zone_is_active(dumpz))
			nr_active_zones++;
	}

	/*
	 * Check that the target drive has enough open and
	 * active zones resource.
	 */
	if (opts->dev_info.max_nr_open_zones &&
	    nr_open_zones > opts->dev_info.max_nr_open_zones) {
		fprintf(stderr,
			"Incompatible maximum number of open zones\n");
		goto out;
	}
	if (opts->dev_info.max_nr_active_zones &&
	    nr_active_zones > opts->dev_info.max_nr_active_zones) {
		fprintf(stderr,
			"Incompatible maximum number of active zones\n");
		goto out;
	}

	ret = 0;
out:

	free(info_path);
	close(info_fd);

	return ret;
}

static int zbd_open_zone_data(struct zbd_restore *ropts,
			      struct zbd_opts *opts)
{
	char *data_path = NULL;
	struct stat st;
	int ret;

	/* Dump zone information */
	ret = asprintf(&data_path, "%s/%s_zone_data.dump",
		       opts->dump_path, opts->dump_prefix);
	if (ret < 0) {
		fprintf(stderr, "No memory\n");
		return -1;
	}

	printf("    Restoring zones [%u..%u] data from %s (this may take a while)...\n",
	       ropts->zstart, ropts->zend - 1, data_path);

	ropts->data_fd = open(data_path, O_RDONLY | O_LARGEFILE);
	if (ropts->data_fd < 0) {
		fprintf(stderr,
			"Open zone data dump file %s failed %d (%s)\n",
			data_path, errno, strerror(errno));
		ret = -1;
		goto out;
	}

	/* Check zone data file size */
	ret = fstat(ropts->data_fd, &st);
	if (ret) {
		fprintf(stderr,
			"stat zone data dump file %s failed %d (%s)\n",
			data_path, errno, strerror(errno));
		goto out;
	}

	if ((unsigned long long)st.st_size != opts->dev_info.nr_sectors << 9) {
		fprintf(stderr, "Invalid zone data dump file size\n");
		ret = -1;
	}

out:
	free(data_path);

	return ret;
}

static long long zbd_restore_zone_data(int fd, struct zbd_restore *ropts,
				       struct zbd_zone *dumpz)
{
	long long ofst, end;
	ssize_t ret, iosize;

	/* Copy zone dump data */
	ofst = zbd_zone_start(dumpz);
	if (zbd_zone_seq(dumpz) && !zbd_zone_full(dumpz))
		end = zbd_zone_wp(dumpz);
	else
		end = ofst + zbd_zone_capacity(dumpz);

	while (ofst < end) {

		if (ofst + ZBD_DUMP_IO_SIZE > end)
			iosize = end - ofst;
		else
			iosize = ZBD_DUMP_IO_SIZE;

		ret = zbd_read(ropts->data_fd, ropts->buf, iosize, ofst);
		if (ret != iosize) {
			fprintf(stderr, "Read zone dump data failed\n");
			return -1;
		}

		ret = zbd_write(fd, ropts->buf, iosize, ofst);
		if (ret != iosize) {
			fprintf(stderr, "Write zone data failed\n");
			return -1;
		}

		ofst += iosize;
	}

	return end - zbd_zone_start(dumpz);
}

static int zbd_restore_one_zone(int fd, struct zbd_restore *ropts,
				struct zbd_zone *dumpz, struct zbd_zone *devz)
{
	long long restored_bytes;
	int ret;

	/* Copy zone data */
	restored_bytes = zbd_restore_zone_data(fd, ropts, dumpz);
	if (restored_bytes < 0)
		return restored_bytes;

	if (restored_bytes) {
		ropts->restored_bytes += restored_bytes;
		ropts->restored_zones++;
	}

	/* Restore zone condition */
	if (zbd_zone_closed(dumpz)) {
		ret = zbd_close_zones(fd, zbd_zone_start(devz),
				      zbd_zone_len(devz));
		if (ret) {
			fprintf(stderr,
				"Close target zone at %llu failed %d (%s)\n",
				zbd_zone_start(devz),
				errno, strerror(errno));
			return ret;
		}
	} else if (zbd_zone_exp_open(dumpz)) {
		ret = zbd_open_zones(fd, zbd_zone_start(devz),
				     zbd_zone_len(devz));
		if (ret) {
			fprintf(stderr,
				"Open target zone at %llu failed %d (%s)\n",
				zbd_zone_start(devz),
				errno, strerror(errno));
			return ret;
		}
	}

	return 0;
}

int zbd_restore(int fd, struct zbd_opts *opts)
{
	struct zbd_restore ropts;
	struct zbd_zone *dumpz, *devz;
	unsigned int i, nz = 0;
	int ret;

	memset(&ropts, 0, sizeof(struct zbd_restore));

	zbd_dump_prep_path(opts);

	/* Get zone information from the target device */
	ret = zbd_list_zones(fd, 0, 0, ZBD_RO_ALL, &ropts.dev_zones, &nz);
	if (ret != 0) {
		fprintf(stderr, "zbd_list_zones() failed %d\n", ret);
		return ret;
	}
	if (nz != opts->dev_info.nr_zones) {
		fprintf(stderr,
			"Invalid number of zones: expected %u, got %u\n",
			opts->dev_info.nr_zones, nz);
		ret = -1;
		goto out;
	}

	/* Get and check zone information from the dump file */
	ret = zbd_load_zone_info(&ropts, opts);
	if (ret)
		goto out;

	/* Open and check the zone data dump file */
	ret = zbd_open_zone_data(&ropts, opts);
	if (ret)
		goto out;

	/* Get an IO buffer */
	ret = posix_memalign(&ropts.buf, sysconf(_SC_PAGESIZE),
			     ZBD_DUMP_IO_SIZE);
	if (ret) {
		fprintf(stderr, "No memory\n");
		goto out;
	}

	/*
	 * Restore the target device. To avoid hitting the max active or max
	 * open zone limits of the target drive, process all zones in several
	 * passes with each pass handling one condition.
	 */

	/* Pass 1: Reset all zones in the dump range */
	for (i = ropts.zstart; i < ropts.zend; i++) {
		dumpz = &ropts.dump_zones[i];
		devz = &ropts.dev_zones[i];

		if (zbd_zone_offline(dumpz))
			continue;
		if (!zbd_zone_seq(dumpz) || zbd_zone_empty(dumpz))
			continue;

		ret = zbd_reset_zones(fd, zbd_zone_start(devz),
				      zbd_zone_len(devz));
		if (ret) {
			fprintf(stderr, "Reset target zone %u failed %d (%s)\n",
				i, errno, strerror(errno));
			goto out;
		}
	}

	/* Pass 2: copy data of conventional and full sequential zones */
	for (i = ropts.zstart; i < ropts.zend; i++) {
		dumpz = &ropts.dump_zones[i];
		devz = &ropts.dev_zones[i];

		if (!zbd_zone_cnv(dumpz) && !zbd_zone_full(dumpz))
			continue;

		ret = zbd_restore_one_zone(fd, &ropts, dumpz, devz);
		if (ret < 0)
			goto out;
	}

	/* Pass 3: handle closed zones */
	for (i = ropts.zstart; i < ropts.zend; i++) {
		dumpz = &ropts.dump_zones[i];
		devz = &ropts.dev_zones[i];

		if (!zbd_zone_closed(dumpz))
			continue;

		ret = zbd_restore_one_zone(fd, &ropts, dumpz, devz);
		if (ret < 0)
			goto out;
	}

	/* Pass 4: handle explicitly open zones */
	for (i = ropts.zstart; i < ropts.zend; i++) {
		dumpz = &ropts.dump_zones[i];
		devz = &ropts.dev_zones[i];

		if (!zbd_zone_exp_open(dumpz))
			continue;

		ret = zbd_restore_one_zone(fd, &ropts, dumpz, devz);
		if (ret < 0)
			goto out;
	}

	/* Pass 5: handle implicitly open zones */
	for (i = ropts.zstart; i < ropts.zend; i++) {
		dumpz = &ropts.dump_zones[i];
		devz = &ropts.dev_zones[i];

		if (!zbd_zone_imp_open(dumpz))
			continue;

		ret = zbd_restore_one_zone(fd, &ropts, dumpz, devz);
		if (ret < 0)
			goto out;
	}

	printf("    Restored %lld B in %u zones\n",
	       ropts.restored_bytes, ropts.restored_zones);

	ret = fsync(fd);
	if (ret)
		fprintf(stderr, "fsync target device failed %d (%s)\n",
			errno, strerror(errno));

	fsync(fd);

out:
	if (ropts.data_fd > 0)
		close(ropts.data_fd);
	free(ropts.dev_zones);
	free(ropts.dump_zones);
	free(ropts.buf);

	return ret;
}
