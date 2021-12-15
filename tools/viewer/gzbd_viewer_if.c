// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * SPDX-FileCopyrightText: 2020 Western Digital Corporation or its affiliates.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "gzbd_viewer.h"

static void gzv_set_zone_tooltip(struct gzv_zone *zone)
{
	struct zbd_zone *zbdz = zone->zbdz;
	char info[512];
	char bs[64];

	if (!zbdz) {
		gtk_widget_set_has_tooltip(GTK_WIDGET(zone->da), false);
		gtk_widget_set_tooltip_markup(GTK_WIDGET(zone->da), NULL);
		return;
	}

	if (gzv.block_size == 1)
		snprintf(bs, sizeof(bs) - 1, "B");
	else
		snprintf(bs, sizeof(bs) - 1,
			 "%d-B blocks", gzv.block_size);

	if (zbd_zone_cnv(zbdz)) {
		snprintf(info, sizeof(info) - 1,
			 "<b>Zone %u</b>:\n"
			 "  - Type: %s\n"
			 "  - Condition: %s\n"
			 "  - Start offset: %llu %s\n"
			 "  - Length: %llu %s\n"
			 "  - Capacity: %llu %s",
			 zone->zno,
			 zbd_zone_type_str(zbdz, false),
			 zbd_zone_cond_str(zbdz, false),
			 zbd_zone_start(zbdz), bs,
			 zbd_zone_len(zbdz), bs,
			 zbd_zone_capacity(zbdz), bs);
	} else {
		snprintf(info, sizeof(info) - 1,
			 "<b>Zone %u</b>:\n"
			 "  - Type: %s\n"
			 "  - Condition: %s\n"
			 "  - Start offset: %llu %s\n"
			 "  - Length: %llu %s\n"
			 "  - Capacity: %llu %s\n"
			 "  - WP offset: +%llu %s",
			 zone->zno,
			 zbd_zone_type_str(zbdz, false),
			 zbd_zone_cond_str(zbdz, false),
			 zbd_zone_start(zbdz), bs,
			 zbd_zone_len(zbdz), bs,
			 zbd_zone_capacity(zbdz), bs,
			 zbd_zone_wp(zbdz) - zbd_zone_start(zbdz), bs);
	}

	gtk_widget_set_tooltip_markup(GTK_WIDGET(zone->da), info);
	gtk_widget_set_has_tooltip(GTK_WIDGET(zone->da), true);
}

static void gzv_if_update(void)
{
	struct zbd_zone *zbdz;
	unsigned int i, z;

	if (gzv.grid_zno_first >= gzv.nr_zones)
		gzv.grid_zno_first = gzv.nr_zones / gzv.nr_col;

	if (gzv_report_zones(gzv.grid_zno_first, gzv.nr_grid_zones))
		goto out;

	z = gzv.grid_zno_first;
	for (i = 0; i < gzv.nr_grid_zones; i++) {
		gzv.grid_zones[i].zno = z;
		zbdz = gzv.grid_zones[i].zbdz;
		if (z >= gzv.nr_zones) {
			if (zbdz)
				gtk_widget_hide(gzv.grid_zones[i].da);
			gzv.grid_zones[i].zbdz = NULL;
		} else {
			gzv.grid_zones[i].zbdz = &gzv.zones[z];
			if (!zbdz)
				gtk_widget_show(gzv.grid_zones[i].da);
		}
		gzv_set_zone_tooltip(&gzv.grid_zones[i]);
		gtk_widget_queue_draw(gzv.grid_zones[i].da);
		z++;
	}

out:
	gzv.last_refresh = gzv_msec();
}

static gboolean gzv_if_timer_cb(gpointer user_data)
{
	if (gzv.last_refresh + gzv.refresh_interval <= gzv_msec())
		gzv_if_update();

	return TRUE;
}

static gboolean gzv_if_resize_cb(GtkWidget *widget, GdkEvent *event,
				 gpointer user_data)
{
	gzv_if_update();

	return FALSE;
}

static void gzv_if_delete_cb(GtkWidget *widget, GdkEvent *event,
			     gpointer user_data)
{
	gzv.window = NULL;
	gtk_main_quit();
}

static void gzv_if_zone_draw_nonwritable(struct zbd_zone *zbdz, cairo_t *cr,
					 GtkAllocation *allocation)
{
	long long w;

	if (zbd_zone_capacity(zbdz) == zbd_zone_len(zbdz))
		return;

	/* Non-writable space in zone */
	w = (long long)allocation->width *
		(zbd_zone_len(zbdz) - zbd_zone_capacity(zbdz)) /
		zbd_zone_len(zbdz);
	if (w > allocation->width)
		w = allocation->width;

	gdk_cairo_set_source_rgba(cr, &gzv.color_nonw);
	cairo_rectangle(cr, allocation->width - w, 0, w, allocation->height);
	cairo_fill(cr);
}

static void gzv_if_zone_draw_written(struct zbd_zone *zbdz, cairo_t *cr,
				     GtkAllocation *allocation)
{
	long long w;

	if (zbd_zone_wp(zbdz) == zbd_zone_start(zbdz))
		return;

	/* Written space in zone */
	w = (long long)allocation->width *
		(zbd_zone_wp(zbdz) - zbd_zone_start(zbdz)) /
		zbd_zone_len(zbdz);
	if (w > allocation->width)
		w = allocation->width;

	gdk_cairo_set_source_rgba(cr, &gzv.color_seqw);
	cairo_rectangle(cr, 0, 0, w, allocation->height);
	cairo_fill(cr);
}

static void gzv_if_zone_draw_num(struct gzv_zone *zone, cairo_t *cr,
				 GtkAllocation *allocation)
{
	cairo_text_extents_t te;
	char str[16];

	/* Draw zone number */
	gdk_cairo_set_source_rgba(cr, &gzv.color_text);
	cairo_select_font_face(cr, "Monospace",
			       CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, 10);
	sprintf(str, "%05d", zone->zno);
	cairo_text_extents(cr, str, &te);
	cairo_move_to(cr, allocation->width / 2 - te.width / 2 - te.x_bearing,
		      (allocation->height + te.height) / 2);
	cairo_show_text(cr, str);
}

static gboolean gzv_if_zone_draw_cb(GtkWidget *widget, cairo_t *cr,
				    gpointer user_data)
{
	struct gzv_zone *zone = user_data;
	struct zbd_zone *zbdz = zone->zbdz;
	GtkAllocation allocation;

	/* Current size */
	gtk_widget_get_allocation(zone->da, &allocation);

	gtk_render_background(gtk_widget_get_style_context(widget),
			      cr, 0, 0, allocation.width, allocation.height);

	if (!zbdz)
		return TRUE;

	/* Draw zone background */
	if (zbd_zone_cnv(zbdz)) {
		gdk_cairo_set_source_rgba(cr, &gzv.color_conv);
		goto out_fill;
	}

	if (zbd_zone_full(zbdz)) {
		gdk_cairo_set_source_rgba(cr, &gzv.color_seqw);
		goto out_fill;
	}

	if (zbd_zone_offline(zbdz)) {
		gdk_cairo_set_source_rgba(cr, &gzv.color_of);
		goto out_fill;
	}

	gdk_cairo_set_source_rgba(cr, &gzv.color_seq);
	if (zbd_zone_empty(zbdz))
		goto out_fill;

	/* Opened or closed zones */
	cairo_rectangle(cr, 0, 0, allocation.width, allocation.height);
	cairo_fill(cr);

	gzv_if_zone_draw_nonwritable(zbdz, cr, &allocation);
	gzv_if_zone_draw_written(zbdz, cr, &allocation);

	/* State highlight */
	if (zbd_zone_imp_open(zbdz))
		gdk_cairo_set_source_rgba(cr, &gzv.color_oi);
	else if (zbd_zone_exp_open(zbdz))
		gdk_cairo_set_source_rgba(cr, &gzv.color_oe);
	else
		gdk_cairo_set_source_rgba(cr, &gzv.color_cl);
	cairo_set_line_width(cr, 10);
	cairo_rectangle(cr, 0, 0, allocation.width, allocation.height);
	cairo_stroke(cr);

	/* Draw zone number */
	gzv_if_zone_draw_num(zone, cr, &allocation);

	return TRUE;

out_fill:
	cairo_rectangle(cr, 0, 0, allocation.width, allocation.height);
	cairo_fill(cr);

	gzv_if_zone_draw_nonwritable(zbdz, cr, &allocation);

	/* Draw zone number */
	gzv_if_zone_draw_num(zone, cr, &allocation);

	return TRUE;
}

static gboolean gzv_if_scroll_cb(GtkWidget *widget, GdkEventScroll *scroll,
				gpointer user_data)
{
	unsigned int row = gtk_adjustment_get_value(gzv.vadj);
	unsigned int new_row = row;

	switch (scroll->direction) {
	case GDK_SCROLL_UP:
		if (row > 0)
			new_row = row - 1;
		break;
	case GDK_SCROLL_DOWN:
		if (row < gzv.max_row)
			new_row = row + 1;
		break;
	case GDK_SCROLL_LEFT:
	case GDK_SCROLL_RIGHT:
	case GDK_SCROLL_SMOOTH:
	default:
		break;
	}

	if (new_row != row)
		gtk_adjustment_set_value(gzv.vadj, new_row);

	return TRUE;
}

static gboolean gzv_if_scroll_value_cb(GtkWidget *widget,
				       GdkEvent *event, gpointer user_data)
{
	unsigned int zno, row = gtk_adjustment_get_value(gzv.vadj);

	if (row >= gzv.max_row)
		row = gzv.max_row - 1;

	zno = row * gzv.nr_col;
	if (zno != gzv.grid_zno_first) {
		gzv.grid_zno_first = zno;
		gzv_if_update();
	}

	return TRUE;
}

static void gzv_if_draw_legend(char *str, const GdkRGBA *color, cairo_t *cr,
			       gint *x, gint y)
{
	cairo_text_extents_t te;
	GdkRGBA border_color;
	gint w = 10;

	gdk_rgba_parse(&border_color, "Black");
	gdk_cairo_set_source_rgba(cr, &border_color);
	cairo_set_line_width(cr, 2);
	cairo_rectangle(cr, *x, y - w / 2, w, w);
	cairo_stroke_preserve(cr);
	gdk_cairo_set_source_rgba(cr, color);
	cairo_fill(cr);
	*x += w;

	cairo_text_extents(cr, str, &te);
	cairo_move_to(cr,
		      *x + 5 - te.x_bearing,
		      y - te.height / 2 - te.y_bearing);
	cairo_show_text(cr, str);
	*x += te.x_advance + 20;
}

static gboolean gzv_if_draw_legend_cb(GtkWidget *widget, cairo_t *cr,
				      gpointer data)
{
	gint x = 10, y = 10;

	/* Set font */
	cairo_select_font_face(cr, "Monospace",
			       CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, 10);

	/* Conv zone legend */
	gzv_if_draw_legend("Conventional zone", &gzv.color_conv, cr, &x, y);

	/* Seq zone legend */
	gzv_if_draw_legend("Sequential zone (unwritten)", &gzv.color_seq, cr,
			   &x, y);

	/* Seq written zone legend */
	gzv_if_draw_legend("Sequential zone (written)", &gzv.color_seqw, cr,
			   &x, y);

	/* Non-writable space legend */
	gzv_if_draw_legend("Non-writable space", &gzv.color_nonw, cr, &x, y);

	/* Second row */
	x = 10;
	y += 20;

	/* Offline zone legend */
	gzv_if_draw_legend("Offline zone", &gzv.color_of, cr, &x, y);

	/* Implicit open zone legend */
	gzv_if_draw_legend("Implicitly opened zone", &gzv.color_oi, cr, &x, y);

	/* Explicit open zone legend */
	gzv_if_draw_legend("Explicitly opened zone", &gzv.color_oe, cr, &x, y);

	/* Closed open zone legend */
	gzv_if_draw_legend("Closed zone", &gzv.color_cl, cr, &x, y);

	return FALSE;
}

static void gzv_if_get_da_size(unsigned int *w, unsigned int *h)
{
	GdkDisplay *disp = gdk_display_get_default();
	GdkMonitor *mon = gdk_display_get_primary_monitor(disp);
	GdkRectangle geom;

	gdk_monitor_get_geometry(mon, &geom);

	*w = (geom.width - 200) / gzv.nr_col;
	if (*w > 150)
		*w = 150;

	*h = (geom.height - 200) / gzv.nr_row;
	if (*h > 60)
		*h = 60;
}

void gzv_if_create_window(void)
{
	if (gzv.window)
		return;

	gzv.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(gzv.window),
			     "Zoned Block Device Zone State");
	gtk_container_set_border_width(GTK_CONTAINER(gzv.window), 10);

	g_signal_connect((gpointer) gzv.window, "delete-event",
			 G_CALLBACK(gzv_if_delete_cb),
			 NULL);
}

void gzv_if_err(const char *msg, const char *fmt, ...)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(GTK_WINDOW(gzv.window),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_OK,
			"%s", msg);

	if (fmt) {
		va_list args;
		char secondary[256];

		va_start(args, fmt);
		vsnprintf(secondary, 255, fmt, args);
		va_end(args);

		gtk_message_dialog_format_secondary_text
			(GTK_MESSAGE_DIALOG(dialog),
			 "%s", secondary);
	}

	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

void gzv_if_create(void)
{
	GtkWidget *top_vbox, *vbox, *frame, *hbox, *scrollbar, *grid, *da;
	GtkAdjustment *vadj;
	struct gzv_zone *zone;
	unsigned int r, c, z = 0;
	unsigned int da_w, da_h;
	char str[128];

	/* Get colors */
	gdk_rgba_parse(&gzv.color_conv, "Magenta");	/* Conv zones */
	gdk_rgba_parse(&gzv.color_seq, "Green");	/* Seq zones */
	gdk_rgba_parse(&gzv.color_seqw, "Red");		/* Seq written/full */
	gdk_rgba_parse(&gzv.color_nonw, "RosyBrown");	/* Non-writable */
	gdk_rgba_parse(&gzv.color_text, "Black");
	gdk_rgba_parse(&gzv.color_oe, "Blue");		/* Exp open zones */
	gdk_rgba_parse(&gzv.color_oi, "DeepSkyBlue");	/* Imp open zones */
	gdk_rgba_parse(&gzv.color_cl, "DarkOrange");	/* Closed zones */
	gdk_rgba_parse(&gzv.color_of, "Grey");		/* Offline zones */

	/* Top vbox */
	top_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_show(top_vbox);
	gtk_container_add(GTK_CONTAINER(gzv.window), top_vbox);

	/* Create a top frame */
	if (!gzv.nr_conv_zones)
		snprintf(str, sizeof(str) - 1,
			 "<b>%s</b>: %u sequential zones",
			 gzv.path, gzv.nr_zones);
	else
		snprintf(str, sizeof(str) - 1,
			 "<b>%s</b>: %u zones (%u conventional + %u sequential)",
			 gzv.path, gzv.nr_zones, gzv.nr_conv_zones,
			 gzv.nr_zones - gzv.nr_conv_zones);
	frame = gtk_frame_new(str);
	gtk_box_pack_start(GTK_BOX(top_vbox), frame, TRUE, TRUE, 0);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_label_set_use_markup(GTK_LABEL(gtk_frame_get_label_widget(GTK_FRAME(frame))), TRUE);
	gtk_frame_set_label_align(GTK_FRAME(frame), 0.05, 0.5);
	gtk_widget_show(frame);

	/* hbox for grid and scrollbar */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_widget_show(hbox);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 10);
	gtk_container_add(GTK_CONTAINER(frame), hbox);

	/* Add a grid */
	grid = gtk_grid_new();
	gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
	gtk_grid_set_row_homogeneous(GTK_GRID(grid), true);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 2);
	gtk_grid_set_column_homogeneous(GTK_GRID(grid), true);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 2);
	gtk_box_pack_start(GTK_BOX(hbox), grid, TRUE, TRUE, 0);
	gtk_widget_show(grid);

	gzv_if_get_da_size(&da_w, &da_h);
	for (r = 0; r < gzv.nr_row; r++) {
		for (c = 0; c < gzv.nr_col; c++) {
			zone = &gzv.grid_zones[z];

			da = gtk_drawing_area_new();
			gtk_widget_set_size_request(da, da_w, da_h);
			gtk_widget_set_hexpand(da, TRUE);
			gtk_widget_set_halign(da, GTK_ALIGN_FILL);
			gtk_widget_show(da);

			zone->da = da;
			gtk_grid_attach(GTK_GRID(grid), da, c, r, 1, 1);
			g_signal_connect(da, "draw",
					 G_CALLBACK(gzv_if_zone_draw_cb), zone);
			z++;
		}
	}

	/* Add scrollbar */
	vadj = gtk_adjustment_new(0, 0, gzv.max_row, 1, 1, gzv.nr_row);
	gzv.vadj = vadj;
	g_signal_connect(vadj, "value-changed",
			 G_CALLBACK(gzv_if_scroll_value_cb), NULL);

	scrollbar = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, vadj);
	gtk_widget_show(scrollbar);
	gtk_box_pack_start(GTK_BOX(hbox), scrollbar, FALSE, FALSE, 0);
	gtk_widget_add_events(scrollbar, GDK_SCROLL_MASK);

	gtk_widget_add_events(gzv.window, GDK_SCROLL_MASK);
	g_signal_connect(gzv.window, "scroll-event",
			 G_CALLBACK(gzv_if_scroll_cb), NULL);

	/* Legend frame */
	frame = gtk_frame_new("<b>Legend</b>");
	gtk_box_pack_start(GTK_BOX(top_vbox), frame, FALSE, TRUE, 0);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_label_set_use_markup(GTK_LABEL(gtk_frame_get_label_widget(GTK_FRAME(frame))), TRUE);
	gtk_frame_set_label_align(GTK_FRAME(frame), 0.05, 0.5);
	gtk_widget_show(frame);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	da = gtk_drawing_area_new();
	gtk_widget_set_size_request(da, -1, 40);
	gtk_widget_show(da);
	gtk_container_add(GTK_CONTAINER(vbox), da);

	g_signal_connect((gpointer) da, "draw",
			 G_CALLBACK(gzv_if_draw_legend_cb), NULL);

	/* Finish setup */
	g_signal_connect(gzv.window, "configure-event",
			 G_CALLBACK(gzv_if_resize_cb), NULL);

	/* Add timer for automatic refresh */
	gzv.refresh_timer = g_timeout_source_new(gzv.refresh_interval);
	g_source_set_name(gzv.refresh_timer, "refresh-timer");
	g_source_set_can_recurse(gzv.refresh_timer, FALSE);
	g_source_set_callback(gzv.refresh_timer, gzv_if_timer_cb,
			      NULL, NULL);
	gzv.last_refresh = gzv_msec();
	g_source_attach(gzv.refresh_timer, NULL);

	gtk_widget_show_all(gzv.window);
	gzv_if_update();
}

void gzv_if_destroy(void)
{

	if (gzv.refresh_timer) {
		g_source_destroy(gzv.refresh_timer);
		gzv.refresh_timer = NULL;
	}

	if (gzv.window) {
		gtk_widget_destroy(gzv.window);
		gzv.window = NULL;
	}
}
