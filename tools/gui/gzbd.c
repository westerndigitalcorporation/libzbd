// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * SPDX-FileCopyrightText: 2020 Western Digital Corporation or its affiliates.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 */
#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#include "gzbd.h"

/**
 * Device control.
 */
dz_t dz;

/**
 * Signal handling.
 */
static gboolean dz_process_signal(GIOChannel *source,
				  GIOCondition condition,
				  gpointer user_data)
{
	char buf[32];
	ssize_t size;

	if (condition & G_IO_IN) {
		size = read(g_io_channel_unix_get_fd(source), buf, sizeof(buf));
		if (size > 0) {
			/* Got signal */
			gtk_main_quit();
			return TRUE;
		}
	}

	return FALSE;
}

static void dz_sig_handler(int sig)
{
	/* Propagate signal through the pipe */
	if (write(dz.sig_pipe[1], &sig, sizeof(int)) < 0)
		printf("Signal %d processing failed\n", sig);
}

static void dz_set_signal_handlers(void)
{
	GIOChannel *sig_channel;
	long fd_flags;
	int ret;

	ret = pipe(dz.sig_pipe);
	if (ret < 0) {
		perror("pipe");
		exit(1);
	}

	fd_flags = fcntl(dz.sig_pipe[1], F_GETFL);
	if (fd_flags < 0) {
		perror("Read descriptor flags");
		exit(1);
	}
	ret = fcntl(dz.sig_pipe[1], F_SETFL, fd_flags | O_NONBLOCK);
	if (ret < 0) {
		perror("Write descriptor flags");
		exit(1);
	}

	/* Install the unix signal handler */
	signal(SIGINT, dz_sig_handler);
	signal(SIGQUIT, dz_sig_handler);
	signal(SIGTERM, dz_sig_handler);

	/* Convert the reading end of the pipe into a GIOChannel */
	sig_channel = g_io_channel_unix_new(dz.sig_pipe[0]);
	g_io_channel_set_encoding(sig_channel, NULL, NULL);
	g_io_channel_set_flags(sig_channel,
			       g_io_channel_get_flags(sig_channel) |
			       G_IO_FLAG_NONBLOCK,
			       NULL);
	g_io_add_watch(sig_channel,
		       G_IO_IN | G_IO_PRI,
		       dz_process_signal, NULL);
}

int main(int argc, char **argv)
{
	gboolean init_ret;
	gboolean verbose = FALSE;
	GError *error = NULL;
	int i, ret = 0;
	GOptionEntry options[] = {
		{
			"verbose", 'v', 0,
			G_OPTION_ARG_NONE, &verbose,
			"Set libzbd verbose mode",
			NULL
		},
		{
			"block", 'b', 0,
			G_OPTION_ARG_INT, &dz.block_size,
			"Use block bytes as the unit for displaying zone "
			"position, length and write pointer position instead "
			"of the default byte value",
			NULL
		},
		{ NULL }
	};

	/* Init */
	memset(&dz, 0, sizeof(dz));
	dz.block_size = 1;
	for (i = 0; i < DZ_MAX_DEV; i++)
		dz.dev[i].dev_fd = -1;

	init_ret = gtk_init_with_args(&argc, &argv,
				      "<path to zoned block device>",
				      options, NULL, &error);
	if (init_ret == FALSE ||
	    error != NULL) {
		printf("Failed to parse command line arguments: %s\n",
		       error->message);
		g_error_free(error);
		return 1;
	}

	if (dz.block_size <= 0) {
		fprintf(stderr, "Invalid block size\n");
		return 1;
	}

	if (verbose)
		zbd_set_log_level(ZBD_LOG_DEBUG);

	dz_set_signal_handlers();

	/* Create GUI */
	dz_if_create();

	/* Check user credentials */
	if (getuid() != 0) {
		dz_if_err("Root privileges are required for running gzbd",
			  "Since gzbd is capable of erasing vast amounts of"
			  " data, only root may run it.");
		ret = 1;
		goto out;
	}

	/* Add devices listed on command line */
	for (i = 1; i < argc; i++)
		dz_if_add_device(argv[i]);

	/* Main event loop */
	gtk_main();

out:
	/* Cleanup GUI */
	dz_if_destroy();

	return ret;
}

/*
 * Report zones.
 */
static int dz_report_zones(dz_dev_t *dzd)
{
	unsigned int i, j = 0;
	int ret;

	if (!dzd->zones || !dzd->max_nr_zones) {

		/* Get list of all zones */
		dzd->zone_ro = ZBD_RO_ALL;
		ret = zbd_list_zones(dzd->dev_fd,
				     0, 0, dzd->zone_ro,
				     &dzd->zbdz, &dzd->nr_zones);
		if (ret != 0)
			return ret;

		if (!dzd->nr_zones) {
			/* That should not happen */
			return -EIO;
		}

		/* Allocate zone array */
		dzd->max_nr_zones = dzd->nr_zones;
		dzd->zones = (dz_dev_zone_t *)
			calloc(dzd->max_nr_zones, sizeof(dz_dev_zone_t));
		if (!dzd->zones)
			return -ENOMEM;

		for (i = 0; i < dzd->max_nr_zones; i++) {
			dzd->zones[i].no = i;
			dzd->zones[i].visible = 1;
			memcpy(&dzd->zones[i].info, &dzd->zbdz[i],
			       sizeof(struct zbd_zone));
		}

		return 0;

	}

	/* Refresh zone list */
	dzd->nr_zones = dzd->max_nr_zones;
	ret = zbd_report_zones(dzd->dev_fd,
			       0, 0, dzd->zone_ro,
			       dzd->zbdz, &dzd->nr_zones);
	if (ret != 0) {
		fprintf(stderr, "Get zone information failed %d (%s)\n",
			errno, strerror(errno));
		dzd->nr_zones = 0;
	}

	/* Apply filter */
	for (i = 0; i < dzd->max_nr_zones; i++) {
		if (j < dzd->nr_zones &&
		    zbd_zone_start(&dzd->zones[i].info) ==
		    zbd_zone_start(&dzd->zbdz[j])) {
			memcpy(&dzd->zones[i].info, &dzd->zbdz[j],
			       sizeof(struct zbd_zone));
			dzd->zones[i].visible = 1;
			j++;
		} else {
			dzd->zones[i].visible = 0;
		}
	}

	return ret;
}

/*
 * Zone operation.
 */
static int dz_zone_operation(dz_dev_t *dzd)
{
	int zno = dzd->zone_no;
	off_t ofst, len;
	int ret;

	if (zno >= (int)dzd->nr_zones) {
		fprintf(stderr, "Invalid zone number %d / %u\n",
			zno,
			dzd->nr_zones);
		return -1;
	}

	if (zno < 0) {
		ofst = 0;
		len = dzd->capacity;
	} else {
		ofst = zbd_zone_start(&dzd->zones[zno].info);
		len = zbd_zone_len(&dzd->zones[zno].info);
	}

	ret = zbd_zones_operation(dzd->dev_fd, dzd->zone_op, ofst, len);
	if (ret != 0)
		fprintf(stderr, "zbd_zone_operation failed %d\n", ret);

	return ret;
}

/*
 * Command thread routine.
 */
void *dz_cmd_run(void *data)
{
	dz_dev_t *dzd = data;
	int do_report_zones = 1;
	int ret;

	switch (dzd->cmd_id) {
	case DZ_CMD_REPORT_ZONES:
		do_report_zones = 0;
		ret = dz_report_zones(dzd);
		break;
	case DZ_CMD_ZONE_OP:
		ret = dz_zone_operation(dzd);
		break;
	default:
		fprintf(stderr, "Invalid command ID %d\n", dzd->cmd_id);
		ret = -1;
		break;
	}

	if (do_report_zones)
		ret = dz_report_zones(dzd);

	if (dzd->cmd_dialog) {
		int response_id;
		if (ret == 0)
			response_id = GTK_RESPONSE_OK;
		else
			response_id = GTK_RESPONSE_REJECT;
		gtk_dialog_response(GTK_DIALOG(dzd->cmd_dialog), response_id);
	}

	return (void *)((unsigned long) ret);
}

static GtkWidget *dz_cmd_dialog(char *msg)
{
	GtkWidget *dialog, *content_area;
	GtkWidget *spinner;

	dialog = gtk_message_dialog_new(GTK_WINDOW(dz.window),
					GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_OTHER,
					GTK_BUTTONS_NONE,
					"%s", msg);
	content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	spinner = gtk_spinner_new();
	gtk_widget_show(spinner);
	gtk_container_add(GTK_CONTAINER(content_area), spinner);
	gtk_spinner_start(GTK_SPINNER(spinner));

	gtk_widget_show_all(dialog);

	return dialog;
}


/*
 * Open a device.
 */
dz_dev_t *dz_open(char *path)
{
	dz_dev_t *dzd = NULL;
	int i, ret;

	/* Get an unused device */
	for (i = 0; i < DZ_MAX_DEV; i++) {
		if (dz.dev[i].dev_fd < 0) {
			dzd = &dz.dev[i];
			break;
		}
	}

	if (!dzd) {
		dz_if_err("Too many open devices",
			  "At most %d devices can be open",
			  (int)DZ_MAX_DEV);
		fprintf(stderr, "Too many open devices\n");
		return NULL;
	}

	/* Open device file */
	strncpy(dzd->path, path, sizeof(dzd->path) - 1);
	dzd->dev_fd = zbd_open(dzd->path, O_RDWR | O_LARGEFILE, &dzd->info);
	if (dzd->dev_fd < 0) {
		ret = errno;
		dz_if_err("Open device failed",
			  "Open %s failed %d (%s)",
			  dzd->path, ret, strerror(ret));
		fprintf(stderr, "Open device %s failed %d (%s)\n",
			dzd->path, ret, strerror(ret));
		return NULL;
	}

	dzd->capacity = dzd->info.nr_sectors << 9;

	dzd->block_size = dz.block_size;
	if (!dzd->block_size) {
		dzd->block_size = 1;
	} else if (dzd->info.zone_size % dzd->block_size) {
		dz_if_err("Invalid block size",
			"The device zone size is not a multiple of the block size");
		fprintf(stderr, "Invalid block size\n");
		ret = 1;
		goto out;
	}

	/* Get zone information */
	ret = dz_report_zones(dzd);
	if (ret != 0)
		goto out;

	dz.nr_devs++;

out:
	if (ret != 0) {
		dz_close(dzd);
		dzd = NULL;
	}

	return dzd;
}

/*
 * Close a device.
 */
void dz_close(dz_dev_t *dzd)
{

	if (dzd->dev_fd < 0)
		return;

	free(dzd->zbdz);
	dzd->zbdz = NULL;

	free(dzd->zones);
	dzd->zones = NULL;

	zbd_close(dzd->dev_fd);
	dzd->dev_fd = -1;

	memset(dzd, 0, sizeof(dz_dev_t));
	dz.nr_devs--;
}

/*
 * Execute a command.
 */
int dz_cmd_exec(dz_dev_t *dzd, int cmd_id, char *msg)
{
	int ret;

	/* Set command */
	dzd->cmd_id = cmd_id;
	if (msg)
		/* Create a dialog */
		dzd->cmd_dialog = dz_cmd_dialog(msg);
	else
		dzd->cmd_dialog = NULL;

	/* Create command thread */
	ret = pthread_create(&dzd->cmd_thread, NULL, dz_cmd_run, dzd);
	if (ret != 0)
		goto out;

	if (dzd->cmd_dialog) {
		if (gtk_dialog_run(GTK_DIALOG(dzd->cmd_dialog))
		    == GTK_RESPONSE_OK)
			ret = 0;
		else
			ret = -1;
	} else {
		void *cmd_ret;
		pthread_join(dzd->cmd_thread, &cmd_ret);
		ret = (long)cmd_ret;
	}

	pthread_join(dzd->cmd_thread, NULL);

out:
	if (dzd->cmd_dialog) {
		gtk_widget_destroy(dzd->cmd_dialog);
		dzd->cmd_dialog = NULL;
	}

	return ret;
}

