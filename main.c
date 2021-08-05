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

/*
 * PLPlot
 */
#include <cairo-ps.h>
#include <cairo.h>
#include <plplot.h>

/*
 * Champlain map
 */
#include <champlain/champlain.h>
#include <champlain-gtk/champlain-gtk.h>
#include <clutter-gtk/clutter-gtk.h>

/*
 * Fit file decoding
 */
#include "decode.h"

#define NSIZE 2880 // long enough for 4 hour marathon at 5 sec intervals

enum ZoomState { Press = 0, Move = 1, Release = 2 };
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
  char *symbol;
};

/*
 * make UI elements and structs globals (ick)
 */
GtkDrawingArea *da;
GtkRadioButton *rb_Pace;
GtkRadioButton *rb_Cadence;
GtkRadioButton *rb_HeartRate;
GtkRadioButton *rb_Altitude;
GtkFileChooserButton *btnFileOpen;
GtkFrame *viewport;
char *fname;
GtkWidget *champlain_widget;
ChamplainView *champlain_view;
GtkButton *btn_Zoom_In, *btn_Zoom_Out;
struct PlotData pdata;
struct PlotData *pd = &pdata;

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
  printf("init_plot_data called\n");

  if (pd == NULL) { return; }

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

  /* Establish x,y axis variables */
  pd->ptype = ptype;

  /* Load data from fit file. */
  int rtnval = get_fit_records(fname, speed, dist, lat, lng, cadence,
                               heart_rate, alt, time_stamp, &num_recs);
  if (rtnval != 100) {
    printf("Could not load activity records.\n");
  } else {
    pd->num_pts = num_recs;
    pd->x = (PLFLT *)malloc(pd->num_pts * sizeof(PLFLT));
    pd->y = (PLFLT *)malloc(pd->num_pts * sizeof(PLFLT));
    switch (pd->ptype) {
    case PacePlot:
      for (int i = 0; i < pd->num_pts; i++) {
        pd->x[i] = (PLFLT)dist[i];
        if (speed[i] != 0.0) {
          pd->y[i] = (PLFLT)speed[i];
        } else {
          pd->y[i] = 0.0;
        }
      }
      break;
    case CadencePlot:
      for (int i = 0; i < pd->num_pts; i++) {
        pd->x[i] = (PLFLT)dist[i];
        pd->y[i] = (PLFLT)cadence[i];
      }
      break;
    case HeartRatePlot:
      for (int i = 0; i < pd->num_pts; i++) {
        pd->x[i] = (PLFLT)dist[i];
        pd->y[i] = (PLFLT)heart_rate[i];
      }
      break;
    case AltitudePlot:
      for (int i = 0; i < pd->num_pts; i++) {
        pd->x[i] = (PLFLT)dist[i];
        pd->y[i] = (PLFLT)alt[i];
      }
      break;
    }

    /*
    struct tm * ptm = gmtime(&time_stamp[i]);
    printf("i =%d, \
        speed = %0.3f m/s, \
        distance = %0.3f m, \
        latitude = %0.6f deg,  \
        longitude = %0.6f deg, \
        cadence = %d steps,  \
        heart_rate = %d bpm, \
        time_stamp =%s UTC \n", i, speed[i], dist[i], lat[i], lng[i],
    cadence[i], heart_rate[i], asctime(ptm));
  */
  }

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
  PLFLT min_per_mile = 0.0;
  label_val = value;

  if (axis == PL_Y_AXIS) {
    if (label_val > 0) {
      min_per_mile = 26.8224 / label_val;
    } else {
      min_per_mile = 999.0;
    }
    double secs, mins;
    secs = modf(min_per_mile, &mins);
    secs *= 60.0;
    snprintf(label, (size_t)length, "%02.0f:%02.0f", mins, secs);
  }

  if (axis == PL_X_AXIS) {
    snprintf(label, (size_t)length, "%3.2f", 0.00062137119 * value);
  }
}

/* A custom axis labeling function for cadence chart in English units. */
void cadence_plot_labeler(PLINT axis, PLFLT value, char *label, PLINT length,
                          PLPointer label_data) {
  if (axis == PL_Y_AXIS) {
    snprintf(label, (size_t)length, "%3.2f", value);
  }
  if (axis == PL_X_AXIS) {
    snprintf(label, (size_t)length, "%3.2f", 0.00062137119 * value);
  }
}

/* A custom axis labeling function for heart rate chart in English units. */
void heart_rate_plot_labeler(PLINT axis, PLFLT value, char *label, PLINT length,
                             PLPointer label_data) {
  if (axis == PL_Y_AXIS) {
    snprintf(label, (size_t)length, "%3.0f", value);
  }
  if (axis == PL_X_AXIS) {
    snprintf(label, (size_t)length, "%3.2f", 0.00062137119 * value);
  }
}

/* A custom axis labeling function for altitude chart in English units. */
void altitude_plot_labeler(PLINT axis, PLFLT value, char *label, PLINT length,
                           PLPointer label_data) {
  if (axis == PL_Y_AXIS) {
    snprintf(label, (size_t)length, "%3.0f", 3.28084 * value);
  }
  if (axis == PL_X_AXIS) {
    snprintf(label, (size_t)length, "%3.2f", 0.00062137119 * value);
  }
}

/* Drawing area callback. */
gboolean on_da_draw(GtkWidget *widget, GdkEventExpose *event,
                    struct PlotData *pd) {
  float ch_size = 4.0; // mm
  float scf = 1.0;     // dimensionless
  PLFLT n_xmin, n_xmax, n_ymin, n_ymax;
  /* Why do we have to hardcode this???? */
  int width = 700;
  int height = 600;

  printf("on_da_draw called\n");

  if (pd == NULL) { return TRUE; }

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
    plcol0(15);
    /* Adjust character size. */
    plschr(ch_size, scf);
    /* Setup a custom axis tick label function. */
    switch (pd->ptype) {
    case PacePlot:
      plslabelfunc(pace_plot_labeler, NULL);
      break;
    case CadencePlot:
      plslabelfunc(cadence_plot_labeler, NULL);
      break;
    case HeartRatePlot:
      plslabelfunc(heart_rate_plot_labeler, NULL);
      break;
    case AltitudePlot:
      plslabelfunc(altitude_plot_labeler, NULL);
      break;
    }
    /* Create a labelled box to hold the plot using custom x,y labels. */
    plenv(pd->xvmin, pd->xvmax, pd->yvmin, pd->yvmax, 0, 70);
    /* Setup a custom chart label function. */
    switch (pd->ptype) {
    case PacePlot:
      pllab("Distance(miles)", "Pace(min/mile)", "Pace Chart");
      break;
    case CadencePlot:
      pllab("Distance(miles)", "Cadence(steps/min)", "Cadence Chart");
      break;
    case HeartRatePlot:
      pllab("Distance(miles)", "Heart rate (bpm)", "Heartrate Chart");
      break;
    case AltitudePlot:
      pllab("Distance(miles)", "Altitude (feet)", "Altitude Chart");
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
      plscol0a(1, 65, 209, 65, 0.25);
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

/* Handle mouse button press. */
gboolean on_button_press(GtkWidget *widget, GdkEvent *event,
                         struct PlotData *pd) {
  /* Get start x, y */
  gui_to_world(pd, (GdkEventButton *)event, Press);
  return TRUE;
}

/* Handle mouse button release. */
gboolean on_button_release(GtkWidget *widget, GdkEvent *event,
                           struct PlotData *pd) {
  guint buttonnum;
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
static void update_map() {
  
  champlain_view_center_on (CHAMPLAIN_VIEW (champlain_view), 29.709889,-95.755781);
  //champlain_view_center_on (CHAMPLAIN_VIEW (champlain_view), 29.709889,-95.755781);
}

static void
zoom_in (GtkWidget *widget,
    ChamplainView *champlain_view)
{
  champlain_view_zoom_in (champlain_view);
}


static void
zoom_out (GtkWidget *widget,
    ChamplainView *view)
{
  champlain_view_zoom_out (champlain_view);
}

//
// Navigation Stuff
//

/* Load the values stored in the ini file as initial widget values.
 * Initialize the calendar and time widgets with the current UTC values.
 */
gboolean initialize_widgets() {
  //gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb_Pace), TRUE);
  return TRUE;
}

/* User has selected Pace Graph. */
void on_rb_pace(GtkToggleButton *togglebutton, gpointer userdata) {
  init_plot_data(fname, PacePlot);
  gtk_widget_queue_draw(GTK_WIDGET(da));
}

/* User has selected Cadence Graph. */
void on_rb_cadence(GtkToggleButton *togglebutton, gpointer userdata) {
  init_plot_data(fname, CadencePlot);
  gtk_widget_queue_draw(GTK_WIDGET(da));
}

/* User has selected Heartrate Graph. */
void on_rb_heartrate(GtkToggleButton *togglebutton, gpointer userdata) {
  init_plot_data(fname, HeartRatePlot);
  gtk_widget_queue_draw(GTK_WIDGET(da));
}

/* User has selected Heartrate Graph. */
void on_rb_altitude(GtkToggleButton *togglebutton, gpointer userdata) {
  init_plot_data(fname, AltitudePlot);
  gtk_widget_queue_draw(GTK_WIDGET(da));
}

/* User has pressed open a new file. */
void on_btnFileOpen_file_set() {
  /* fname is a global */
  fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(btnFileOpen));
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_Pace))) {
    on_rb_pace(GTK_TOGGLE_BUTTON(rb_Pace), NULL);
  }
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_Cadence))) {
    on_rb_cadence(GTK_TOGGLE_BUTTON(rb_Cadence), NULL);
  }
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_HeartRate))) {
    on_rb_heartrate(GTK_TOGGLE_BUTTON(rb_HeartRate), NULL);
  }
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_Altitude))) {
    on_rb_altitude(GTK_TOGGLE_BUTTON(rb_Altitude), NULL);
  }
}

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

  gtk_builder_connect_signals(builder, NULL);

  /* Set initial values.*/
  initialize_widgets();

  /* Initialize champlain map and add it to a frame after initializing clutter. */
  if (gtk_clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;
  champlain_widget = gtk_champlain_embed_new ();
  champlain_view = gtk_champlain_embed_get_view (GTK_CHAMPLAIN_EMBED (champlain_widget));
  //clutter_actor_set_reactive (CLUTTER_ACTOR (champlain_view), TRUE);
    g_object_set (G_OBJECT (champlain_view),
      "kinetic-mode", TRUE,
      "zoom-level", 14,
      NULL);
  gtk_widget_set_size_request (champlain_widget, 640, 480);
  gtk_container_add (GTK_CONTAINER (viewport), champlain_widget);
  gtk_widget_show_all (window);

  /* Signals and events */
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
  g_signal_connect (GTK_BUTTON(btn_Zoom_In), "clicked", G_CALLBACK (zoom_in), champlain_view);
  g_signal_connect (GTK_BUTTON(btn_Zoom_Out), "clicked", G_CALLBACK (zoom_out), champlain_view);

  g_object_unref(builder);

  gtk_widget_show(window);

  on_btnFileOpen_file_set();

  gtk_main();

  return 0;
}

/* Call when the window is closed.  Store GUI values before exiting.*/
void on_window1_destroy() { 
  gtk_main_quit(); 
}
