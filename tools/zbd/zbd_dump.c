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

	ret = write(info_fd, dump, sizeof(struct zbd_dump));
	if (ret != (ssize_t)sizeof(struct zbd_dump)) {
		fprintf(stderr, "Write dump header failed\n");
		ret = -1;
		goto out;
	}

	sz = sizeof(struct zbd_zone) * opts->dev_info.nr_zones;
	ret = write(info_fd, zones, sz);
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

int zbd_dump(int fd, struct zbd_opts *opts)
{
	struct zbd_zone *zones = NULL;
	struct zbd_dump dump;
	unsigned int nz;
	int ret;

	if (!opts->dump_path) {
		opts->dump_path = get_current_dir_name();
		if (!opts->dump_path)
			opts->dump_path = ".";
	}

	if (!opts->dump_prefix)
		opts->dump_prefix = basename(opts->dev_path);

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

