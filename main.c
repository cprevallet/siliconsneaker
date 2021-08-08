/*
 * This program provides a GTK graphical user interface to PLPlot graphical
 * plotting routines.
 *
 * License:
 *
 * Permission is granted to copy, use, and distribute for any commercial
 * or noncommercial purpose in accordance with the requirements of
 * version 2.0 of the GNU General Public license.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * On Debian systems, the complete text of the GNU General
 * Public License can be found in `/usr/share/common-licenses/GPL-2'.
 *
 * - Craig S. Prevallet, July, 2020
 */

/*
 * Required external Debian libraries: libplplot-dev, plplot-driver-cairo,
 * libgtk-3-dev
 */
#include <float.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <math.h>
#include <string.h>

/*
 * PLPlot
 */
#include <cairo-ps.h>
#include <cairo.h>
#include <plplot.h>

/*
 * Champlain map
 */
#include <champlain-gtk/champlain-gtk.h>
#include <champlain/champlain.h>
#include <clutter-gtk/clutter-gtk.h>

/*
 * Fit file decoding
 */
#include "decode.h"

#define NSIZE 2880 // long enough for 4 hour marathon at 5 sec intervals

enum ZoomState { Press = 0, Move = 1, Release = 2 };
enum UnitSystem { Metric = 1, English = 0 };
enum PlotType {
  PacePlot = 1,
  CadencePlot = 2,
  HeartRatePlot = 3,
  AltitudePlot = 4
};

struct PlotData {
  enum PlotType ptype;
  int num_pts;
  PLFLT *x;
  PLFLT *y;
  PLFLT xmin; // data, world coordinates
  PLFLT xmax;
  PLFLT ymin;
  PLFLT ymax;
  PLFLT xvmin; // view, world coordinates
  PLFLT xvmax;
  PLFLT yvmin;
  PLFLT yvmax;
  PLFLT zmxmin; // zoom limits, world coordinates
  PLFLT zmxmax;
  PLFLT zmymin;
  PLFLT zmymax;
  PLFLT zm_startx; // zoom, world coordinates
  PLFLT zm_starty;
  PLFLT zm_endx;
  PLFLT zm_endy;
  PLFLT *lat;
  PLFLT *lng;
  char *start_time;
  char *symbol;
  enum UnitSystem units;
};

/*
 * define global UI elements and structs (ick)
 */
GtkDrawingArea *da;
GtkRadioButton *rb_Pace;
GtkRadioButton *rb_Cadence;
GtkRadioButton *rb_HeartRate;
GtkRadioButton *rb_Altitude;
GtkFileChooserButton *btnFileOpen;
GtkFrame *viewport;
GtkWidget *champlain_widget;
GtkButton *btn_Zoom_In, *btn_Zoom_Out;
GtkComboBoxText *cb_Units;
struct PlotData pdata;
struct PlotData *pd = &pdata;
char *fname = "";
ChamplainView *c_view;
static ChamplainPathLayer *c_path_layer;
static ChamplainMarkerLayer *c_marker_layer;

//
// Graph stuff
//

/* Set the view limits to the data extents. */
void reset_view_limits() {
  pd->xvmax = pd->xmax;
  pd->yvmin = pd->ymin;
  pd->yvmax = pd->ymax;
  pd->xvmin = pd->xmin;
}

/* Set zoom back to zero. */
void reset_zoom() {
  pd->zm_startx = 0;
  pd->zm_starty = 0;
  pd->zm_endx = 0;
  pd->zm_endy = 0;
}

/* Load data into memory and prepare data to be plotted. */
void init_plot_data(char *fname, enum PlotType ptype) {

  float speed[NSIZE], dist[NSIZE], lat[NSIZE], lng[NSIZE], alt[NSIZE];
  int cadence[NSIZE], heart_rate[NSIZE];
  time_t time_stamp[NSIZE];
  int num_recs = 0;
  /* Hey, this needs to be initialized prior to entry. */
  if (pd == NULL) {
    return;
  }
  /* Housekeeping. Release any memory previously allocated before
   * reinitializing. 
   */
  if (pd->x != NULL) {
    free(pd->x);
  }
  if (pd->y != NULL) {
    free(pd->y);
  }
  if (pd->lat != NULL) {
    free(pd->lat);
  }
  if (pd->lng != NULL) {
    free(pd->lng);
  }
  /* Defaults */
  pd->ptype = PacePlot;
  pd->symbol = "";
  pd->xmin = 0;
  pd->xmax = 0;
  pd->ymin = 0;
  pd->ymax = 0;
  pd->num_pts = 0;
  pd->x = NULL;
  pd->y = NULL;
  pd->xvmax = 0;
  pd->yvmin = 0;
  pd->yvmax = 0;
  pd->xvmin = 0;
  pd->zmxmin = 0;
  pd->zmxmax = 0;
  pd->zmymin = 0;
  pd->zmymax = 0;
  pd->zm_startx = 0;
  pd->zm_starty = 0;
  pd->zm_endx = 0;
  pd->zm_endy = 0;
  pd->lat = NULL;
  pd->lng = NULL;
  pd->start_time = "";
  gchar *user_units = gtk_combo_box_text_get_active_text(cb_Units);
  if (!strcmp(user_units, "Metric")) {
    pd->units = Metric;
  } else {
    pd->units = English;
  }
  g_free(user_units);

  /* Establish x,y axis variables */
  pd->ptype = ptype;

  /* Load data from fit file. */
  int rtnval = get_fit_records(fname, speed, dist, lat, lng, cadence,
                               heart_rate, alt, time_stamp, &num_recs);
  if (rtnval != 100) {
    printf("Could not load activity records.\n");
  } else {
    pd->num_pts = num_recs;
    pd->lat = (PLFLT *)malloc(pd->num_pts * sizeof(PLFLT));
    pd->lng = (PLFLT *)malloc(pd->num_pts * sizeof(PLFLT));
    for (int i = 0; i < pd->num_pts; i++) {
      pd->lat[i] = (PLFLT)lat[i];
      pd->lng[i] = (PLFLT)lng[i];
    }
    pd->x = (PLFLT *)malloc(pd->num_pts * sizeof(PLFLT));
    pd->y = (PLFLT *)malloc(pd->num_pts * sizeof(PLFLT));
    // Move into the diplayed variables and do unit conversions
    // from the raw decoded values where necessary.
    float x_cnv, y_cnv;
    switch (pd->ptype) {
    case PacePlot:
      if (pd->units == English) {
        x_cnv = 0.00062137119; // meters to miles
        y_cnv = 0.037282272;   // meters per sec to miles per min
      } else {
        x_cnv = 0.001; // meters to kilometers
        y_cnv = 0.06;  // meters per sec to kilometers per min
      }
      for (int i = 0; i < pd->num_pts; i++) {
        pd->x[i] = (PLFLT)dist[i] * x_cnv;
        if (speed[i] != 0.0) {
          pd->y[i] = (PLFLT)speed[i] * y_cnv;
        } else {
          pd->y[i] = 0.0;
        }
      }
      break;
    case CadencePlot:
      if (pd->units == English) {
        x_cnv = 0.00062137119; // meters to miles
        y_cnv = 1.0;           // steps to steps
      } else {
        x_cnv = 0.001; // meters to kilometers
        y_cnv = 1.0;   // steps to steps
      }
      for (int i = 0; i < pd->num_pts; i++) {
        pd->x[i] = (PLFLT)dist[i] * x_cnv;
        pd->y[i] = (PLFLT)cadence[i] * y_cnv;
      }
      break;
    case HeartRatePlot:
      if (pd->units == English) {
        x_cnv = 0.00062137119; // meters to miles
        y_cnv = 1.0;           // bpm to bpm
      } else {
        x_cnv = 0.001; // meters to kilometers
        y_cnv = 1.0;   // bpm to bpm
      }
      for (int i = 0; i < pd->num_pts; i++) {
        pd->x[i] = (PLFLT)dist[i] * x_cnv;
        pd->y[i] = (PLFLT)heart_rate[i] * y_cnv;
      }
      break;
    case AltitudePlot:
      if (pd->units == English) {
        x_cnv = 0.00062137119; // meters to miles
        y_cnv = 3.28084;       // meters to feet
      } else {
        x_cnv = 0.001; // meters to kilometers
        y_cnv = 1.0;   // meters to meters
      }
      for (int i = 0; i < pd->num_pts; i++) {
        pd->x[i] = (PLFLT)dist[i] * x_cnv;
        pd->y[i] = (PLFLT)alt[i] * y_cnv;
      }
      break;
    }
  }
  /* Set start time in UTC (for title) */
  struct tm *ptm = gmtime(&time_stamp[0]);
  pd->start_time = asctime(ptm);

  /* Set symbol. */
  pd->symbol = "⏺";

  /* Find plot data min, max */
  pd->xmin = FLT_MAX;
  pd->xmax = -FLT_MAX;
  pd->ymin = FLT_MAX;
  pd->ymax = -FLT_MAX;
  for (int i = 0; i < pd->num_pts; i++) {
    if (pd->x[i] < pd->xmin) {
      pd->xmin = pd->x[i];
    }
    if (pd->x[i] > pd->xmax) {
      pd->xmax = pd->x[i];
    }
    if (pd->y[i] < pd->ymin) {
      pd->ymin = pd->y[i];
    }
    if (pd->y[i] > pd->ymax) {
      pd->ymax = pd->y[i];
    }
  }
  reset_view_limits();
  reset_zoom();
}

/* A custom axis labeling function for pace chart in English units. */
void pace_plot_labeler(PLINT axis, PLFLT value, char *label, PLINT length,
                       PLPointer label_data) {
  PLFLT label_val = 0.0;
  PLFLT pace_units = 0.0;
  label_val = value;

  if (axis == PL_Y_AXIS) {
    if (label_val > 0) {
      pace_units = 1 / label_val;
    } else {
      pace_units = 999.0;
    }
    double secs, mins;
    secs = modf(pace_units, &mins);
    secs *= 60.0;
    snprintf(label, (size_t)length, "%02.0f:%02.0f", mins, secs);
  }

  if (axis == PL_X_AXIS) {
    snprintf(label, (size_t)length, "%3.2f", value);
  }
}

/* A custom axis labeling function for cadence chart in English units. */
void cadence_plot_labeler(PLINT axis, PLFLT value, char *label, PLINT length,
                          PLPointer label_data) {
  if (axis == PL_Y_AXIS) {
    snprintf(label, (size_t)length, "%3.2f", value);
  }
  if (axis == PL_X_AXIS) {
    snprintf(label, (size_t)length, "%3.2f", value);
  }
}

/* A custom axis labeling function for heart rate chart in English units. */
void heart_rate_plot_labeler(PLINT axis, PLFLT value, char *label, PLINT length,
                             PLPointer label_data) {
  if (axis == PL_Y_AXIS) {
    snprintf(label, (size_t)length, "%3.0f", value);
  }
  if (axis == PL_X_AXIS) {
    snprintf(label, (size_t)length, "%3.2f", value);
  }
}

/* A custom axis labeling function for altitude chart in English units. */
void altitude_plot_labeler(PLINT axis, PLFLT value, char *label, PLINT length,
                           PLPointer label_data) {
  if (axis == PL_Y_AXIS) {
    snprintf(label, (size_t)length, "%3.0f", value);
  }
  if (axis == PL_X_AXIS) {
    snprintf(label, (size_t)length, "%3.2f", value);
  }
}

/* Drawing area callback. */
gboolean on_da_draw(GtkWidget *widget, GdkEventExpose *event,
                    struct PlotData *pd) {
  float ch_size = 4.0; // mm
  float scf = 1.0;     // dimensionless
  PLFLT n_xmin, n_xmax, n_ymin, n_ymax;
  GtkAllocation allocation;
  gtk_widget_get_allocation(widget, &allocation);
  int width = allocation.width;
  int height = allocation.height;
  // printf("%d, %d\n", width, height);

  if (pd == NULL) {
    return TRUE;
  }

  /* "Convert" the G*t*kWidget to G*d*kWindow (no, it's not a GtkWindow!) */
  GdkWindow *window = gtk_widget_get_window(widget);
  cairo_region_t *cairoRegion = cairo_region_create();
  GdkDrawingContext *drawingContext;
  drawingContext = gdk_window_begin_draw_frame(window, cairoRegion);
  /* Say: "I want to start drawing". */
  cairo_t *cr = gdk_drawing_context_get_cairo_context(drawingContext);

  if ((pd->x != NULL) && (pd->y != NULL)) {
    /* Do your drawing. */
    /* Initialize plplot using the external cairo backend. */
    plsdev("extcairo");

    /* Device attributes */
    plinit();
    pl_cmd(PLESC_DEVINIT, cr);
    /* Color */
    plscol0a(1, 65, 209, 65, 0.25);   // light green for selector
    plscol0a(15, 200, 200, 200, 0.9); // light gray for background
    plscol0a(5, 156, 100, 134, 0.8);  // light magenta for pace
    plscol0a(6, 31, 119, 180, 0.8);   // light blue for heartrate
    plscol0a(7, 255, 127, 14, 0.8);   // light yellow for altitude
    plscol0a(8, 77, 175, 74, 0.8);    // light green for heartrate

    /* Viewport and window */
    pladv(0);
    plvpor(0.15, 0.85, 0.15, 0.85);

    plwind(pd->xvmin, pd->xvmax, pd->yvmin, pd->yvmax);

    /* Adjust character size. */
    plschr(ch_size, scf);
    plcol0(15);

    /* Setup a custom axis tick label function. */
    switch (pd->ptype) {
    case PacePlot:
      // plcol0(5);
      plslabelfunc(pace_plot_labeler, NULL);
      break;
    case CadencePlot:
      // plcol0(6);
      plslabelfunc(cadence_plot_labeler, NULL);
      break;
    case AltitudePlot:
      // plcol0(7);
      plslabelfunc(altitude_plot_labeler, NULL);
      break;
    case HeartRatePlot:
      // plcol0(8);
      plslabelfunc(heart_rate_plot_labeler, NULL);
      break;
    }

    /* Create a labelled box to hold the plot using custom x,y labels. */
    // Do we want finer control here?  Let's try.
    char *xopt = "bnost";
    char *yopt = "bgnost";
    plaxes(pd->xvmin, pd->yvmin, xopt, 0, 0, yopt, 0, 0);

    /* Setup axis labels and titles. */
    switch (pd->ptype) {
    case PacePlot:
      if (pd->units == English) {
        pllab("Distance(miles)", "Pace(min/mile)", pd->start_time);
      } else {
        pllab("Distance(km)", "Pace(min/km)", pd->start_time);
      }
      plcol0(5);
      break;
    case CadencePlot:
      if (pd->units == English) {
        pllab("Distance(miles)", "Cadence(steps/min)", pd->start_time);
      } else {
        pllab("Distance(km)", "Cadence(steps/min)", pd->start_time);
      }
      plcol0(6);
      break;
    case AltitudePlot:
      if (pd->units == English) {
        pllab("Distance(miles)", "Altitude (feet)", pd->start_time);
      } else {
        pllab("Distance(km)", "Altitude(meters)", pd->start_time);
      }
      plcol0(7);
      break;
    case HeartRatePlot:
      if (pd->units == English) {
        pllab("Distance(miles)", "Heart rate (bpm)", pd->start_time);
      } else {
        pllab("Distance(km)", "Heart rate (bpm)", pd->start_time);
      }
      plcol0(8);
      break;
    }
    /* Plot the data that was loaded. */
    plline(pd->num_pts, pd->x, pd->y);
    /* Plot symbols for individual data points. */
    plstring(pd->num_pts, pd->x, pd->y, pd->symbol);
    /* Calculate the zoom limits (in pixels) for the graph. */
    plgvpd(&n_xmin, &n_xmax, &n_ymin, &n_ymax);
    pd->zmxmin = width * n_xmin;
    pd->zmxmax = width * n_xmax;
    pd->zmymin = height * (n_ymin - 1.0) + height;
    pd->zmymax = height * (n_ymax - 1.0) + height;
    /*  Draw_selection box "rubber-band". */
    PLFLT rb_x[4];
    PLFLT rb_y[4];
    rb_x[0] = pd->zm_startx;
    rb_y[0] = pd->zm_starty;
    rb_x[1] = pd->zm_startx;
    rb_y[1] = pd->zm_endy;
    rb_x[2] = pd->zm_endx;
    rb_y[2] = pd->zm_endy;
    rb_x[3] = pd->zm_endx;
    rb_y[3] = pd->zm_starty;
    if ((pd->zm_startx != pd->zm_endx) && (pd->zm_starty != pd->zm_endy)) {
      plcol0(1);
      plfill(4, rb_x, rb_y);
    }
  }
  /* Close PLplot library */
  plend();
  /* Say: "I'm finished drawing. */
  gdk_window_end_draw_frame(window, drawingContext);
  /* Cleanup */
  cairo_region_destroy(cairoRegion);
  return FALSE;
}

/* Calculate the graph ("world") x,y coordinates corresponding to the
 * GUI mouse ("device") coordinates.
 * The plot view bounds (xvmin, xvmax, yvmin, yvmax) and the plot
 * zoom bounds (zmxmin, zmxmax, zmymin, zmymax) are calculated
 * by the draw routine.
 */
void gui_to_world(struct PlotData *pd, GdkEventButton *event,
                  enum ZoomState state) {
  float fractx = (event->x - pd->zmxmin) / (pd->zmxmax - pd->zmxmin);
  float fracty = (pd->zmymax - event->y) / (pd->zmymax - pd->zmymin);
  if (state == Press) {
    pd->zm_startx = fractx * (pd->xvmax - pd->xvmin) + pd->xvmin;
    pd->zm_starty = fracty * (pd->yvmax - pd->yvmin) + pd->yvmin;
  }
  if (state == Release || state == Move) {
    pd->zm_endx = fractx * (pd->xvmax - pd->xvmin) + pd->xvmin;
    pd->zm_endy = fracty * (pd->yvmax - pd->yvmin) + pd->yvmin;
  }
}

/* Change the cursor style. */
void change_cursor(GtkWidget *widget, const gchar *name) {
  /* Change cursor to cross. */
  GdkDisplay *display = gtk_widget_get_display(widget);
  GdkCursor *cursor;
  cursor = gdk_cursor_new_from_name(display, name);
  gdk_window_set_cursor(gtk_widget_get_window(widget), cursor);
  // Release the reference on the cursor
  g_object_unref(cursor);
}

/* Handle mouse button press. */
gboolean on_button_press(GtkWidget *widget, GdkEvent *event,
                         struct PlotData *pd) {
  guint buttonnum;
  gdk_event_get_button(event, &buttonnum);
  if (buttonnum == 3) {
    change_cursor(widget, "crosshair");
  }
  if (buttonnum == 1) {
    change_cursor(widget, "hand1");
  }
  /* Get start x, y */
  gui_to_world(pd, (GdkEventButton *)event, Press);
  return TRUE;
}

/* Handle mouse button release. */
gboolean on_button_release(GtkWidget *widget, GdkEvent *event,
                           struct PlotData *pd) {
  guint buttonnum;
  change_cursor(widget, "default");
  gdk_event_get_button(event, &buttonnum);
  /* Zoom out if right mouse button release. */
  if (buttonnum == 2) {
    reset_view_limits(pd);
    gtk_widget_queue_draw(GTK_WIDGET(da));
    reset_zoom(pd);
    return TRUE;
  }
  /* Zoom in if left mouse button release. */
  gui_to_world(pd, (GdkEventButton *)event, Release);
  if ((pd->zm_startx != pd->zm_endx) && (pd->zm_starty != pd->zm_endy)) {
    /* Zoom */
    if (buttonnum == 3) {
      pd->xvmin = fmin(pd->zm_startx, pd->zm_endx);
      pd->yvmin = fmin(pd->zm_starty, pd->zm_endy);
      pd->xvmax = fmax(pd->zm_startx, pd->zm_endx);
      pd->yvmax = fmax(pd->zm_starty, pd->zm_endy);
    }
    /* Pan */
    if (buttonnum == 1) {
      pd->xvmin = pd->xvmin + (pd->zm_startx - pd->zm_endx);
      pd->xvmax = pd->xvmax + (pd->zm_startx - pd->zm_endx);
      pd->yvmin = pd->yvmin + (pd->zm_starty - pd->zm_endy);
      pd->yvmax = pd->yvmax + (pd->zm_starty - pd->zm_endy);
    }
    gtk_widget_queue_draw(GTK_WIDGET(da));
    reset_zoom(pd);
  }
  return TRUE;
}

/* Handle mouse motion event by drawing a filled
 * polygon.
 */
gboolean on_motion_notify(GtkWidget *widget, GdkEventButton *event,
                          struct PlotData *pd) {

  if (event->state & GDK_BUTTON3_MASK) {
    gui_to_world(pd, event, Move);
    gtk_widget_queue_draw(GTK_WIDGET(da));
  }
  return TRUE;
}

//
// Map Stuff
//

/* Add a latitude, longitude to the path. */
static void append_point(ChamplainPathLayer *layer, gdouble lon, gdouble lat) {
  ChamplainCoordinate *coord;

  coord = champlain_coordinate_new_full(lon, lat);
  champlain_path_layer_add_node(layer, CHAMPLAIN_LOCATION(coord));
}

static void add_marker(ChamplainMarkerLayer *my_marker_layer, gdouble lng,
                       gdouble lat, const ClutterColor *color) {
  ClutterActor *my_marker = champlain_point_new_full(12, color);
  champlain_location_set_location(CHAMPLAIN_LOCATION(my_marker), lat, lng);
  champlain_marker_layer_add_marker(my_marker_layer,
                                    CHAMPLAIN_MARKER(my_marker));
}

/* Update the map. */
static void update_map() {
  /* Define colors for start, end markers.*/
  ClutterColor my_green;
  ClutterColor my_magenta;
  ClutterColor my_blue;
  clutter_color_from_string(&my_green, "rgba(77, 175, 74, 0.9)");
  clutter_color_from_string(&my_magenta, "rgba(156, 100, 134, 0.9)");
  clutter_color_from_string(&my_blue, "rgba(31, 119, 180, 0.9)");
  if ((pd != NULL) && (pd->lat != NULL) && (pd->lng != NULL)) {
    /* Center at the start. */
    champlain_view_center_on(CHAMPLAIN_VIEW(c_view), pd->lat[0], pd->lng[0]);
    /* Remove any existing layers if we are reloading. */
    if (c_path_layer != NULL) {
      champlain_view_remove_layer(c_view, CHAMPLAIN_LAYER(c_path_layer));
    }
    if (c_marker_layer != NULL) {
      champlain_view_remove_layer(c_view, CHAMPLAIN_LAYER(c_marker_layer));
    }
    champlain_view_reload_tiles(c_view);
    /*Create new layers. */
    c_path_layer = champlain_path_layer_new();
    champlain_path_layer_set_stroke_color(c_path_layer, &my_blue);
    c_marker_layer = champlain_marker_layer_new();
    /* Add start and stop markers. Green for start. Red for end. */
    add_marker(c_marker_layer, pd->lng[pd->num_pts - 1],
               pd->lat[pd->num_pts - 1], &my_magenta);
    add_marker(c_marker_layer, pd->lng[0], pd->lat[0], &my_green);
    /* Add a path */
    for (int i = 0; i < pd->num_pts; i++) {
      append_point(c_path_layer, pd->lat[i], pd->lng[i]);
    }
    champlain_view_add_layer(c_view, CHAMPLAIN_LAYER(c_path_layer));
    champlain_view_add_layer(c_view, CHAMPLAIN_LAYER(c_marker_layer));
  } else {
    /* Start-up. */
    champlain_view_center_on(CHAMPLAIN_VIEW(c_view), 0, 0);
  }
}

/* Zoom in. */
static void zoom_in(GtkWidget *widget, ChamplainView *c_view) {
  champlain_view_zoom_in(c_view);
}

/* Zoom out. */
static void zoom_out(GtkWidget *widget, ChamplainView *c_view) {
  champlain_view_zoom_out(c_view);
}

//
// Navigation Stuff
//
/* Default to the pace chart.  */
gboolean default_chart() {
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb_Pace), TRUE);
  return TRUE;
}

gboolean do_plot() {
  if ((fname != NULL) && (fname[0] != '\0')) {
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_Pace))) {
      init_plot_data(fname, PacePlot);
    }
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_Cadence))) {
      init_plot_data(fname, CadencePlot);
    }
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_HeartRate))) {
      init_plot_data(fname, HeartRatePlot);
    }
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_Altitude))) {
      init_plot_data(fname, AltitudePlot);
    }
    gtk_widget_queue_draw(GTK_WIDGET(da));
    return FALSE;
  } else {
    return TRUE;
  }
}
/* User has changed unit system.*/
void on_cb_units_changed(GtkComboBox *cb_Units, gpointer data) { do_plot(); }

/* User has selected Pace Graph. */
void on_rb_pace(GtkToggleButton *togglebutton, gpointer data) { do_plot(); }

/* User has selected Cadence Graph. */
void on_rb_cadence(GtkToggleButton *togglebutton, gpointer data) { do_plot(); }

/* User has selected Heartrate Graph. */
void on_rb_heartrate(GtkToggleButton *togglebutton, gpointer data) {
  do_plot();
}

/* User has selected Heartrate Graph. */
void on_rb_altitude(GtkToggleButton *togglebutton, gpointer data) { do_plot(); }

/* User has pressed open a new file. */
void on_btnFileOpen_file_set() {
  /* fname is a global */
  fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(btnFileOpen));
  printf("open fname = %s\n", fname);
  // TODO Should load the fitfile once here in a routine
  // and then move the associated arrays each time we call from init_plot_data
  // from do_plot.
  do_plot();
  update_map();
}

//
// Main
//

/* This is the program entry point.  The builder reads an XML file (generated
 * by the Glade application and instantiate the associated (global) objects.
 */
int main(int argc, char *argv[]) {
  GtkBuilder *builder;
  GtkWidget *window;

  gtk_init(&argc, &argv);

  builder = gtk_builder_new_from_file("gtkdraw.glade");

  window = GTK_WIDGET(gtk_builder_get_object(builder, "window1"));
  viewport = GTK_FRAME(gtk_builder_get_object(builder, "viewport"));
  da = GTK_DRAWING_AREA(gtk_builder_get_object(builder, "da"));
  rb_Pace = GTK_RADIO_BUTTON(gtk_builder_get_object(builder, "rb_Pace"));
  rb_Cadence = GTK_RADIO_BUTTON(gtk_builder_get_object(builder, "rb_Cadence"));
  rb_HeartRate =
      GTK_RADIO_BUTTON(gtk_builder_get_object(builder, "rb_HeartRate"));
  rb_Altitude =
      GTK_RADIO_BUTTON(gtk_builder_get_object(builder, "rb_Altitude"));
  btnFileOpen =
      GTK_FILE_CHOOSER_BUTTON(gtk_builder_get_object(builder, "btnFileOpen"));
  btn_Zoom_In = GTK_BUTTON(gtk_builder_get_object(builder, "btn_Zoom_In"));
  btn_Zoom_Out = GTK_BUTTON(gtk_builder_get_object(builder, "btn_Zoom_Out"));
  cb_Units = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "cb_Units"));

  /* Select a default chart to start. */
  default_chart();

  /* Initialize champlain map and add it to a frame after initializing clutter.
   */
  if (gtk_clutter_init(&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;
  champlain_widget = gtk_champlain_embed_new();
  c_view = gtk_champlain_embed_get_view(GTK_CHAMPLAIN_EMBED(champlain_widget));
  clutter_actor_set_reactive(CLUTTER_ACTOR(c_view), TRUE);
  g_object_set(G_OBJECT(c_view), "kinetic-mode", TRUE, "zoom-level", 14, NULL);
  gtk_widget_set_size_request(champlain_widget, 640, 480);
  gtk_container_add(GTK_CONTAINER(viewport), champlain_widget);
  gtk_widget_show_all(window);

  /* Signals and events */
  gtk_builder_connect_signals(builder, NULL);
  gtk_widget_add_events(GTK_WIDGET(da), GDK_BUTTON_PRESS_MASK);
  gtk_widget_add_events(GTK_WIDGET(da), GDK_BUTTON_RELEASE_MASK);
  gtk_widget_add_events(GTK_WIDGET(da), GDK_POINTER_MOTION_MASK);
  g_signal_connect(GTK_DRAWING_AREA(da), "button-press-event",
                   G_CALLBACK(on_button_press), pd);
  g_signal_connect(GTK_DRAWING_AREA(da), "button-release-event",
                   G_CALLBACK(on_button_release), pd);
  g_signal_connect(GTK_DRAWING_AREA(da), "motion-notify-event",
                   G_CALLBACK(on_motion_notify), pd);
  g_signal_connect(GTK_DRAWING_AREA(da), "draw", G_CALLBACK(on_da_draw), pd);
  g_signal_connect(GTK_RADIO_BUTTON(rb_Pace), "toggled", G_CALLBACK(on_rb_pace),
                   NULL);
  g_signal_connect(GTK_RADIO_BUTTON(rb_Cadence), "toggled",
                   G_CALLBACK(on_rb_cadence), NULL);
  g_signal_connect(GTK_RADIO_BUTTON(rb_HeartRate), "toggled",
                   G_CALLBACK(on_rb_heartrate), NULL);
  g_signal_connect(GTK_RADIO_BUTTON(rb_Altitude), "toggled",
                   G_CALLBACK(on_rb_altitude), NULL);
  g_signal_connect(GTK_BUTTON(btn_Zoom_In), "clicked", G_CALLBACK(zoom_in),
                   c_view);
  g_signal_connect(GTK_BUTTON(btn_Zoom_Out), "clicked", G_CALLBACK(zoom_out),
                   c_view);
  g_signal_connect(GTK_COMBO_BOX_TEXT(cb_Units), "changed",
                   G_CALLBACK(on_cb_units_changed), pd);

  g_object_unref(builder);

  /* Required to display champlain widget. */
  gtk_widget_show(window);

  gtk_main();

  return 0;
}

/* Call when the window is closed.  Store GUI values before exiting.*/
void on_window1_destroy() { gtk_main_quit(); }
