// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * SPDX-FileCopyrightText: 2020 Western Digital Corporation or its affiliates.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 *	    Ting Yao <tingyao@hust.edu.cn>
 */
#include "zbd.h"

static int zbd_mgmt(int fd, struct zbd_opts *opts)
{
	int ret;

	/* Check zone range */
	if (opts->ofst % opts->dev_info.zone_size ||
	    opts->len % opts->dev_info.zone_size) {
		fprintf(stderr, "Invalid unaligned offset/length\n");
		return 1;
	}

	switch (opts->cmd) {
	case ZBD_RESET:
		ret = zbd_reset_zones(fd, opts->ofst, opts->len);
		break;
	case ZBD_OPEN:
		ret = zbd_open_zones(fd, opts->ofst, opts->len);
		break;
	case ZBD_CLOSE:
		ret = zbd_close_zones(fd, opts->ofst, opts->len);
		break;
	case ZBD_FINISH:
		ret = zbd_finish_zones(fd, opts->ofst, opts->len);
		break;
	default:
		fprintf(stderr, "Invalid operation\n");
		return 1;
	}

	if (ret)
		fprintf(stderr, "Zone operation failed %d (%s)\n",
			errno, strerror(errno));

	return ret;
}

static void zbd_print_zone(struct zbd_opts *opts, struct zbd_zone *z)
{
	unsigned int zno = zbd_zone_start(z) / opts->dev_info.zone_size;

	if (opts->rep_csv) {
		printf("%05u, %u, %014llu, %014llu, %014llu, %014llu, 0x%01x, %01d, %01d\n",
		       zno,
		       (unsigned int)zbd_zone_type(z),
		       zbd_zone_start(z) / opts->unit,
		       zbd_zone_len(z) / opts->unit,
		       zbd_zone_capacity(z) / opts->unit,
		       zbd_zone_wp(z) / opts->unit,
		       zbd_zone_cond(z),
		       zbd_zone_non_seq_resources(z) ? 1 : 0,
		       zbd_zone_rwp_recommended(z) ? 1 : 0);
		return;
	}

	if (zbd_zone_cnv(z)) {
		printf("Zone %05u: %s, ofst %014llu, len %014llu, cap %014llu\n",
		       zno,
		       zbd_zone_type_str(z, true),
		       zbd_zone_start(z) / opts->unit,
		       zbd_zone_len(z) / opts->unit,
		       zbd_zone_capacity(z) / opts->unit);
		return;
	}

	if (zbd_zone_seq(z)) {
		printf("Zone %05u: %s, ofst %014llu, len %014llu, cap %014llu, "
		       "wp %014llu, %s, non_seq %01d, reset %01d\n",
		       zno,
		       zbd_zone_type_str(z, true),
		       zbd_zone_start(z) / opts->unit,
		       zbd_zone_len(z) / opts->unit,
		       zbd_zone_capacity(z) / opts->unit,
		       zbd_zone_wp(z) / opts->unit,
		       zbd_zone_cond_str(z, true),
		       zbd_zone_non_seq_resources(z) ? 1 : 0,
		       zbd_zone_rwp_recommended(z) ? 1 : 0);
		return;
	}

	printf("Zone %05u: unknown type 0x%01x, ofst %014llu, len %014llu\n",
	       zno, zbd_zone_type(z),
	       zbd_zone_start(z) / opts->unit,
	       zbd_zone_len(z) / opts->unit);
}

static int zbd_report(int fd, struct zbd_opts *opts)
{
	struct zbd_zone *zones = NULL;
	unsigned int i, nz;
	int ret;

	/* Allocate zone array */
	nz = (opts->len + opts->dev_info.zone_size - 1) /
		opts->dev_info.zone_size;
	if (!nz)
		return 0;

	zones = (struct zbd_zone *) calloc(nz, sizeof(struct zbd_zone));
	if (!zones) {
		fprintf(stderr, "No memory\n");
		return 1;
	}

	if (!opts->rep_dump) {
		/* Get zone information from the device */
		ret = zbd_report_zones(fd, opts->ofst, opts->len, opts->rep_opt,
				       zones, &nz);
		if (ret != 0) {
			fprintf(stderr, "zbd_report_zones() failed %d\n", ret);
			ret = 1;
			goto out;
		}
	} else {
		/* Get zone information from the dump file */
		ret = zbd_dump_report_zones(fd, opts, zones, &nz);
		if (ret != 0) {
			ret = 1;
			goto out;
		}
	}

	if (opts->rep_num_zones) {
		if (opts->rep_csv) {
			if (!opts->rep_capacity)
				printf("%u\n", nz);
			else
				printf("%u, ", nz);
		} else {
			printf("%u zones\n", nz);
		}
		if (!opts->rep_capacity)
			goto out;
	}

	if (opts->rep_capacity) {
		unsigned long long capacity = 0;

		for (i = 0; i < nz; i++)
			capacity += zbd_zone_capacity(&zones[i]) / opts->unit;

		if (opts->rep_csv) {
			printf("%llu\n", capacity);
		} else {
			if (opts->unit != 1)
				printf("%llu x %zu B total zone capacity\n",
				       capacity, opts->unit);
			else
				printf("%llu B total zone capacity\n",
				       capacity);
		}
		goto out;
	}

	if (opts->rep_csv)
		printf("zone num, type, ofst, len, cap, wp, cond, non_seq, reset\n");
	for (i = 0; i < nz; i++)
		zbd_print_zone(opts, &zones[i]);

out:
	free(zones);
	return ret;
}

struct zbd_action {
	int (*action)(int, struct zbd_opts *);
	int flags;
};

static struct zbd_action zact[] = {
	{ zbd_report,	O_RDONLY },	/* ZBD_REPORT */
	{ zbd_mgmt,	O_WRONLY },	/* ZBD_RESET */
	{ zbd_mgmt,	O_WRONLY },	/* ZBD_OPEN */
	{ zbd_mgmt,	O_WRONLY },	/* ZBD_CLOSE */
	{ zbd_mgmt,	O_WRONLY },	/* ZBD_FINISH */
	{ zbd_dump,	O_RDONLY },	/* ZBD_DUMP */
	{ zbd_restore,	O_RDWR | O_DIRECT },	/* ZBD_RESTORE */
};

static void zbd_print_dev_info(struct zbd_opts *opts)
{
	if (opts->cmd == ZBD_REPORT && opts->rep_csv)
		return;

	printf("Device %s:\n", opts->dev_path);

	printf("    Vendor ID: %s\n",
	       opts->dev_info.vendor_id);
	printf("    Zone model: %s\n",
		zbd_device_model_str(opts->dev_info.model, false));
	printf("    Capacity: %.03F GB (%llu 512-bytes sectors)\n",
	       (double)(opts->dev_info.nr_sectors << 9) / 1000000000,
	       opts->dev_info.nr_sectors);
	printf("    Logical blocks: %llu blocks of %u B\n",
	       opts->dev_info.nr_lblocks, opts->dev_info.lblock_size);
	printf("    Physical blocks: %llu blocks of %u B\n",
	       opts->dev_info.nr_pblocks, opts->dev_info.pblock_size);
	printf("    Zones: %u zones of %.1F MB\n",
	       opts->dev_info.nr_zones,
	       (double)opts->dev_info.zone_size / 1048576.0);

	printf("    Maximum number of open zones: ");
	if (opts->dev_info.max_nr_open_zones == 0)
		printf("no limit\n");
	else
		printf("%u\n", opts->dev_info.max_nr_open_zones);

	printf("    Maximum number of active zones: ");
	if (opts->dev_info.max_nr_active_zones == 0)
		printf("no limit\n");
	else
		printf("%u\n", opts->dev_info.max_nr_active_zones);
}

static int zbd_usage(char *cmd)
{
	printf("Usage: %s <command> [options] <device path | dump file>\n"
	       "Commands:\n"
	       "  report : Get zone information from a device or from\n"
	       "           a zone information dump file\n"
	       "  reset  : Reset zone(s) of a device\n"
	       "  open   : Explicitly open zone(s) of a device\n"
	       "  close  : Close zone(s) of a device\n"
	       "  finish : Finish zone(s) of a device\n"
	       "  dump   : Dump a device zone information and zone data to\n"
	       "           files (see -d and -f options).\n"
	       "  restore : Restore a device zones status and data from dump\n"
	       "            files (see -d and -f options).\n"
	       "Common options:\n"
	       "  -v		   : Verbose mode (for debug)\n"
	       "  -i		   : Display device information\n"
	       "  -ofst <ofst (B)> : Start offset of the first zone of the\n"
	       "		     target range (default: 0)\n"
	       "  -len <len (B)>   : Size of the zone range to operate on\n"
	       "		     (default: device capacity)\n"
	       "  -u <unit (B)>	   : Size unit to use for ofst and len options,\n"
	       "		     and for displaying zone report results.\n"
	       "		     (default: 1)\n"
	       "Report command options:\n"
	       "  -csv      : Use csv output format\n"
	       "  -n        : Only output the number of zones reported\n"
	       "  -c        : Only output the total capacity of zones reported\n"
	       "  -ro <opt> : Specify a zone report option. Possible values are:\n"
	       "              * \"em\": empty zones\n"
	       "              * \"oi\": implicitly open zones\n"
	       "              * \"oe\": explicitly open zones\n"
	       "              * \"cl\": closed zones\n"
	       "              * \"fu\": full zones\n"
	       "              * \"ro\": read-only zones\n"
	       "              * \"ol\": offline zones\n"
	       "              * \"nw\": conventional zones\n"
	       "              * \"ns\": non-seq write resource zones\n"
	       "              * \"rw\": reset-wp recommended zones\n"
	       "dump and restore commands options:\n"
	       "  -d <path> : Path where to save dump files.\n"
	       "  -f <name> : Name prefix for the dump files. If not\n"
	       "              specified, the device base name is used\n"
	       "              as a dump file name prefix\n",
	       cmd);
	return 1;
}

int main(int argc, char **argv)
{
	struct zbd_opts opts;
	bool dev_info = false;
	int dev_fd = 0, i, ret = 1;
	long long capacity;
	char dev_path[PATH_MAX];

	memset(&opts, 0, sizeof(struct zbd_opts));
	opts.rep_opt = ZBD_RO_ALL;
	opts.rep_dump = false;
	opts.unit = 1;

	/* Parse options */
	if (argc < 3)
		return zbd_usage(argv[0]);

	if (strcmp(argv[1], "report") == 0) {
		opts.cmd = ZBD_REPORT;
	} else if (strcmp(argv[1], "reset") == 0) {
		opts.cmd = ZBD_RESET;
	} else if (strcmp(argv[1], "open") == 0) {
		opts.cmd = ZBD_OPEN;
	} else if (strcmp(argv[1], "close") == 0) {
		opts.cmd = ZBD_CLOSE;
	} else if (strcmp(argv[1], "finish") == 0) {
		opts.cmd = ZBD_FINISH;
	} else if (strcmp(argv[1], "dump") == 0) {
		opts.cmd = ZBD_DUMP;
	} else if (strcmp(argv[1], "restore") == 0) {
		opts.cmd = ZBD_RESTORE;
	} else {
		fprintf(stderr, "Invalid command \"%s\"\n", argv[1]);
		return 1;
	}

	for (i = 2; i < (argc - 1); i++) {

		/*
		 * Common options.
		 */
		if (strcmp(argv[i], "-v") == 0) {

			zbd_set_log_level(ZBD_LOG_DEBUG);

		} else if (strcmp(argv[i], "-i") == 0) {

			dev_info = true;

		} else if (strcmp(argv[i], "-ofst") == 0) {

			if (i >= (argc - 1)) {
				fprintf(stderr, "Invalid command line\n");
				return 1;
			}
			i++;

			opts.ofst = strtoll(argv[i], NULL, 10);

		} else if (strcmp(argv[i], "-len") == 0) {

			if (i >= (argc - 1)) {
				fprintf(stderr, "Invalid command line\n");
				return 1;
			}
			i++;

			opts.len = strtoll(argv[i], NULL, 10);

		} else if (strcmp(argv[i], "-u") == 0) {

			if (i >= (argc - 1)) {
				fprintf(stderr, "Invalid command line\n");
				return 1;
			}
			i++;

			opts.unit = strtoll(argv[i], NULL, 10);

		/*
		 * Report zones options.
		 */
		} else if (strcmp(argv[i], "-csv") == 0) {

			opts.rep_csv = true;

		} else if (strcmp(argv[i], "-n") == 0) {

			opts.rep_num_zones = true;

		} else if (strcmp(argv[i], "-c") == 0) {

			opts.rep_capacity = true;

		} else if (strcmp(argv[i], "-ro") == 0) {

			if (i >= (argc - 1)) {
				fprintf(stderr, "Invalid command line\n");
				return 1;
			}
			i++;

			if (strcmp(argv[i], "em") == 0) {
				opts.rep_opt = ZBD_RO_EMPTY;
			} else if (strcmp(argv[i], "oi") == 0) {
				opts.rep_opt = ZBD_RO_IMP_OPEN;
			} else if (strcmp(argv[i], "oe") == 0) {
				opts.rep_opt = ZBD_RO_EXP_OPEN;
			} else if (strcmp(argv[i], "cl") == 0) {
				opts.rep_opt = ZBD_RO_CLOSED;
			} else if (strcmp(argv[i], "fu") == 0) {
				opts.rep_opt = ZBD_RO_FULL;
			} else if (strcmp(argv[i], "ro") == 0) {
				opts.rep_opt = ZBD_RO_RDONLY;
			} else if (strcmp(argv[i], "ol") == 0) {
				opts.rep_opt = ZBD_RO_OFFLINE;
			} else if (strcmp(argv[i], "rw") == 0) {
				opts.rep_opt = ZBD_RO_RWP_RECOMMENDED;
			} else if (strcmp(argv[i], "ns") == 0) {
				opts.rep_opt = ZBD_RO_NON_SEQ;
			} else if (strcmp(argv[i], "nw") == 0) {
				opts.rep_opt = ZBD_RO_NOT_WP;
			} else {
				fprintf(stderr,
					"Unknown report option \"%s\"\n",
					argv[i]);
				return 1;
			}

		/*
		 * Dump and restore command options.
		 */
		} else if (strcmp(argv[i], "-d") == 0) {

			if (i >= (argc - 1)) {
				fprintf(stderr, "Invalid command line\n");
				return 1;
			}
			i++;

			opts.dump_path = argv[i];

		} else if (strcmp(argv[i], "-f") == 0) {

			if (i >= (argc - 1)) {
				fprintf(stderr, "Invalid command line\n");
				return 1;
			}
			i++;

			opts.dump_prefix = argv[i];

		} else if (argv[i][0] == '-') {

			fprintf(stderr, "Unknown option \"%s\"\n", argv[i]);
			return 1;

		} else {
			break;
		}

	}

	if (i != (argc - 1)) {
		fprintf(stderr, "No device specified\n");
		return 1;
	}

	if (!realpath(argv[i], dev_path)) {
		fprintf(stderr, "Invalid device path %s\n", argv[i]);
		return 1;
	}
	opts.dev_path = dev_path;

	/*
	 * Special case for zone report using zone info dump file.
	 */
	if (opts.cmd == ZBD_REPORT) {
		dev_fd = zbd_open_dump(&opts);
		if (dev_fd < 0)
			return 1;
	}

	if (!dev_fd) {
		/* Open device */
		dev_fd = zbd_open(opts.dev_path,
				  zact[opts.cmd].flags | O_LARGEFILE,
				  &opts.dev_info);
		if (dev_fd < 0) {
			fprintf(stderr, "Open %s failed (%s)\n",
				opts.dev_path, strerror(errno));
			return 1;
		}
	}

	/* Check unit, offset and length */
	capacity = (long long)opts.dev_info.nr_sectors << 9;
	if (opts.unit > 1 &&
	    (opts.unit > opts.dev_info.zone_size || opts.unit % 512)) {
		fprintf(stderr, "Invalid unit\n");
		ret = 1;
		goto out;
	}

	if (opts.ofst % 512 || opts.len % 512) {
		fprintf(stderr, "Invalid unaligned offset/length\n");
		ret = 1;
		goto out;
	}

	if (opts.ofst >= capacity) {
		ret = 0;
		goto out;
	}

	if (!opts.len)
		opts.len = capacity;
	if (opts.ofst + opts.len > capacity)
		opts.len = capacity - opts.ofst;

	if (dev_info)
		zbd_print_dev_info(&opts);

	ret = zact[opts.cmd].action(dev_fd, &opts);
	if (ret)
		ret = 1;

out:
	if (!opts.rep_dump)
		zbd_close(dev_fd);
	else
		close(dev_fd);

	return ret;
}

