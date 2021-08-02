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
#include <math.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

/*
 * PLPlot
 */
#include <cairo.h>
#include <cairo-ps.h>
#include <plplot.h>

/*
 * Fit file decoding
 */
#include "decode.h"

#define NSIZE 1001

struct PlotData {
  PLFLT x[NSIZE];
  PLFLT y[NSIZE];
  PLFLT xmin; //data, world
  PLFLT xmax;
  PLFLT ymin; 
  PLFLT ymax;
  PLFLT xvmin; //view coordinates, world
  PLFLT xvmax;
  PLFLT yvmin; 
  PLFLT yvmax;
  PLFLT zmxmin; //zoom limits, world
  PLFLT zmxmax;
  PLFLT zmymin; 
  PLFLT zmymax;
  PLFLT zm_startx; //zoom coordinates, world
  PLFLT zm_starty; 
  PLFLT zm_endx;
  PLFLT zm_endy;
  char * symbol;
};

enum ZoomState {Press = 0, Move = 1, Release = 2}; 

/* 
 * make UI elements globals (ick)
 */
GtkSpinButton * sb_int;
GtkSpinButton * sb_numint;
GtkDrawingArea * da;
GtkButton * b_execute;
GtkScaleButton *xscale_btn;
GtkSpinButton * sb_xscale;
GtkSpinButton * sb_yscale;
GtkSpinButton * sb_x_trans;
GtkSpinButton * sb_y_trans;
GtkScaleButton * scb_x;
GtkScaleButton * scb_y;
GtkButton * btn_pan_up;
GtkButton * btn_pan_down;
GtkButton * btn_pan_right;
GtkButton * btn_pan_left;

float float_rand( float min, float max )
{
    float scale = rand() / (float) RAND_MAX; /* [0, 1.0] */
    return min + scale * ( max - min );      /* [min, max] */
}

/* Get get file data. */
int get_fit_file_data() {
  char * fname = "./fitfiles/2021-08-02-08-14-52.fit";
  float speed[NSIZE], dist[NSIZE], lat[NSIZE], lng[NSIZE];
  

  for (int i = 0; i<NSIZE; i++) {
    speed[i] = 0.0/0.0;  //initialize to NaN
    dist[i] = 0.0/0.0;  //initialize to NaN
    lat[i] = 0.0/0.0;  //initialize to NaN
    lng[i] = 0.0/0.0;  //initialize to NaN
  }
  int rtnval = get_fit_records(fname, speed, dist, lat, lng);
  if (rtnval != 0) {
    printf("Could not load activity records.\n");
  } else {
    for (int i = 0; i<NSIZE; i++) {
      printf("i =%d, speed = %0.3f m/s, distance = %0.3f m, latitude = %0.6f deg, longitude = %0.6f deg \n", i, speed[i], dist[i], lat[i], lng[i]);
    }
  }
  return 0;
}

/* Prepare data to be plotted. */
struct PlotData* init_plot_data() {

  int i;
  static struct PlotData pdata;
  struct PlotData *p;

  get_fit_file_data();

  /* Defaults */
  p = &pdata;
  p->symbol = "⏺";
  p->xmin =  9999999;
  p->xmax = -9999999;
  p->ymin =  9999999; 
  p->ymax = -9999999;

  /* Input distances in miles (for testing). */
  for (i = 0; i < NSIZE; i++) {
    p->x[i] = (PLFLT)(i * 0.01);  /* dummy data */
    /* Find min, max */
    if (p->x[i] < p->xmin) {
      p->xmin = p->x[i];
    }
    if (p->x[i] > p->xmax) {
      p->xmax = p->x[i];
    }
  }
  for (i = 0; i < NSIZE/2; i++) {
    p->y[i] = float_rand(2.5, 3);
  }
  for (i = NSIZE/2; i < NSIZE; i++) {
    p->y[i] = float_rand(3, 4);
  }

    /* Find min, max */
  for (i = 0; i < NSIZE; i++) {
    if (p->y[i] < p->ymin) {
      p->ymin = p->y[i];
    }
    if (p->y[i] > p->ymax) {
      p->ymax = p->y[i];
    }
  }
  p->xvmax = p->xmax;
  p->yvmin = p->ymin;
  p->yvmax = p->ymax;
  p->xvmin = p->xmin;
  return p;
}

/* A custom axis labeling function for pace chart. */
void custom_labeler(PLINT axis, PLFLT value, char * label, PLINT length, 
    PLPointer PL_UNUSED(data)) {
  PLFLT label_val = 0.0;
  PLFLT min_per_mile = 0.0;
  label_val = value;
  if (axis == PL_Y_AXIS) {
    if (label_val > 0) {
      min_per_mile = 26.8224 / label_val;
    } else {
      min_per_mile = 999.0;
    }
  }
  double secs, mins;
  secs = modf(min_per_mile, &mins);
  secs *= 60.0;

  if (axis == PL_Y_AXIS) {
    snprintf(label, (size_t) length, "%02.0f:%02.0f", mins, secs);
  } else {
    snprintf(label, (size_t) length, "%.1f", label_val);
  }
}

/* Drawing area callback. */
gboolean on_da_draw(GtkWidget * widget,
  GdkEventExpose * event,
  struct PlotData *pd) 
{
  float ch_size = 4.0; //mm
  float scf = 1.0; //dimensionless
  PLFLT n_xmin, n_xmax, n_ymin, n_ymax;
  /* Why do we have to hardcode this???? */
  int width = 700;
  int height = 600;

  /* "Convert" the G*t*kWidget to G*d*kWindow (no, it's not a GtkWindow!) */
  GdkWindow * window = gtk_widget_get_window(widget);
  cairo_region_t * cairoRegion = cairo_region_create();
  GdkDrawingContext * drawingContext;
  drawingContext = gdk_window_begin_draw_frame(window, cairoRegion);
  /* Say: "I want to start drawing". */
  cairo_t * cr = gdk_drawing_context_get_cairo_context(drawingContext);

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
  /* Setup a custom label function. */
  plslabelfunc(custom_labeler, NULL);
  /* Create a labelled box to hold the plot using custom x,y labels. */
  plenv(pd->xvmin, pd->xvmax, pd->yvmin, pd->yvmax, 0, 70);
  pllab("Distance(miles)", "Pace(min/mile)", "Pace Chart");
  /* Plot the data that was prepared above. */
  plline(NSIZE, pd->x, pd->y);
  /* Plot symbols for individual data points. */
  plstring(NSIZE, pd->x, pd->y, pd->symbol);
  /* Calculate the zoom limits (in pixels) for the graph. */
  plgvpd (&n_xmin, &n_xmax, &n_ymin, &n_ymax);
  pd->zmxmin = width * n_xmin;
  pd->zmxmax = width * n_xmax;
  pd->zmymin = height * (n_ymin - 1.0) + height;
  pd->zmymax  = height * (n_ymax - 1.0) + height;
  /*  Draw_selection box. */
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
    plscol0a (1, 65, 209, 65, 0.25);
    plcol0( 1 );
    plfill(4, rb_x, rb_y);
  }
  /* Close PLplot library */
  plend();
  /* Say: "I'm finished drawing. */
  gdk_window_end_draw_frame(window, drawingContext);
  /* Cleanup */
  cairo_region_destroy(cairoRegion);
  return FALSE;
}

/* Set zoom back to zero. */
void reset_zoom(struct PlotData *pd) {
    pd->zm_startx = 0;
    pd->zm_starty = 0;
    pd->zm_endx = 0;
    pd->zm_endy = 0;
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
  float fracty = (pd->zmymax  - event->y) / (pd->zmymax - pd->zmymin);
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
gboolean on_button_press(GtkWidget* widget,
  GdkEvent *event, struct PlotData *pd) {
/* Get start x, y */
  gui_to_world(pd, (GdkEventButton*)event, Press );
  return TRUE;
}

/* Handle mouse button release. */
gboolean on_button_release(GtkWidget* widget,
  GdkEvent *event, struct PlotData *pd) {
  guint buttonnum;
  gdk_event_get_button (event, &buttonnum);
  /* Zoom out if right mouse button release. */
  if (buttonnum == 2) {
    init_plot_data();
    gtk_widget_queue_draw(GTK_WIDGET(da));
    reset_zoom(pd);
    return TRUE;
  }
  /* Zoom in if left mouse button release. */
  gui_to_world(pd, (GdkEventButton*)event, Release);
  if ((pd->zm_startx != pd->zm_endx) && (pd->zm_starty != pd->zm_endy)) {
    /* Zoom */
    if (buttonnum ==3) {
    pd->xvmin = fmin(pd->zm_startx, pd->zm_endx);
    pd->yvmin = fmin(pd->zm_starty, pd->zm_endy);
    pd->xvmax = fmax(pd->zm_startx, pd->zm_endx);
    pd->yvmax = fmax(pd->zm_starty, pd->zm_endy);
    }
    /* Pan */
    if (buttonnum ==1) {
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
gboolean on_motion_notify(GtkWidget* widget,
  GdkEventButton *event, struct PlotData *pd) {
    if (event->state & GDK_BUTTON3_MASK) {
      gui_to_world(pd, event, Move);
      gtk_widget_queue_draw(GTK_WIDGET(da));
    }
  return TRUE;
}

/* This is the program entry point.  The builder reads an XML file (generated  
 * by the Glade application and instantiate the associated (global) objects.
 */
int main(int argc, char * argv[]) {
  GtkBuilder * builder;
  GtkWidget * window;
  struct PlotData* pd;

  gtk_init( & argc, & argv);

  builder = gtk_builder_new_from_file("gtkdraw.glade");

  window = GTK_WIDGET(gtk_builder_get_object(builder, "window1"));
  da = GTK_DRAWING_AREA(gtk_builder_get_object(builder, "da"));

  gtk_builder_connect_signals(builder, NULL);
  pd = init_plot_data();
  gtk_widget_add_events(GTK_WIDGET(da), GDK_BUTTON_PRESS_MASK);
  gtk_widget_add_events(GTK_WIDGET(da), GDK_BUTTON_RELEASE_MASK);
  gtk_widget_add_events(GTK_WIDGET(da), GDK_POINTER_MOTION_MASK);
  g_signal_connect(GTK_DRAWING_AREA(da), "button-press-event", 
      G_CALLBACK(on_button_press), pd);
  g_signal_connect(GTK_DRAWING_AREA(da), "button-release-event", 
      G_CALLBACK(on_button_release), pd);
  g_signal_connect(GTK_DRAWING_AREA(da), "motion-notify-event", 
      G_CALLBACK(on_motion_notify), pd);
  g_signal_connect(GTK_DRAWING_AREA(da), "draw", 
      G_CALLBACK(on_da_draw), pd);
  g_object_unref(builder);

  gtk_widget_show(window);
  gtk_main();

  return 0;
}

/* Call when the window is closed.  Store GUI values before exiting.*/
void on_window1_destroy() {
  gtk_main_quit();
}
