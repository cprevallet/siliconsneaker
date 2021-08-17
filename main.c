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

/* Required external top level-dependencies.
 *  libplplot.so.17
 *	libchamplain-gtk-0.12.so.0
 *	libchamplain-0.12.so.0
 *	libclutter-gtk-1.0.so.0
 *	libclutter-1.0.so.0
 *	libgtk-3.so.0
 *	libgdk-3.so.0
 *	libcairo.so.2
 *	libgobject-2.0.so.0
 *	libglib-2.0.so.0
 *	libm.so.6 =>
 *	libc.so.6
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

//
// Declarations section
//

// Maximum readable records from a fit file.
// 2880 is large enough for 4 hour marathon at 5 sec intervals
#define NSIZE 2880
// Maximum readable laps from a fit file.
#define LSIZE 400

enum ZoomState { Press = 0, Move = 1, Release = 2 };
enum UnitSystem { Metric = 1, English = 0 };
enum PlotType {
  PacePlot = 1,
  CadencePlot = 2,
  HeartRatePlot = 3,
  AltitudePlot = 4,
  LapPlot = 5
};

/* Map colors */
ClutterColor my_green;
ClutterColor my_magenta;
ClutterColor my_blue;

/* The main data structure for the program defining
 * values for various aspects of displaying a plot
 * including the actual x,y pairs, axis labels,
 * line colors, etc. */
typedef struct PlotData {
  enum PlotType ptype;
  int num_pts;
  PLFLT *x; // xy data pairs, world coordinates
  PLFLT *y;
  PLFLT xmin;
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
  PLFLT *lat; // activity location, degrees lat,lng
  PLFLT *lng;
  char *start_time; // activity start time
  char *symbol;     // plot symbol character
  char *xaxislabel; // axis labels
  char *yaxislabel;
  int linecolor[3]; // rgb attributes
  enum UnitSystem units;
} PlotData;

typedef struct SessionData {
  long int timestamp ;
  long int start_time ;
  float start_position_lat ;
  float start_position_long ;
  float total_elapsed_time ; 
  float total_timer_time ;
  float total_distance ;
  float nec_lat ;
  float nec_long ;
  float swc_lat ;
  float swc_long ;
  float total_work ;
  float total_moving_time ;
  float avg_lap_time ;
  float total_calories ;
  float avg_speed ;
  float max_speed ;
  float total_ascent ;
  float total_descent ;
  float avg_altitude ;
  float max_altitude ;
  float min_altitude ;
  float max_heart_rate ;
  float avg_heart_rate ;
  float max_cadence ;
  float avg_cadence ;
  float avg_temperature ;
  float max_temperature ;
  float min_heart_rate ;
  float total_anaerobic_training_effect ;
  enum UnitSystem units;
} SessionData;

/*
 * Declare global instances of the main data structure (one for
 * each type of plot).
 *
 * There must be a better way make all this available to the
 * relevant routines but I haven't discovered anything cleaner.
 */
PlotData paceplot = {.ptype = PacePlot,
                     .symbol = "⏺",
                     .xmin = 0,
                     .xmax = 0,
                     .ymin = 0,
                     .ymax = 0,
                     .num_pts = 0,
                     .x = NULL,
                     .y = NULL,
                     .xvmax = 0,
                     .yvmin = 0,
                     .yvmax = 0,
                     .xvmin = 0,
                     .zmxmin = 0,
                     .zmxmax = 0,
                     .zmymin = 0,
                     .zmymax = 0,
                     .zm_startx = 0,
                     .zm_starty = 0,
                     .zm_endx = 0,
                     .zm_endy = 0,
                     .lat = NULL,
                     .lng = NULL,
                     .xaxislabel = NULL,
                     .yaxislabel = NULL,
                     // light magenta for pace
                     .linecolor = {156, 100, 134},
                     .start_time = ""};
PlotData cadenceplot = {.ptype = CadencePlot,
                        .symbol = "⏺",
                        .xmin = 0,
                        .xmax = 0,
                        .ymin = 0,
                        .ymax = 0,
                        .num_pts = 0,
                        .x = NULL,
                        .y = NULL,
                        .xvmax = 0,
                        .yvmin = 0,
                        .yvmax = 0,
                        .xvmin = 0,
                        .zmxmin = 0,
                        .zmxmax = 0,
                        .zmymin = 0,
                        .zmymax = 0,
                        .zm_startx = 0,
                        .zm_starty = 0,
                        .zm_endx = 0,
                        .zm_endy = 0,
                        .lat = NULL,
                        .lng = NULL,
                        .xaxislabel = NULL,
                        .yaxislabel = NULL,
                        // light blue for heartrate
                        .linecolor = {31, 119, 180},
                        .start_time = ""};
PlotData heartrateplot = {.ptype = HeartRatePlot,
                          .symbol = "⏺",
                          .xmin = 0,
                          .xmax = 0,
                          .ymin = 0,
                          .ymax = 0,
                          .num_pts = 0,
                          .x = NULL,
                          .y = NULL,
                          .xvmax = 0,
                          .yvmin = 0,
                          .yvmax = 0,
                          .xvmin = 0,
                          .zmxmin = 0,
                          .zmxmax = 0,
                          .zmymin = 0,
                          .zmymax = 0,
                          .zm_startx = 0,
                          .zm_starty = 0,
                          .zm_endx = 0,
                          .zm_endy = 0,
                          .lat = NULL,
                          .lng = NULL,
                          .xaxislabel = NULL,
                          .yaxislabel = NULL,
                          // light yellow for heartrate
                         .linecolor = {247, 250, 191},
                          .start_time = ""};
PlotData altitudeplot = {.ptype = AltitudePlot,
                         .symbol = "⏺",
                         .xmin = 0,
                         .xmax = 0,
                         .ymin = 0,
                         .ymax = 0,
                         .num_pts = 0,
                         .x = NULL,
                         .y = NULL,
                         .xvmax = 0,
                         .yvmin = 0,
                         .yvmax = 0,
                         .xvmin = 0,
                         .zmxmin = 0,
                         .zmxmax = 0,
                         .zmymin = 0,
                         .zmymax = 0,
                         .zm_startx = 0,
                         .zm_starty = 0,
                         .zm_endx = 0,
                         .zm_endy = 0,
                         .lat = NULL,
                         .lng = NULL,
                         .xaxislabel = NULL,
                         .yaxislabel = NULL,
                         // light green for heartrate
                         .linecolor = {77, 175, 74},
                         .start_time = ""};
PlotData lapplot = {.ptype = LapPlot,
                         .symbol = "⏺",
                         .xmin = 0,
                         .xmax = 0,
                         .ymin = 0,
                         .ymax = 0,
                         .num_pts = 0,
                         .x = NULL,
                         .y = NULL,
                         .xvmax = 0,
                         .yvmin = 0,
                         .yvmax = 0,
                         .xvmin = 0,
                         .zmxmin = 0,
                         .zmxmax = 0,
                         .zmymin = 0,
                         .zmymax = 0,
                         .zm_startx = 0,
                         .zm_starty = 0,
                         .zm_endx = 0,
                         .zm_endy = 0,
                         .lat = NULL,
                         .lng = NULL,
                         .xaxislabel = NULL,
                         .yaxislabel = NULL,
                          // light orange for laps
                          .linecolor = {255, 127, 14},
                         .start_time = ""};

/* Rely on the default values for C structures = 0, 0.0 for ints, floats */
struct SessionData sess; 

/* The pointers for the data plots.  There is one for each
 * type of plot and an additional pointer, pd, that is assigned
 * from one of the other four depending on what the user is currently
 * displaying.
 */
struct PlotData *ppace = &paceplot;
struct PlotData *pcadence = &cadenceplot;
struct PlotData *pheart = &heartrateplot;
struct PlotData *paltitude = &altitudeplot;
struct PlotData *plap = &lapplot;
struct PlotData *pd;  //for xy-plots
/* The pointer for the session summary. */
struct SessionData *psd = &sess;

/* Declarations for the GUI widgets. */
GtkDrawingArea *da;
GtkRadioButton *rb_Pace;
GtkRadioButton *rb_Cadence;
GtkRadioButton *rb_HeartRate;
GtkRadioButton *rb_Altitude;
GtkRadioButton *rb_Splits;
GtkFileChooserButton *btnFileOpen;
GtkFrame *viewport;
GtkWidget *champlain_widget;
GtkButton *btn_Zoom_In, *btn_Zoom_Out;
GtkComboBoxText *cb_Units;
GtkScale *sc_IdxPct;
GtkLabel *lbl_val;

/* Declaration for the fit filename. */
char *fname = "";

/* Declarations for Champlain maps. */
ChamplainView *c_view;
static ChamplainPathLayer *c_path_layer;
static ChamplainMarkerLayer *c_marker_layer;

/* Current index */
int currIdx = 0;
//
// Convenience functions.
//
void printfloat(float x, char * name) {
  printf("%s = %f \n", name, x);
}
//
// Summary routines.
//

/* Convenience routine to print a floating point line. */
void print_float_val(float val, char* plabel, char* punit, FILE* fp ){
  if ((plabel != NULL) && (punit != NULL) && (val < FLT_MAX - 1.0)) {
    fprintf( fp, "%-30s", plabel);
    fprintf( fp, "%3s", " = ");
    fprintf( fp, "%10.2f", val );
    fprintf( fp, "%3s", " ");
    fprintf( fp, "%-20s", punit);
    fprintf( fp, "\n");
  }
}

void print_timer_val(float timer, char* plabel, FILE* fp) {
  if (timer < FLT_MAX - 1.0) {
    double hours, secs, mins, extra;
    extra = modf(timer/3600.0, &hours);
    secs = modf(extra * 60.0, &mins);
    secs *= 60.0;
    fprintf( fp, "%-30s", plabel);
    fprintf( fp, "%3s", " = ");
    fprintf(fp, "%4.0f:%02.0f:%02.0f", hours, mins, secs);
    fprintf( fp, "\n");
  }
}

/* Generate the summary report */
void generate_report(SessionData *psd) {
  FILE *fp;
  fp = fopen("runplotter.txt", "w");
  if (fp != NULL) {
    fprintf( fp, "%-30s", "Start time");
    fprintf( fp, "%3s", " = ");
    fprintf( fp, "%s", asctime(gmtime(&psd->start_time)));
    print_float_val(psd->start_position_lat ,"Starting latitude", "deg", fp);
    print_float_val(psd->start_position_long , "Starting longitude", "deg", fp);
    print_timer_val(psd->total_elapsed_time, "Total elapsed time", fp);
    print_timer_val(psd->total_timer_time , "Total timer time", fp);
    if (psd->units == English) 
      print_float_val(psd->total_distance , "Total distance","miles" ,fp);
    else 
      print_float_val(psd->total_distance , "Total distance","kilometers" ,fp);

    //print_float_val(psd->nec_lat , "nec_lat","" ,fp);
    //print_float_val(psd->nec_long , "nec_long","" ,fp);
    //print_float_val(psd->swc_lat , "swc_lat","" ,fp);
    //print_float_val(psd->swc_long , "swc_long","" ,fp);
    //TODO Why is this coming out bogus???
    //print_float_val(psd->total_work , "total_work","" ,fp);
    print_timer_val(psd->total_moving_time , "Total moving time",fp);
    print_timer_val(psd->avg_lap_time , "Average lap time",fp);
    print_float_val(psd->total_calories , "Total calories","kcal" ,fp);
    if (psd->units == English) {
      print_float_val(psd->avg_speed , "Average speed","miles/hour" ,fp);
      print_float_val(psd->max_speed , "Maximum speed","miles/hour" ,fp);
    }
    else {
      print_float_val(psd->avg_speed , "Average speed","kilometers/hour" ,fp);
      print_float_val(psd->max_speed , "Maxium speed","kilometers/hour" ,fp);
    }
    if (psd->units == English) {
      print_float_val(psd->total_ascent , "Total ascent","feet" ,fp);
      print_float_val(psd->total_descent , "Total descent","feet" ,fp);
      print_float_val(psd->avg_altitude , "Average altitude","feet" ,fp);
      print_float_val(psd->max_altitude , "Maximum altitude","feet" ,fp);
      print_float_val(psd->min_altitude , "Minimum altitude","feet" ,fp);
    }
    else {
      print_float_val(psd->total_ascent , "Total ascent","meters" ,fp);
      print_float_val(psd->total_descent , "Total descent","meters" ,fp);
      print_float_val(psd->avg_altitude , "Average altitude","meters" ,fp);
      print_float_val(psd->max_altitude , "Maximum altitude","meters" ,fp);
      print_float_val(psd->min_altitude , "Minimum altitude","meters" ,fp);
    }
    print_float_val(psd->max_heart_rate , "Maximum heart rate","" ,fp);
    print_float_val(psd->avg_heart_rate , "Average heart rate","" ,fp);
    print_float_val(psd->max_cadence , "Maximum cadence","" ,fp);
    print_float_val(psd->avg_cadence , "Average cadence","" ,fp);
    if (psd->units == English) {
      print_float_val(psd->avg_temperature , "Average temperature","deg F" ,fp);
      print_float_val(psd->max_temperature , "Maximum temperature","deg F" ,fp);
    }
    else {
      print_float_val(psd->avg_temperature , "Average temperature","deg C" ,fp);
      print_float_val(psd->max_temperature , "Maximum temperature","deg C" ,fp);
    }
    print_float_val(psd->min_heart_rate , "Minimum heart_rate","" ,fp);
    print_float_val(psd->total_anaerobic_training_effect, "Total anaerobic training effect", "", fp);
    fprintf( fp, "%-30s", "End time");
    fprintf( fp, "%3s", " = ");
    fprintf( fp, "%s", asctime(gmtime(&psd->timestamp)));
    fclose (fp);
  }
}


//
// Plot routines.
//

/* Set the view limits to the data extents. */
void reset_view_limits() {
  if (pd == NULL) {
    return;
  }
  pd->xvmax = pd->xmax;
  pd->yvmin = pd->ymin;
  pd->yvmax = pd->ymax;
  pd->xvmin = pd->xmin;
}

/* Set zoom back to zero. */
void reset_zoom() {
  if (pd == NULL) {
    return;
  }
  pd->zm_startx = 0;
  pd->zm_starty = 0;
  pd->zm_endx = 0;
  pd->zm_endy = 0;
}

/* Smooth the data via a 5 element Savitzky-Golay filter (destructively).
 * Ref: https://en.wikipedia.org/wiki/Savitzky%E2%80%93Golay_filter */
void sg_smooth(PlotData *pdest) {
  /* Set up an array with 4 extra elements to handle the start and
   * end of the series. */
  PLFLT *smooth_arr = NULL;
  int np = pdest->num_pts;
  smooth_arr = (PLFLT *)malloc((np + 4) * sizeof(PLFLT));
  smooth_arr[0] = pdest->y[2];
  smooth_arr[1] = pdest->y[1];
  smooth_arr[np + 1] = pdest->y[np - 1];
  smooth_arr[np + 2] = pdest->y[np - 2];
  for (int i = 0; i < np; i++) {
    smooth_arr[i + 2] = pdest->y[i];
  }
  for (int i = 0; i < pdest->num_pts; i++) {
    pdest->y[i] = 1.0 / 35.0 *
                  ((-3.0 * smooth_arr[i]) + (12.0 * smooth_arr[i + 1]) +
                   (17.0 * smooth_arr[i + 2]) + (12.0 * smooth_arr[i + 3]) +
                   (-3.0 * smooth_arr[i + 4]));
  }
  free(smooth_arr);
}
/*  This routine is where the bulk of the session report
 *  initialization occurs.
 *
 *  We take the raw values from the fit file conversion
 *  routines and convert them to display-appropriate values based
 *  on the selected unit system.
 *
 */

void init_session(SessionData* psd,
                 time_t sess_timestamp ,
                 time_t sess_start_time ,
                 float sess_start_position_lat ,
                 float sess_start_position_long ,
                 float sess_total_elapsed_time , 
                 float sess_total_timer_time ,
                 float sess_total_distance ,
                 float sess_nec_lat ,
                 float sess_nec_long ,
                 float sess_swc_lat ,
                 float sess_swc_long ,
                 float sess_total_work ,
                 float sess_total_moving_time ,
                 float sess_avg_lap_time ,
                 float sess_total_calories ,
                 float sess_avg_speed ,
                 float sess_max_speed ,
                 float sess_total_ascent ,
                 float sess_total_descent ,
                 float sess_avg_altitude ,
                 float sess_max_altitude ,
                 float sess_min_altitude ,
                 float sess_max_heart_rate ,
                 float sess_avg_heart_rate ,
                 float sess_max_cadence ,
                 float sess_avg_cadence ,
                 float sess_avg_temperature ,
                 float sess_max_temperature ,
                 float sess_min_heart_rate ,
                 float sess_total_anaerobic_training_effect
                 ) 
{
    psd->timestamp = sess_timestamp ;
    psd->start_time = sess_start_time ;
    psd->start_position_lat = sess_start_position_lat ;
    psd->start_position_long = sess_start_position_long ;
    psd->total_elapsed_time = sess_total_elapsed_time ; 
    psd->total_timer_time = sess_total_timer_time ;
    if (psd->units == English) {
      psd->total_distance = sess_total_distance * 0.00062137119; // meters to miles
    } else {
      psd->total_distance = sess_total_distance * 0.001; //meters to kilometers
    }
    psd->nec_lat = sess_nec_lat ;
    psd->nec_long = sess_nec_long ;
    psd->swc_lat = sess_swc_lat ;
    psd->swc_long = sess_swc_long ;
    psd->total_work = sess_total_work / 1000.0 ; //J to kJ

    psd->total_moving_time = sess_total_moving_time ;
    psd->avg_lap_time = sess_avg_lap_time ;
    psd->total_calories = sess_total_calories ;

    if (psd->units == English) {
      psd->avg_speed = sess_avg_speed * 2.2369363; // meters/s to miles/hr
      psd->max_speed = sess_max_speed * 2.2369363; // meters/s to miles/hr
    } else {
      psd->avg_speed = sess_avg_speed * 3.6; //meters/s to kilometers/hr
      psd->max_speed = sess_max_speed * 3.6; //meters/s to kilometers/hr
    }
    
    if (psd->units == English) {
      psd->total_ascent = sess_total_ascent * 3.2808399; // meters to feet
      psd->total_descent = sess_total_descent * 3.2808399; // meters to feet
      psd->avg_altitude = sess_avg_altitude * 3.2808399; // meters to feet
      psd->max_altitude = sess_max_altitude * 3.2808399; // meters to feet
      psd->min_altitude = sess_min_altitude * 3.2808399; // meters to feet
    } else {
      psd->total_ascent = sess_total_ascent * 1.0; //meters to meters
      psd->total_descent = sess_total_descent * 1.0; //meters to meters
      psd->avg_altitude = sess_avg_altitude * 1.0; //meters to meters
      psd->max_altitude = sess_max_altitude * 1.0; //meters to meters
      psd->min_altitude = sess_min_altitude * 1.0; //meters to meters
    }

    psd->max_heart_rate = sess_max_heart_rate ;
    psd->avg_heart_rate = sess_avg_heart_rate ;
    psd->max_cadence = sess_max_cadence ;
    psd->avg_cadence = sess_avg_cadence ;

    if (psd->units == English) {
      psd->avg_temperature = 1.8 * sess_avg_temperature + 32.0;
      psd->max_temperature = 1.8 * sess_max_temperature + 32.0;
    } else {
      psd->avg_temperature = sess_avg_temperature * 1.0;
      psd->max_temperature = sess_max_temperature * 1.0 ;
    }
    
    psd->min_heart_rate = sess_min_heart_rate ;
    psd->total_anaerobic_training_effect = sess_total_anaerobic_training_effect;
}



/*  This routine is where the bulk of the plot initialization
 *  occurs.
 *
 *  We take the raw values from the fit file conversion
 *  routines and convert them to display-appropriate values based
 *  on the selected unit system as well as seting labels and range
 *  limits to initial values.
 *
 */
void init_plots(enum PlotType ptype, int num_recs, float x_raw[NSIZE],
                float y_raw[NSIZE], float lat_raw[NSIZE], float lng_raw[NSIZE],
                time_t time_stamp[NSIZE]) {
  PlotData *pdest;
  float x_cnv, y_cnv;
  /* Store to the correct global variable. */
  switch (ptype) {
  case PacePlot:
    pdest = ppace;
    break;
  case CadencePlot:
    pdest = pcadence;
    break;
  case HeartRatePlot:
    pdest = pheart;
    break;
  case AltitudePlot:
    pdest = paltitude;
    break;
  case LapPlot:
    pdest = plap;
    break;
  }
  /* Housekeeping. Release any memory previously allocated before
   * reinitializing.
   */
  if (pdest->x != NULL) {
    free(pdest->x);
  }
  if (pdest->y != NULL) {
    free(pdest->y);
  }
  if (pdest->lat != NULL) {
    free(pdest->lat);
  }
  if (pdest->lng != NULL) {
    free(pdest->lng);
  }
  /* How big are we? */
  pdest->num_pts = num_recs;
  /* Allocate new memory for the converted values. */
  pdest->lat = (PLFLT *)malloc(pdest->num_pts * sizeof(PLFLT));
  pdest->lng = (PLFLT *)malloc(pdest->num_pts * sizeof(PLFLT));
  pdest->x = (PLFLT *)malloc(pdest->num_pts * sizeof(PLFLT));
  pdest->y = (PLFLT *)malloc(pdest->num_pts * sizeof(PLFLT));
  /* Assign the conversion factors by plot type. */
  switch (pdest->ptype) {
  case PacePlot:
    if (pdest->units == English) {
      x_cnv = 0.00062137119; // meters to miles
      y_cnv = 0.037282272;   // meters per sec to miles per min
    } else {
      x_cnv = 0.001; // meters to kilometers
      y_cnv = 0.06;  // meters per sec to kilometers per min
    }
    break;
  case CadencePlot:
    if (pdest->units == English) {
      x_cnv = 0.00062137119; // meters to miles
      y_cnv = 1.0;           // steps to steps
    } else {
      x_cnv = 0.001; // meters to kilometers
      y_cnv = 1.0;   // steps to steps
    }
    break;
  case HeartRatePlot:
    if (pdest->units == English) {
      x_cnv = 0.00062137119; // meters to miles
      y_cnv = 1.0;           // bpm to bpm
    } else {
      x_cnv = 0.001; // meters to kilometers
      y_cnv = 1.0;   // bpm to bpm
    }
    break;
  case AltitudePlot:
    if (pdest->units == English) {
      x_cnv = 0.00062137119; // meters to miles
      y_cnv = 3.28084;       // meters to feet
    } else {
      x_cnv = 0.001; // meters to kilometers
      y_cnv = 1.0;   // meters to meters
    }
   break;
  case LapPlot:
    if (pdest->units == English) {
      x_cnv = 0.00062137119; // meters to miles
      y_cnv = 1.0/60.0;       // seconds/lap to minutes/lap
    } else {
      x_cnv = 0.001; // meters to kilometers
      y_cnv = 1.0/60.0;       // seconds/lap to minutes/lap
    }
  }
  /* Convert (or in the case of positions/time, copy) the raw values to the
   * displayed values.
   */
  for (int i = 0; i < pdest->num_pts; i++) {
    pdest->x[i] = (PLFLT)x_raw[i] * x_cnv;
    pdest->y[i] = (PLFLT)y_raw[i] * y_cnv;
  }
  for (int i = 0; i < pdest->num_pts; i++) {
    pdest->lat[i] = (PLFLT)lat_raw[i];
    pdest->lng[i] = (PLFLT)lng_raw[i];
  }
  /* Smooth the Y values. */
  // TODO Needs more testing.  I think there are some bugs.
  gboolean filter = FALSE;
  if (filter) {
    sg_smooth(pdest);
  }
  /* Set start time in UTC (for title) */
  if (pdest->num_pts > 0) {
    struct tm *ptm = gmtime(&time_stamp[0]);
    pdest->start_time = asctime(ptm);
  }
  /* Find plot data min, max */
  pdest->xmin = FLT_MAX;
  pdest->xmax = -FLT_MAX;
  pdest->ymin = FLT_MAX;
  pdest->ymax = -FLT_MAX;
  for (int i = 0; i < pdest->num_pts; i++) {
    if (pdest->x[i] < pdest->xmin) {
      pdest->xmin = pdest->x[i];
    }
    if (pdest->x[i] > pdest->xmax) {
      pdest->xmax = pdest->x[i];
    }
    if (pdest->y[i] < pdest->ymin) {
      pdest->ymin = pdest->y[i];
    }
    if (pdest->y[i] > pdest->ymax) {
      pdest->ymax = pdest->y[i];
    }
  }
  /* Set axis labels based on plot type and unit system. */
  switch (pdest->ptype) {
  case PacePlot:
    if (pdest->units == English) {
      pdest->xaxislabel = "Distance(miles)";
      pdest->yaxislabel = "Pace(min/mile)";
    } else {
      pdest->xaxislabel = "Distance(km)";
      pdest->yaxislabel = "Pace(min/km)";
    }
    break;
  case CadencePlot:
    if (pdest->units == English) {
      pdest->xaxislabel = "Distance(miles)";
      pdest->yaxislabel = "Cadence(steps/min)";
    } else {
      pdest->xaxislabel = "Distance(km)";
      pdest->yaxislabel = "Cadence(steps/min)";
    }
    break;
  case AltitudePlot:
    if (pdest->units == English) {
      pdest->xaxislabel = "Distance(miles)";
      pdest->yaxislabel = "Altitude (feet)";
    } else {
      pdest->xaxislabel = "Distance(km)";
      pdest->yaxislabel = "Altitude(meters)";
    }
    break;
  case HeartRatePlot:
    if (pdest->units == English) {
      pdest->xaxislabel = "Distance(miles)";
      pdest->yaxislabel = "Heart rate (bpm)";
    } else {
      pdest->xaxislabel = "Distance(km)";
      pdest->yaxislabel = "Heart rate (bpm)";
    }
    break;
  case LapPlot:
    if (pdest->units == English) {
      pdest->xaxislabel = "Lap";
      pdest->yaxislabel = "Elapsed Split Time(min)";
    } else {
      pdest->xaxislabel = "Lap";
      pdest->yaxislabel = "Elapsed Split Time(min)";
    }
  }
  /* Set the view to the data extents. */
  pdest->xvmax = pdest->xmax;
  pdest->yvmin = pdest->ymin;
  pdest->yvmax = pdest->ymax;
  pdest->xvmin = pdest->xmin;
  pdest->zm_startx = 0;
  pdest->zm_starty = 0;
  pdest->zm_endx = 0;
  pdest->zm_endy = 0;
  return;
}

/* Read file data, convert to display plot structures. */
gboolean init_plot_data() {
  /* Sensor declarations */
  float speed[NSIZE], dist[NSIZE], lat[NSIZE], lng[NSIZE], alt[NSIZE],
      cadence[NSIZE], heart_rate[NSIZE];
  int num_recs = 0;
  float lap_start_lat[LSIZE], lap_start_lng[LSIZE], 
        lap_end_lat[LSIZE], lap_end_lng[LSIZE],
        lap_total_distance[LSIZE], lap_total_calories[LSIZE],
        lap_total_elapsed_time[LSIZE], lap_total_timer_time[LSIZE];
  int lap_num_recs = 0;
  time_t time_stamp[NSIZE], lap_time_stamp[LSIZE];

  time_t sess_timestamp ;
  time_t sess_start_time ;
  float sess_start_position_lat ;
  float sess_start_position_long ;
  float sess_total_elapsed_time ; 
  float sess_total_timer_time ;
  float sess_total_distance ;
  float sess_nec_lat ;
  float sess_nec_long ;
  float sess_swc_lat ;
  float sess_swc_long ;
  float sess_total_work ;
  float sess_total_moving_time ;
  float sess_avg_lap_time ;
  float sess_total_calories ;
  float sess_avg_speed ;
  float sess_max_speed ;
  float sess_total_ascent ;
  float sess_total_descent ;
  float sess_avg_altitude ;
  float sess_max_altitude ;
  float sess_min_altitude ;
  float sess_max_heart_rate ;
  float sess_avg_heart_rate ;
  float sess_max_cadence ;
  float sess_avg_cadence ;
  float sess_avg_temperature ;
  float sess_max_temperature ;
  float sess_min_heart_rate ;
  float sess_total_anaerobic_training_effect;

  /* Unit system first. */
  gchar *user_units = gtk_combo_box_text_get_active_text(cb_Units);
  if (!strcmp(user_units, "Metric")) {
    ppace->units = Metric;
    pcadence->units = Metric;
    pheart->units = Metric;
    paltitude->units = Metric;
    plap->units = Metric;
    psd->units = Metric;
  } else {
    ppace->units = English;
    pcadence->units = English;
    pheart->units = English;
    paltitude->units = English;
    plap->units = English;
    psd->units = English;
  }
  g_free(user_units);
  /* Load data from fit file. */
  int rtnval = get_fit_records(fname, speed, dist, lat, lng, cadence,
                               heart_rate, alt, time_stamp, &num_recs,
                               lap_start_lat, lap_start_lng, 
                               lap_end_lat, lap_end_lng,
                               lap_total_distance, lap_total_calories,
                               lap_total_elapsed_time, lap_total_timer_time,
                               lap_time_stamp, &lap_num_recs,
                               &sess_timestamp ,
                               &sess_start_time ,
                               &sess_start_position_lat ,
                               &sess_start_position_long ,
                               &sess_total_elapsed_time , 
                               &sess_total_timer_time ,
                               &sess_total_distance ,
                               &sess_nec_lat ,
                               &sess_nec_long ,
                               &sess_swc_lat ,
                               &sess_swc_long ,
                               &sess_total_work ,
                               &sess_total_moving_time ,
                               &sess_avg_lap_time ,
                               &sess_total_calories ,
                               &sess_avg_speed ,
                               &sess_max_speed ,
                               &sess_total_ascent ,
                               &sess_total_descent ,
                               &sess_avg_altitude ,
                               &sess_max_altitude ,
                               &sess_min_altitude ,
                               &sess_max_heart_rate ,
                               &sess_avg_heart_rate ,
                               &sess_max_cadence ,
                               &sess_avg_cadence ,
                               &sess_avg_temperature ,
                               &sess_max_temperature ,
                               &sess_min_heart_rate ,
                               &sess_total_anaerobic_training_effect
                               );

  if (rtnval != 100) {
    /* Something blew up. */
    return FALSE;
  } else {
    /* Initialize the display data structures based on the data. */
    init_plots(PacePlot, num_recs, dist, speed, lat, lng, time_stamp);
    init_plots(CadencePlot, num_recs, dist, cadence, lat, lng, time_stamp);
    init_plots(HeartRatePlot, num_recs, dist, heart_rate, lat, lng, time_stamp);
    init_plots(AltitudePlot, num_recs, dist, alt, lat, lng, time_stamp);
    init_plots(LapPlot, lap_num_recs, lap_total_distance, lap_total_elapsed_time, lap_start_lat, lap_start_lng, time_stamp);

    //TESTING
    init_session(psd ,
                 sess_timestamp ,
                 sess_start_time ,
                 sess_start_position_lat ,
                 sess_start_position_long ,
                 sess_total_elapsed_time , 
                 sess_total_timer_time ,
                 sess_total_distance ,
                 sess_nec_lat ,
                 sess_nec_long ,
                 sess_swc_lat ,
                 sess_swc_long ,
                 sess_total_work ,
                 sess_total_moving_time ,
                 sess_avg_lap_time ,
                 sess_total_calories ,
                 sess_avg_speed ,
                 sess_max_speed ,
                 sess_total_ascent ,
                 sess_total_descent ,
                 sess_avg_altitude ,
                 sess_max_altitude ,
                 sess_min_altitude ,
                 sess_max_heart_rate ,
                 sess_avg_heart_rate ,
                 sess_max_cadence ,
                 sess_avg_cadence ,
                 sess_avg_temperature ,
                 sess_max_temperature ,
                 sess_min_heart_rate ,
                 sess_total_anaerobic_training_effect
                 ),

    generate_report(psd);

    return TRUE;
  }
}

/* A custom axis labeling function for a pace plot. */
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

/* A custom axis labeling function for a cadence plot. */
void cadence_plot_labeler(PLINT axis, PLFLT value, char *label, PLINT length,
                          PLPointer label_data) {
  if (axis == PL_Y_AXIS) {
    snprintf(label, (size_t)length, "%3.2f", value);
  }
  if (axis == PL_X_AXIS) {
    snprintf(label, (size_t)length, "%3.2f", value);
  }
}

/* A custom axis labeling function for a heart rate plot. */
void heart_rate_plot_labeler(PLINT axis, PLFLT value, char *label, PLINT length,
                             PLPointer label_data) {
  if (axis == PL_Y_AXIS) {
    snprintf(label, (size_t)length, "%3.0f", value);
  }
  if (axis == PL_X_AXIS) {
    snprintf(label, (size_t)length, "%3.2f", value);
  }
}

/* A custom axis labeling function for an altitude plot. */
void altitude_plot_labeler(PLINT axis, PLFLT value, char *label, PLINT length,
                           PLPointer label_data) {
  if (axis == PL_Y_AXIS) {
    snprintf(label, (size_t)length, "%3.0f", value);
  }
  if (axis == PL_X_AXIS) {
    snprintf(label, (size_t)length, "%3.2f", value);
  }
}

/* Draw an xy plot. */

void draw_xy(int width, int height) {
  float ch_size = 4.0; // mm
  float scf = 1.0;     // dimensionless
  PLFLT n_xmin, n_xmax, n_ymin, n_ymax;
  PLFLT x_hair = 0;
  PLFLT hairline_x[2], hairline_y[2];
  if ((pd->x != NULL) && (pd->y != NULL)) {
    /* Do your drawing. */
    /* Color */
    plscol0a(1, 65, 209, 65, 0.25);   // light green for selector
    plscol0a(15, 128, 128, 128, 0.9); // light gray for background
    plscol0a(2, pd->linecolor[0], pd->linecolor[1], pd->linecolor[2], 0.8);
    plwind(pd->xvmin, pd->xvmax, pd->yvmin, pd->yvmax);
    /* Adjust character size. */
    plschr(ch_size, scf);
    plcol0(15);
    /* Setup a custom axis tick label function. */
    switch (pd->ptype) {
    case PacePlot:
      plslabelfunc(pace_plot_labeler, NULL);
      break;
    case CadencePlot:
      plslabelfunc(cadence_plot_labeler, NULL);
      break;
    case AltitudePlot:
      plslabelfunc(altitude_plot_labeler, NULL);
      break;
    case HeartRatePlot:
      plslabelfunc(heart_rate_plot_labeler, NULL);
      break;
    case LapPlot:
      break;
    }
    /* Create a labelled box to hold the plot using custom x,y labels. */
    // We want finer control here, so we ignore the convenience function.
    char *xopt = "bnost";
    char *yopt = "bgnost";
    // TODO valgrind reports mem lost on below line...
    plaxes(pd->xvmin, pd->yvmin, xopt, 0, 0, yopt, 0, 0);
    /* Setup axis labels and titles. */
    pllab(pd->xaxislabel, pd->yaxislabel, pd->start_time);
    /* Set line color to the second pallette color. */
    plcol0(2);
    /* Plot the data that was loaded. */
    plwidth(2);
    plline(pd->num_pts, pd->x, pd->y);
    plwidth(0);
    /* Plot symbols for individual data points. */
    // TODO valgrind reports mem lost on below line...
    // plstring(pd->num_pts, pd->x, pd->y, pd->symbol);
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
    /* Add a hairline */
    plcol0(15);
    /* If we are between the view limits, draw a line from the
     * current index on the x scale from the bottom to the top
     * of the view. */
    x_hair = pd->x[currIdx];
    if ((x_hair >= pd->xvmin) && (x_hair <= pd->xvmax)) {
      hairline_x[0] = x_hair;
      hairline_x[1] = x_hair;
      hairline_y[0] = pd->yvmin;
      hairline_y[1] = pd->yvmax;
      pllsty(2);
      plline(2, hairline_x, hairline_y);
      pllsty(1);
    }
  }
}

/* Draw a filled box. */
void plfbox( PLFLT x0, PLFLT y0, PLINT color )
{
    PLFLT x[4], y[4];

    x[0] = x0;
    y[0] = 0.;
    x[1] = x0;
    y[1] = y0;
    x[2] = x0 + 1.;
    y[2] = y0;
    x[3] = x0 + 1.;
    y[3] = 0.;
    plcol0( color );
    plfill( 4, x, y );
    plcol0( 15 );
    pllsty( 1 );
    plline( 4, x, y );
}

/* Draw a bar chart */
void draw_bar(int width, int height) {
  char         string[8];
  plwind( 0.0, (float)plap->num_pts - 1.0, plap->ymin, plap->ymax);
  plscol0a(15, 128, 128, 128, 0.9); // light gray for background
  plcol0( 15 );
  plbox( "bc", 1.0, 0, "bcnv", 1.0, 0 );
  pllab( plap->xaxislabel, plap->yaxislabel, plap->start_time );
  // Normal color.
  plscol0a(2, plap->linecolor[0], plap->linecolor[1], plap->linecolor[2], 0.3);
  // Highlight (progress) color.
  plscol0a(3, plap->linecolor[0], plap->linecolor[1], plap->linecolor[2], 0.5);
  float tot_dist = 0.0;
  for (int i = 0; i < plap->num_pts-1; i++ )
  {
      tot_dist = plap->x[i] + tot_dist;
      plcol0( 15 );
      plpsty( 0 );
      if (ppace->x[currIdx] > tot_dist) { 
        plfbox(i, plap->y[i], 3);
      }
      else {
        plfbox(i, plap->y[i], 2);
      }
      /* x axis */
      sprintf( string, "%1.0f", (float)i + 1.0);
      float bar_width = 1.0 / ( (float)(plap->num_pts) - 1.0);
      float xposn =  (i + 0.5) * bar_width;
      plmtex( "b", 1.0, xposn, 0.5, string );
      /* bar label */
      double secs, mins;
      secs = modf(plap->y[i], &mins);
      secs *= 60.0;
      snprintf(string, 8, "%2.0f:%02.0f", mins, secs);
      plptex( (float)i + 0.5, (1.1 * plap->ymin ), 0.0, 90.0, 0.0, string );
  }
}

/* Convenience function to find active radio button. */
enum PlotType checkRadioButtons()
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(rb_Pace))==TRUE)
    return PacePlot ;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(rb_Cadence))==TRUE)
    return CadencePlot ;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(rb_HeartRate))==TRUE)
    return HeartRatePlot ;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(rb_Altitude))==TRUE)
    return AltitudePlot;
  return LapPlot;
}

/* Drawing area callback.
 *
 * The GUI definition wraps a GTKDrawing area inside a GTK widget.
 * This routine recasts the widget as a GDKWindow which is then used
 * with a device-independent vector-graphics based API (Cairo) and a
 * plotting library API (PLPlot) that supports Cairo to generate the
 * user's plots.
 */
gboolean on_da_draw(GtkWidget *widget, GdkEventExpose *event, gpointer *data) {
  PLFLT xdpi, ydpi;
  PLINT width, height, xoff, yoff;
  /* Can't plot uninitialized. */
  if ((pd == NULL) || (plap == NULL)) {
    return TRUE;
  }
  /* "Convert" the G*t*kWidget to G*d*kWindow (no, it's not a GtkWindow!) */
  GdkWindow *window = gtk_widget_get_window(widget);
  cairo_region_t *cairoRegion = cairo_region_create();
  GdkDrawingContext *drawingContext;
  drawingContext = gdk_window_begin_draw_frame(window, cairoRegion);
  /* Say: "I want to start drawing". */
  cairo_t *cr = gdk_drawing_context_get_cairo_context(drawingContext);
  /* Initialize plplot using the external cairo backend. */
  plsdev("extcairo");
  /* Device attributes */
  plinit();
  pl_cmd(PLESC_DEVINIT, cr);
  /* Retrieve "page" height, width in pixels.  Note this is different
   *  from the widget size.
   */
  plgpage(&xdpi, &ydpi, &width, &height, &xoff, &yoff);
  /* Viewport and window */
  pladv(0);
  plvasp((float)height / (float)width);

  /* Draw an xy plot or a bar chart. */
  switch (checkRadioButtons()) {
  case PacePlot:
    draw_xy(width, height);
    break;
  case CadencePlot:
    draw_xy(width, height);
    break;
  case HeartRatePlot:
    draw_xy(width, height);
    break;
  case AltitudePlot:
    draw_xy(width, height);
    break;
  case LapPlot:
    draw_bar(width, height);
    break;
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
 *
 * The plot view bounds (xvmin, xvmax, yvmin, yvmax) and the plot
 * zoom bounds (zmxmin, zmxmax, zmymin, zmymax) are calculated
 * by the draw routine.
 *
 */
void gui_to_world(struct PlotData *pd, GdkEventButton *event,
                  enum ZoomState state) {
  if (pd == NULL) {
    return;
  }
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

/* Convenience routine to change the cursor style. */
void change_cursor(GtkWidget *widget, const gchar *name) {
  GdkDisplay *display = gtk_widget_get_display(widget);
  GdkCursor *cursor;
  cursor = gdk_cursor_new_from_name(display, name);
  gdk_window_set_cursor(gtk_widget_get_window(widget), cursor);
  // Release the (memory) reference on the cursor.
  g_object_unref(cursor);
}

/* Handle mouse button press. */
gboolean on_button_press(GtkWidget *widget, GdkEvent *event) {
  guint buttonnum;
  if (pd == NULL) {
    return FALSE;
  }
  gdk_event_get_button(event, &buttonnum);
  if (buttonnum == 3) {
    change_cursor(widget, "crosshair");
  }
  if (buttonnum == 1) {
    change_cursor(widget, "hand1");
  }
  /* Set user selected starting x, y in world coordinates. */
  gui_to_world(pd, (GdkEventButton *)event, Press);
  return TRUE;
}

/* Handle mouse button release. */
gboolean on_button_release(GtkWidget *widget, GdkEvent *event) {
  guint buttonnum;
  if (pd == NULL) {
    return FALSE;
  }
  change_cursor(widget, "default");
  gdk_event_get_button(event, &buttonnum);
  /* Zoom out if right mouse button release. */
  if (buttonnum == 2) {
    reset_view_limits();
    gtk_widget_queue_draw(GTK_WIDGET(da));
    reset_zoom();
    return TRUE;
  }
  /* Zoom in if left mouse button release. */
  /* Set user selected ending x, y in world coordinates. */
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
    reset_zoom();
  }
  return TRUE;
}

/* Handle mouse motion event by drawing a filled
 * polygon.
 */
gboolean on_motion_notify(GtkWidget *widget, GdkEventButton *event) {

  if (pd == NULL) {
    return FALSE;
  }
  if (event->state & GDK_BUTTON3_MASK) {
    gui_to_world(pd, event, Move);
    gtk_widget_queue_draw(GTK_WIDGET(da));
  }
  return TRUE;
}

//
// Map Stuff
//

/* Instantiate a (global) instance of a champlain map widget and
 * its associated view.  Add it to a GTKFrame named viewport.
 * The function assumes clutter has already been initialized.
 */
static void init_map() {
  champlain_widget = gtk_champlain_embed_new();
  c_view = gtk_champlain_embed_get_view(GTK_CHAMPLAIN_EMBED(champlain_widget));
  clutter_actor_set_reactive(CLUTTER_ACTOR(c_view), TRUE);
  g_object_set(G_OBJECT(c_view), "kinetic-mode", TRUE, "zoom-level", 14, NULL);
  gtk_widget_set_size_request(champlain_widget, 640, 480);
  /* Add the global widget to the global GTKFrame named viewport */
  gtk_container_add(GTK_CONTAINER(viewport), champlain_widget);
}

/* Convenience routine to add a latitude, longitude to a path layer. */
static void append_point(ChamplainPathLayer *layer, gdouble lon, gdouble lat) {
  ChamplainCoordinate *coord;
  coord = champlain_coordinate_new_full(lon, lat);
  // TODO This line throwing assertion errors.  Borrowed from example so why?
  champlain_path_layer_add_node(layer, CHAMPLAIN_LOCATION(coord));
}

/* Convenience routine to add a marker to the marker layer. */
static void add_marker(ChamplainMarkerLayer *my_marker_layer, gdouble lng,
                       gdouble lat, const ClutterColor *color) {
  ClutterActor *my_marker = champlain_point_new_full(12, color);
  champlain_location_set_location(CHAMPLAIN_LOCATION(my_marker), lat, lng);
  champlain_marker_layer_add_marker(my_marker_layer,
                                    CHAMPLAIN_MARKER(my_marker));
}

/* Convenience routine to move marker. */
static void move_marker(ChamplainMarkerLayer *my_marker_layer,
                        ChamplainMarker *my_marker, gdouble new_lng,
                        gdouble new_lat, const ClutterColor *color) {
  if (my_marker != NULL) {
    champlain_location_set_location(CHAMPLAIN_LOCATION(my_marker), new_lat,
                                    new_lng);
  }
}

/* Update the map. */
static void create_map() {
  /* Define colors for start, end markers.*/
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
    /* Add current position marker */
    add_marker(c_marker_layer, pd->lng[currIdx], pd->lat[currIdx], &my_blue);
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
// GTK GUI Stuff
//

/* Default to the pace chart. */
gboolean default_chart() {
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb_Pace), TRUE);
  return TRUE;
}

/* User has changed unit system. */
void on_cb_units_changed(GtkComboBox *cb_Units) {
  init_plot_data(); // got to reconvert the raw data
  g_signal_emit_by_name(sc_IdxPct, "value-changed");
  gtk_widget_queue_draw(GTK_WIDGET(da));
}

/* User has selected Pace Graph. */
void on_rb_pace(GtkToggleButton *togglebutton) {
  pd = ppace;
  gtk_widget_queue_draw(GTK_WIDGET(da));
  g_signal_emit_by_name(sc_IdxPct, "value-changed");
}

/* User has selected Cadence Graph. */
void on_rb_cadence(GtkToggleButton *togglebutton) {
  pd = pcadence;
  gtk_widget_queue_draw(GTK_WIDGET(da));
  g_signal_emit_by_name(sc_IdxPct, "value-changed");
}

/* User has selected Heartrate Graph. */
void on_rb_heartrate(GtkToggleButton *togglebutton) {
  pd = pheart;
  gtk_widget_queue_draw(GTK_WIDGET(da));
  g_signal_emit_by_name(sc_IdxPct, "value-changed");
}

/* User has selected Altitude Graph. */
void on_rb_altitude(GtkToggleButton *togglebutton) {
  pd = paltitude;
  gtk_widget_queue_draw(GTK_WIDGET(da));
  g_signal_emit_by_name(sc_IdxPct, "value-changed");
}

/* User has selected Splits Graph. */
void on_rb_splits(GtkToggleButton *togglebutton) {
  gtk_widget_queue_draw(GTK_WIDGET(da));
  g_signal_emit_by_name(sc_IdxPct, "value-changed");
}


/* User has pressed open a new file. */
void on_btnFileOpen_file_set(GtkFileChooserButton *btnFileOpen) {
  /* fname is a global */
  fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(btnFileOpen));
  init_plot_data();
  if (pd == NULL) {
    pd = ppace;
  }
  gtk_widget_queue_draw(GTK_WIDGET(da));
  create_map();
}

//
// Slider/Index routines.
//

void on_update_index(GtkScale *widget, gpointer *data) {
  GtkAdjustment *adj;
  GList *marker_list, *position_marker;
  adj = gtk_range_get_adjustment((GtkRange *)widget);
  gdouble val = gtk_adjustment_get_value(adj);
  // Slider from zero to 100 - normalized.  Calculate portion of activity.
  currIdx = (int)floor(val / 100.0 * (float)ppace->num_pts);
  gtk_widget_queue_draw(GTK_WIDGET(da));
  // Redraw the position marker on the map.
  if ((c_marker_layer != NULL) && (c_view != NULL)) {
    marker_list = champlain_marker_layer_get_markers(c_marker_layer);
    /*Danger! This depends on the order markers are added in create_map!!!
      Last in, first out in linked list. If more are added you might
      need to walk the list (marker_list->next).
     */
    position_marker = marker_list; // position marker is the zero-eth element
    if (position_marker != NULL) {
      move_marker(c_marker_layer, position_marker->data, pd->lng[currIdx],
                  pd->lat[currIdx], &my_blue);
    }
    g_signal_emit_by_name(c_view, "layer-relocated");

    char yval[15];
    char xval[15];

    switch (pd->ptype) {
    case PacePlot:
      pace_plot_labeler(PL_Y_AXIS, pd->y[currIdx], yval, 15, NULL);
      pace_plot_labeler(PL_X_AXIS, pd->x[currIdx], xval, 15, NULL);
      break;
    case CadencePlot:
      cadence_plot_labeler(PL_Y_AXIS, pd->y[currIdx], yval, 15, NULL);
      cadence_plot_labeler(PL_X_AXIS, pd->x[currIdx], xval, 15, NULL);
      break;
    case AltitudePlot:
      altitude_plot_labeler(PL_Y_AXIS, pd->y[currIdx], yval, 15, NULL);
      altitude_plot_labeler(PL_X_AXIS, pd->x[currIdx], xval, 15, NULL);
      break;
    case HeartRatePlot:
      heart_rate_plot_labeler(PL_Y_AXIS, pd->y[currIdx], yval, 15, NULL);
      heart_rate_plot_labeler(PL_X_AXIS, pd->x[currIdx], xval, 15, NULL);
      break;
    case LapPlot:
      break;
    }
    char *curr_vals;
    curr_vals = malloc(strlen(pd->xaxislabel) + 2 + strlen(xval) + 2 +
                       strlen(pd->yaxislabel) + 2 + strlen(yval) + 1);
    strcpy(curr_vals, pd->xaxislabel);
    strcat(curr_vals, "= ");
    strcat(curr_vals, xval);
    strcat(curr_vals, ", ");
    strcat(curr_vals, pd->yaxislabel);
    strcat(curr_vals, "= ");
    strcat(curr_vals, yval);
    gtk_label_set_text(lbl_val, curr_vals);
    free(curr_vals);
  }
}

//
// Main
//

/*
 * This is the program entry point.  The builder reads an XML file (generated
 * by the Glade application and instantiate the associated (global) objects.
 *
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
  rb_Splits =
      GTK_RADIO_BUTTON(gtk_builder_get_object(builder, "rb_Splits"));
  btnFileOpen =
      GTK_FILE_CHOOSER_BUTTON(gtk_builder_get_object(builder, "btnFileOpen"));
  btn_Zoom_In = GTK_BUTTON(gtk_builder_get_object(builder, "btn_Zoom_In"));
  btn_Zoom_Out = GTK_BUTTON(gtk_builder_get_object(builder, "btn_Zoom_Out"));
  cb_Units = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "cb_Units"));
  sc_IdxPct = GTK_SCALE(gtk_builder_get_object(builder, "sc_IdxPct"));
  lbl_val = GTK_LABEL(gtk_builder_get_object(builder, "lbl_val"));

  /* Select a default chart to start. */
  default_chart();

  /* Initialize champlain map and add it to a frame after initializing clutter.
   */
  if (gtk_clutter_init(&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;
  init_map();
  gtk_widget_show_all(window);

  /* Signals and events */
  gtk_builder_connect_signals(builder, NULL);
  gtk_widget_add_events(GTK_WIDGET(da), GDK_BUTTON_PRESS_MASK);
  gtk_widget_add_events(GTK_WIDGET(da), GDK_BUTTON_RELEASE_MASK);
  gtk_widget_add_events(GTK_WIDGET(da), GDK_POINTER_MOTION_MASK);
  g_signal_connect(GTK_DRAWING_AREA(da), "button-press-event",
                   G_CALLBACK(on_button_press), NULL);
  g_signal_connect(GTK_DRAWING_AREA(da), "button-release-event",
                   G_CALLBACK(on_button_release), NULL);
  g_signal_connect(GTK_DRAWING_AREA(da), "motion-notify-event",
                   G_CALLBACK(on_motion_notify), NULL);
  g_signal_connect(GTK_DRAWING_AREA(da), "draw", G_CALLBACK(on_da_draw), NULL);
  g_signal_connect(GTK_RADIO_BUTTON(rb_Pace), "toggled", G_CALLBACK(on_rb_pace),
                   NULL);
  g_signal_connect(GTK_RADIO_BUTTON(rb_Cadence), "toggled",
                   G_CALLBACK(on_rb_cadence), NULL);
  g_signal_connect(GTK_RADIO_BUTTON(rb_HeartRate), "toggled",
                   G_CALLBACK(on_rb_heartrate), NULL);
  g_signal_connect(GTK_RADIO_BUTTON(rb_Altitude), "toggled",
                   G_CALLBACK(on_rb_altitude), NULL);
  g_signal_connect(GTK_RADIO_BUTTON(rb_Splits), "toggled",
                   G_CALLBACK(on_rb_splits), NULL);
  g_signal_connect(GTK_BUTTON(btn_Zoom_In), "clicked", G_CALLBACK(zoom_in),
                   c_view);
  g_signal_connect(GTK_BUTTON(btn_Zoom_Out), "clicked", G_CALLBACK(zoom_out),
                   c_view);
  g_signal_connect(GTK_COMBO_BOX_TEXT(cb_Units), "changed",
                   G_CALLBACK(on_cb_units_changed), NULL);
  g_signal_connect(GTK_FILE_CHOOSER(btnFileOpen), "file-set",
                   G_CALLBACK(on_btnFileOpen_file_set), NULL);
  g_signal_connect(GTK_SCALE(sc_IdxPct), "value-changed",
                   G_CALLBACK(on_update_index), NULL);

  /* Release the builder memory. */
  g_object_unref(builder);

  gtk_widget_show(window); // Required to display champlain widget.
  gtk_main();

  return 0;
}

/* Release the allocated memory. */
void destroy_plots() {
  if (ppace->x != NULL) {
    free(ppace->x);
  }
  if (ppace->y != NULL) {
    free(ppace->y);
  }
  if (ppace->lat != NULL) {
    free(ppace->lat);
  }
  if (ppace->lng != NULL) {
    free(ppace->lng);
  }
  if (pcadence->x != NULL) {
    free(pcadence->x);
  }
  if (pcadence->y != NULL) {
    free(pcadence->y);
  }
  if (pcadence->lat != NULL) {
    free(pcadence->lat);
  }
  if (pcadence->lng != NULL) {
    free(pcadence->lng);
  }
  if (pheart->x != NULL) {
    free(pheart->x);
  }
  if (pheart->y != NULL) {
    free(pheart->y);
  }
  if (pheart->lat != NULL) {
    free(pheart->lat);
  }
  if (pheart->lng != NULL) {
    free(pheart->lng);
  }
  if (paltitude->x != NULL) {
    free(paltitude->x);
  }
  if (paltitude->y != NULL) {
    free(paltitude->y);
  }
  if (paltitude->lat != NULL) {
    free(paltitude->lat);
  }
  if (paltitude->lng != NULL) {
    free(paltitude->lng);
  }
}

/* Call when the window is closed.*/
void on_window1_destroy() {
  destroy_plots();
  gtk_main_quit();
}
