/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * SPDX-FileCopyrightText: 2020 Western Digital Corporation or its affiliates.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 */
#ifndef __GZVIEWER_H__
#define __GZVIEWER_H__

#include <sys/time.h>
#include <gtk/gtk.h>

#include <libzbd/zbd.h>

/*
 * Device zone information.
 */
struct gzv_zone {
	unsigned int		zno;
	struct zbd_zone		*zbdz;
	GtkWidget		*da;
};

/*
 * GUI data.
 */
struct gzv {

	/*
	 * Parameters.
	 */
	int			refresh_interval;
	int			block_size;
	int			abort;

	/*
	 * For handling timer and signals.
	 */
	GSource			*refresh_timer;
	unsigned long long	last_refresh;
	int			sig_pipe[2];

	/*
	 * Interface stuff.
	 */
	GdkRGBA			color_conv;
	GdkRGBA			color_seq;
	GdkRGBA			color_seqw;
	GdkRGBA			color_nonw;
	GdkRGBA			color_text;
	GdkRGBA			color_oi;
	GdkRGBA			color_oe;
	GdkRGBA			color_cl;
	GdkRGBA			color_of;
	GtkWidget		*window;
	GtkAdjustment		*vadj;

	/*
	 * Device information.
	 */
	char			*path;
	int			dev_fd;
	struct zbd_info		info;
	unsigned int		nr_zones;
	unsigned int		nr_conv_zones;
	struct zbd_zone		*zones;

	/*
	 * Drawn zones.
	 */
	unsigned int		nr_row;
	unsigned int		nr_col;
	unsigned int		nr_grid_zones;
	unsigned int		max_row;
	struct gzv_zone		*grid_zones;
	unsigned int		grid_zno_first;
};

/**
 * System time in usecs.
 */
static inline unsigned long long gzv_msec(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return (unsigned long long) tv.tv_sec * 1000LL +
		(unsigned long long) tv.tv_usec / 1000;
}

extern struct gzv gzv;

int gzv_report_zones(unsigned int zno_start, unsigned int nr_zones);

void gzv_if_err(const char *msg, const char *fmt, ...);
void gzv_if_create_window(void);
void gzv_if_create(void);
void gzv_if_destroy(void);

#endif /* __GZWATCH_H__ */
