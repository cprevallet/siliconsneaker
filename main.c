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
#include <gtk/gtk.h>

#include <gdk/gdk.h>

/*
 * PLPlot
 */
#include <cairo.h>

#include <cairo-ps.h>

#include <plplot.h>

#define NSIZE 11

struct PlotData {
  PLFLT x[NSIZE];
  PLFLT y[NSIZE];
  PLFLT xmin;
  PLFLT xmax;
  PLFLT ymin; 
  PLFLT ymax;
  PLFLT xvmin;
  PLFLT xvmax;
  PLFLT yvmin; 
  PLFLT yvmax;
  char * symbol;
};


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

/* Prepare data to be plotted. */
struct PlotData* init_plot_data() {

  int i;
  static struct PlotData pdata;
  struct PlotData *p;

  /* Defaults */
  p = &pdata;
  p->symbol = "⏺";
  p->xmin =  9999999;
  p->xmax = -9999999;
  p->ymin =  9999999; 
  p->ymax = -9999999;
  /* Input distances in miles (for testing). */
  for (i = 0; i < NSIZE; i++) {
    p->x[i] = (PLFLT)(i * 0.25);  /* dummy data */
    if (p->x[i] < p->xmin) {
      p->xmin = p->x[i];
    }
    if (p->x[i] > p->xmax) {
      p->xmax = p->x[i];
    }
  }
  //TODO NICE axis limits
  p->xmin = 0.9 * p->xmin;
  p->xmax = 1.1 * p->xmax;
  /* Input speeds in meters/sec., dummy data */
  p->y[0] = 3.0;
  p->y[1] = 3.2;
  p->y[2] = 2.7;
  p->y[3] = 2.8;
  p->y[4] = 2.9;
  p->y[5] = 2.85;
  p->y[6] = 3.0;
  p->y[7] = 3.1;
  p->y[8] = 3.2;
  p->y[9] = 3.3;
  p->y[10] = 3.0;
  for (i = 0; i < NSIZE; i++) {
    if (p->y[i] < p->ymin) {
      p->ymin = p->y[i];
    }
    if (p->y[i] > p->ymax) {
      p->ymax = p->y[i];
    }
  }
  //TODO NICE axis limits
  p->ymin = 0.9 * p->ymin;
  p->ymax = 1.1 * p->ymax;
  p->xvmin = p->xmin;
  p->xvmax = p->xmax;
  p->yvmin = p->ymin;
  p->yvmax = p->ymax;
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
  if (axis == PL_Y_AXIS) {
    snprintf(label, (size_t) length, "%.1f", min_per_mile);
  } else {
    snprintf(label, (size_t) length, "%.1f", label_val);
  }
}

/* Drawing area callback. */
gboolean on_da_draw(GtkWidget * widget,
  GdkEventExpose * event,
  struct PlotData *pd) 
{
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
  plinit();
  plcol0(15);
  pl_cmd(PLESC_DEVINIT, cr);
  /* Setup a custom label function. */
  plslabelfunc(custom_labeler, NULL);
  /* Create a labelled box to hold the plot using custom x,y labels. */
  plenv(pd->xvmin, pd->xvmax, pd->yvmin, pd->yvmax, 0, 70);
  pllab("Distance(miles)", "Pace(min/mile)", "Pace Chart");
  /* Plot the data that was prepared above. */
  plline(NSIZE, pd->x, pd->y);
  /* Plot symbols for individual data points. */
  plstring(NSIZE, pd->x, pd->y, pd->symbol);
  /* Close PLplot library */
  plend();
  /* Say: "I'm finished drawing. */
  gdk_window_end_draw_frame(window, drawingContext);
  /* Cleanup */
  cairo_region_destroy(cairoRegion);
  return FALSE;
}

/* Store the GUI variables in user's home directory.*/
int store_ini() {
  //FILE * fp, * fopen();
  char s[84];

  char * t = getenv("HOME");
  strcpy(s, "gtkdraw.ini");
  if (t && strlen(t) < 70) {
    strcpy(s, t);
    strcat(s, "/.gtkdraw.ini");
  }
  return 0;
}

/* Load the values stored in the ini file as initial widget values.
 * Initialize the calendar and time widgets with the current UTC values.
 */
int initialize_widgets() {
  return 0;
}

/* Clear out the text buffer when clear button is pressed. */
void _clear_results(GtkButton * b) {
}

/* Rescale on button clicked. */
void on_clicked(GtkButton * b, struct PlotData *pd) {
  pd->xmax = pd->xmax - 0.1 * (pd->xmax - pd->xmin);
  pd->xmin = pd->xmin + 0.1 * (pd->xmax - pd->xmin);
  gtk_widget_queue_draw(GTK_WIDGET(da));
}

void
rescale_x_axis (GtkScaleButton *button,
               double          value,
               struct PlotData *pd) {
  pd->xvmin = pd->xmin + (100.0 - gtk_scale_button_get_value (button)) / 100.0 * (pd->xmax - pd->xmin);
  pd->xvmax = pd->xmax - (100.0 - gtk_scale_button_get_value (button)) / 100.0 * (pd->xmax - pd->xmin);
  gtk_widget_queue_draw(GTK_WIDGET(da));
}
void
rescale_y_axis (GtkScaleButton *button,
               double          value,
               struct PlotData *pd) {
  pd->yvmin = pd->ymin + (100.0 - gtk_scale_button_get_value (button)) / 100.0 * (pd->ymax - pd->ymin);
  pd->yvmax = pd->ymax - (100.0 - gtk_scale_button_get_value (button)) / 100.0 * (pd->ymax - pd->ymin);
  gtk_widget_queue_draw(GTK_WIDGET(da));
}
void
trans_right(GtkButton *button,
               double          value,
               struct PlotData *pd) {
  pd->xvmin = pd->xvmin + (pd->xmax - pd->xmin) * 0.10;
  pd->xvmax = pd->xvmax + (pd->xmax - pd->xmin) * 0.10;
  gtk_widget_queue_draw(GTK_WIDGET(da));
}
void
trans_left(GtkButton *button,
               double          value,
               struct PlotData *pd) {
  pd->xvmin = pd->xvmin - (pd->xmax - pd->xmin) * 0.10;
  pd->xvmax = pd->xvmax - (pd->xmax - pd->xmin) * 0.10;
  gtk_widget_queue_draw(GTK_WIDGET(da));
}
void
trans_down(GtkButton *button,
               double          value,
               struct PlotData *pd) {
  pd->yvmin = pd->yvmin + (pd->ymax - pd->ymin) * 0.10;
  pd->yvmax = pd->yvmax + (pd->ymax - pd->ymin) * 0.10;
  gtk_widget_queue_draw(GTK_WIDGET(da));
}
void
trans_up(GtkButton *button,
               double          value,
               struct PlotData *pd) {
  pd->yvmin = pd->yvmin - (pd->ymax - pd->ymin) * 0.10;
  pd->yvmax = pd->yvmax - (pd->ymax - pd->ymin) * 0.10;
  gtk_widget_queue_draw(GTK_WIDGET(da));
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
  //sb_int = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sb_int"));
  //sb_numint = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sb_numint"));
  da = GTK_DRAWING_AREA(gtk_builder_get_object(builder, "da"));
  // b_execute = GTK_BUTTON(gtk_builder_get_object(builder, "b_execute"));
  //sb_xscale = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sb_xscale"));
  //sb_yscale = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sb_yscale"));
  scb_x = GTK_SCALE_BUTTON(gtk_builder_get_object(builder, "scb_x"));
  scb_y = GTK_SCALE_BUTTON(gtk_builder_get_object(builder, "scb_y"));
  btn_pan_left = GTK_BUTTON(gtk_builder_get_object(builder, "btn_pan_left"));
  btn_pan_right = GTK_BUTTON(gtk_builder_get_object(builder, "btn_pan_right"));
  btn_pan_up = GTK_BUTTON(gtk_builder_get_object(builder, "btn_pan_up"));
  btn_pan_down = GTK_BUTTON(gtk_builder_get_object(builder, "btn_pan_down"));

  gtk_builder_connect_signals(builder, NULL);
  pd = init_plot_data();
  g_signal_connect(GTK_DRAWING_AREA(da), "draw", G_CALLBACK(on_da_draw), pd);
  //g_signal_connect(GTK_BUTTON(b_execute), "clicked", G_CALLBACK(on_clicked), pd);
  g_signal_connect(GTK_SCALE_BUTTON(scb_x), "value-changed", G_CALLBACK(rescale_x_axis), pd);
  g_signal_connect(GTK_SCALE_BUTTON(scb_y), "value-changed", G_CALLBACK(rescale_y_axis), pd);
  g_signal_connect(GTK_BUTTON(btn_pan_left), "clicked", G_CALLBACK(trans_left), pd);
  g_signal_connect(GTK_BUTTON(btn_pan_right), "clicked", G_CALLBACK(trans_right), pd);
  g_signal_connect(GTK_BUTTON(btn_pan_up), "clicked", G_CALLBACK(trans_up), pd);
  g_signal_connect(GTK_BUTTON(btn_pan_down), "clicked", G_CALLBACK(trans_down), pd);

  /* Retrieve initial values from .ini file.*/
  initialize_widgets();


  g_object_unref(builder);

  gtk_widget_show(window);
  gtk_main();

  return 0;
}

/* Call when the window is closed.  Store GUI values before exiting.*/
void on_window1_destroy() {
  store_ini();
  gtk_main_quit();
}
