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

#include <clutter-gtk/clutter-gtk.h>
#include <float.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <math.h>
#include <string.h>
/*
 * Rsvglib
 */
#include <librsvg/rsvg.h>

/*
 * PLPlot
 */
#include <cairo-ps.h>
#include <cairo.h>
#include <plplot.h>

/*
 *  Map
 */
#include "osm-gps-map.h"

/*
 * Fit file decoding
 */
#include "fitwrapper.h"

//
// Declarations section
//

// Maximum readable records from a fit file.
// 2880 is large enough for 4 hour marathon at 5 sec intervals
#define NSIZE 2880
// Maximum readable laps from a fit file.
#define LSIZE 400

// the result structure defined by fitwrapper.h
struct parse_fit_file_return result;

enum ZoomState { Press = 0, Move = 1, Release = 2 };
enum UnitSystem { Metric = 1, English = 0 };
enum PlotType {
  PacePlot = 1,
  CadencePlot = 2,
  HeartRatePlot = 3,
  AltitudePlot = 4,
  LapPlot = 5
};

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
  char *timestamp;
  char *start_time;
  float start_position_lat;
  float start_position_long;
  float total_elapsed_time;
  float total_timer_time;
  float total_distance;
  float nec_lat;
  float nec_long;
  float swc_lat;
  float swc_long;
  float total_work;
  float total_moving_time;
  float avg_lap_time;
  float total_calories;
  float avg_speed;
  float max_speed;
  float total_ascent;
  float total_descent;
  float avg_altitude;
  float max_altitude;
  float min_altitude;
  float max_heart_rate;
  float avg_heart_rate;
  float max_cadence;
  float avg_cadence;
  float avg_temperature;
  float max_temperature;
  float min_heart_rate;
  float total_anaerobic_training_effect;
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
                     .start_time = NULL};
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
                        .start_time = NULL};
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
                          .start_time = NULL};
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
                         .start_time = NULL};
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
                    .start_time = NULL};

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
struct PlotData *pd; // for xy-plots
/* The pointer for the session summary. */
struct SessionData *psd = &sess;

/* Declarations for the GUI widgets. */
GtkTextBuffer *textbuffer1;
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

/* Declarations for OsmGps maps. */
OsmGpsMap *map;
OsmGpsMapTrack *routeTrack;
static GdkPixbuf *starImage = NULL;
OsmGpsMapImage *startTrackMarker = NULL;
OsmGpsMapImage *endTrackMarker = NULL;
OsmGpsMapImage *posnTrackMarker = NULL;

/* Current index */
int currIdx = 0;
//
// Convenience functions.
//
void printfloat(float x, char *name) { printf("%s = %f \n", name, x); }
//
// Summary routines.
//

/* Convenience routine to print a floating point line. */
void print_float_val(float val, char *plabel, char *punit, FILE *fp) {
  if ((plabel != NULL) && (punit != NULL) && (val < FLT_MAX - 1.0)) {
    fprintf(fp, "%-30s", plabel);
    fprintf(fp, "%3s", " = ");
    fprintf(fp, "%10.2f", val);
    fprintf(fp, "%3s", " ");
    fprintf(fp, "%-20s", punit);
    fprintf(fp, "\n");
  }
}

/* Convenience routine to print a formatted timer value. */
void print_timer_val(float timer, char *plabel, FILE *fp) {
  if (timer < FLT_MAX - 1.0) {
    double hours, secs, mins, extra;
    extra = modf(timer / 3600.0, &hours);
    secs = modf(extra * 60.0, &mins);
    secs *= 60.0;
    fprintf(fp, "%-30s", plabel);
    fprintf(fp, "%3s", " = ");
    fprintf(fp, "%4.0f:%02.0f:%02.0f", hours, mins, secs);
    fprintf(fp, "\n");
  }
}

/* Generate the summary report */
void create_summary(FILE *fp) {
  if ((fp != NULL) && (psd != NULL)) {
    fprintf(fp, "%-30s", "Start time");
    fprintf(fp, "%3s", " = ");
    fprintf(fp, "%s", psd->start_time);
    print_float_val(psd->start_position_lat, "Starting latitude", "deg", fp);
    print_float_val(psd->start_position_long, "Starting longitude", "deg", fp);
    print_timer_val(psd->total_elapsed_time, "Total elapsed time", fp);
    print_timer_val(psd->total_timer_time, "Total timer time", fp);
    if (psd->units == English)
      print_float_val(psd->total_distance, "Total distance", "miles", fp);
    else
      print_float_val(psd->total_distance, "Total distance", "kilometers", fp);

    // print_float_val(psd->nec_lat , "nec_lat","" ,fp);
    // print_float_val(psd->nec_long , "nec_long","" ,fp);
    // print_float_val(psd->swc_lat , "swc_lat","" ,fp);
    // print_float_val(psd->swc_long , "swc_long","" ,fp);
    // TODO Why is this coming out bogus???
    // print_float_val(psd->total_work , "total_work","" ,fp);
    print_timer_val(psd->total_moving_time, "Total moving time", fp);
    print_timer_val(psd->avg_lap_time, "Average lap time", fp);
    print_float_val(psd->total_calories, "Total calories", "kcal", fp);
    if (psd->units == English) {
      print_float_val(psd->avg_speed, "Average speed", "miles/hour", fp);
      print_float_val(psd->max_speed, "Maximum speed", "miles/hour", fp);
    } else {
      print_float_val(psd->avg_speed, "Average speed", "kilometers/hour", fp);
      print_float_val(psd->max_speed, "Maxium speed", "kilometers/hour", fp);
    }
    if (psd->units == English) {
      print_float_val(psd->total_ascent, "Total ascent", "feet", fp);
      print_float_val(psd->total_descent, "Total descent", "feet", fp);
      print_float_val(psd->avg_altitude, "Average altitude", "feet", fp);
      print_float_val(psd->max_altitude, "Maximum altitude", "feet", fp);
      print_float_val(psd->min_altitude, "Minimum altitude", "feet", fp);
    } else {
      print_float_val(psd->total_ascent, "Total ascent", "meters", fp);
      print_float_val(psd->total_descent, "Total descent", "meters", fp);
      print_float_val(psd->avg_altitude, "Average altitude", "meters", fp);
      print_float_val(psd->max_altitude, "Maximum altitude", "meters", fp);
      print_float_val(psd->min_altitude, "Minimum altitude", "meters", fp);
    }
    print_float_val(psd->max_heart_rate, "Maximum heart rate", "", fp);
    print_float_val(psd->avg_heart_rate, "Average heart rate", "", fp);
    print_float_val(psd->max_cadence, "Maximum cadence", "", fp);
    print_float_val(psd->avg_cadence, "Average cadence", "", fp);
    if (psd->units == English) {
      print_float_val(psd->avg_temperature, "Average temperature", "deg F", fp);
      print_float_val(psd->max_temperature, "Maximum temperature", "deg F", fp);
    } else {
      print_float_val(psd->avg_temperature, "Average temperature", "deg C", fp);
      print_float_val(psd->max_temperature, "Maximum temperature", "deg C", fp);
    }
    print_float_val(psd->min_heart_rate, "Minimum heart_rate", "", fp);
    print_float_val(psd->total_anaerobic_training_effect,
                    "Total anaerobic training effect", "", fp);
    fprintf(fp, "%-30s", "End time");
    fprintf(fp, "%3s", " = ");
    fprintf(fp, "%s", psd->timestamp);
  }
}

/* Create a summary report to disk and display. */
void update_summary() {
  GtkTextMark *mark;
  GtkTextIter iter;
  GtkTextIter start;
  GtkTextIter end;

  char line[80];
  /* TODO writing this out and reading it back in is not that elegant. */
  /*Create a new summary file.*/
  FILE *fp;
  fp = fopen("runplotter.txt", "w");
  create_summary(fp);
  fclose(fp);
  /* Display the summary file in the textbuffer, textbuffer1*/
  fp = fopen("runplotter.txt", "r");
  /* Clear out anything already in the text buffer. */
  gtk_text_buffer_get_bounds(textbuffer1, &start, &end);
  gtk_text_buffer_delete(textbuffer1, &start, &end);
  /* Read the output from the file a line at a time and display it.
   */
  while (fgets(line, sizeof(line), fp) != NULL) {
    mark = gtk_text_buffer_get_insert(textbuffer1);
    gtk_text_buffer_get_iter_at_mark(textbuffer1, &iter, mark);
    gtk_text_buffer_insert(textbuffer1, &iter, line, -1);
  }
  fclose(fp);
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
 *  on the selected unit system and local time zone.
 */
void raw_to_user_session(
    SessionData *psd, time_t sess_timestamp, time_t sess_start_time,
    float sess_start_position_lat, float sess_start_position_long,
    float sess_total_elapsed_time, float sess_total_timer_time,
    float sess_total_distance, float sess_nec_lat, float sess_nec_long,
    float sess_swc_lat, float sess_swc_long, float sess_total_work,
    float sess_total_moving_time, float sess_avg_lap_time,
    float sess_total_calories, float sess_avg_speed, float sess_max_speed,
    float sess_total_ascent, float sess_total_descent, float sess_avg_altitude,
    float sess_max_altitude, float sess_min_altitude, float sess_max_heart_rate,
    float sess_avg_heart_rate, float sess_max_cadence, float sess_avg_cadence,
    float sess_avg_temperature, float sess_max_temperature,
    float sess_min_heart_rate, float sess_total_anaerobic_training_effect,
    time_t tz_offset) {
  /* Correct the start and end times to local time. */
  time_t l_time = sess_start_time + tz_offset;
  psd->start_time = strdup(asctime(gmtime(&l_time)));
  l_time = sess_timestamp + tz_offset;
  psd->timestamp = strdup(asctime(gmtime(&l_time)));
  psd->start_position_lat = sess_start_position_lat;
  psd->start_position_long = sess_start_position_long;
  psd->total_elapsed_time = sess_total_elapsed_time;
  psd->total_timer_time = sess_total_timer_time;
  if (psd->units == English) {
    psd->total_distance =
        sess_total_distance * 0.00062137119; // meters to miles
  } else {
    psd->total_distance = sess_total_distance * 0.001; // meters to kilometers
  }
  psd->nec_lat = sess_nec_lat;
  psd->nec_long = sess_nec_long;
  psd->swc_lat = sess_swc_lat;
  psd->swc_long = sess_swc_long;
  psd->total_work = sess_total_work / 1000.0; // J to kJ

  psd->total_moving_time = sess_total_moving_time;
  psd->avg_lap_time = sess_avg_lap_time;
  psd->total_calories = sess_total_calories;

  if (psd->units == English) {
    psd->avg_speed = sess_avg_speed * 2.2369363; // meters/s to miles/hr
    psd->max_speed = sess_max_speed * 2.2369363; // meters/s to miles/hr
  } else {
    psd->avg_speed = sess_avg_speed * 3.6; // meters/s to kilometers/hr
    psd->max_speed = sess_max_speed * 3.6; // meters/s to kilometers/hr
  }

  if (psd->units == English) {
    psd->total_ascent = sess_total_ascent * 3.2808399;   // meters to feet
    psd->total_descent = sess_total_descent * 3.2808399; // meters to feet
    psd->avg_altitude = sess_avg_altitude * 3.2808399;   // meters to feet
    psd->max_altitude = sess_max_altitude * 3.2808399;   // meters to feet
    psd->min_altitude = sess_min_altitude * 3.2808399;   // meters to feet
  } else {
    psd->total_ascent = sess_total_ascent * 1.0;   // meters to meters
    psd->total_descent = sess_total_descent * 1.0; // meters to meters
    psd->avg_altitude = sess_avg_altitude * 1.0;   // meters to meters
    psd->max_altitude = sess_max_altitude * 1.0;   // meters to meters
    psd->min_altitude = sess_min_altitude * 1.0;   // meters to meters
  }

  psd->max_heart_rate = sess_max_heart_rate;
  psd->avg_heart_rate = sess_avg_heart_rate;
  psd->max_cadence = sess_max_cadence;
  psd->avg_cadence = sess_avg_cadence;

  if (psd->units == English) {
    psd->avg_temperature = 1.8 * sess_avg_temperature + 32.0;
    psd->max_temperature = 1.8 * sess_max_temperature + 32.0;
  } else {
    psd->avg_temperature = sess_avg_temperature * 1.0;
    psd->max_temperature = sess_max_temperature * 1.0;
  }

  psd->min_heart_rate = sess_min_heart_rate;
  psd->total_anaerobic_training_effect = sess_total_anaerobic_training_effect;
}

/*  This routine is where the bulk of the plot initialization
 *  occurs.
 *
 *  We take the raw values from the fit file conversion
 *  routines and convert them to display-appropriate values based
 *  on:
 *  1. the selected unit system
 *  2. the local time zone
 *
 *  as well as setting labels and range limits to initial values.
 *
 */
void raw_to_user_plots(enum PlotType ptype, int num_recs, float x_raw[NSIZE],
                       float y_raw[NSIZE], float lat_raw[NSIZE],
                       float lng_raw[NSIZE], time_t sess_start_time,
                       time_t tz_offset) {
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
      y_cnv = 1.0 / 60.0;    // seconds/lap to minutes/lap
    } else {
      x_cnv = 0.001;      // meters to kilometers
      y_cnv = 1.0 / 60.0; // seconds/lap to minutes/lap
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
  /* Set start time in local time (for title) */
  time_t l_time = sess_start_time + tz_offset;
  pdest->start_time = strdup(asctime(gmtime(&l_time)));

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

/* Read the raw file data, call helper routines to convert to user-facing
   values. */
gboolean init_plot_data() {
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
  /* Parse the data from the fit file in a cGO routine and return the
   * result as a structure defined by fitwrapper.go.
   */
  result = parse_fit_file(fname, NSIZE, LSIZE);
  //  long  *pRecTimestamp = result.r1;
  float *pRecDistance = result.r3;
  float *pRecSpeed = result.r5;
  float *pRecAltitude = result.r7;
  float *pRecCadence = result.r9;
  float *pRecHeartRate = result.r11;
  float *pRecLat = result.r13;
  float *pRecLong = result.r15;
  long nRecs = result.r16;
  //  long  *pLapTimestamp = result.r18;
  float *pLapTotalDistance = result.r20;
  float *pLapStartPositionLat = result.r22;
  float *pLapStartPositionLong = result.r24;
  //  float *pLapEndPositionLat = result.r26;
  //  float *pLapEndPositionLong = result.r28;
  //  float *pLapTotalCalories = result.r30;
  float *pLapTotalElapsedTime = result.r32;
  //  float *pLapTotalTimerTime = result.r34;
  long nLaps = result.r35;
  long sessTimestamp = result.r36;
  long sessStartTime = result.r37;
  float sessStartPositionLat = result.r38;
  float sessStartPositionLong = result.r39;
  float sessTotalElapsedTime = result.r40;
  float sessTotalTimerTime = result.r41;
  float sessTotalDistance = result.r42;
  float sessNecLat = result.r43;
  float sessNecLong = result.r44;
  float sessSwcLat = result.r45;
  float sessSwcLong = result.r46;
  float sessTotalWork = result.r47;
  float sessTotalMovingTime = result.r48;
  float sessAvgLapTime = result.r49;
  float sessTotalCalories = result.r50;
  float sessAvgSpeed = result.r51;
  float sessMaxSpeed = result.r52;
  float sessTotalAscent = result.r53;
  float sessTotalDescent = result.r54;
  float sessAvgAltitude = result.r55;
  float sessMaxAltitude = result.r56;
  float sessMinAltitude = result.r57;
  float sessAvgHeartRate = result.r58;
  float sessMaxHeartRate = result.r59;
  float sessMinHeartRate = result.r60;
  float sessAvgCadence = result.r61;
  float sessMaxCadence = result.r62;
  float sessAvgTemperature = result.r63;
  float sessMaxTemperature = result.r64;
  float sessTotalAnaerobicTrainingEffect = result.r65;
  long tzOffset = result.r66;

  /* Convert the raw values to user-facing values. */
  raw_to_user_plots(PacePlot, nRecs, pRecDistance, pRecSpeed, pRecLat, pRecLong,
                    sessStartTime, tzOffset);
  raw_to_user_plots(CadencePlot, nRecs, pRecDistance, pRecCadence, pRecLat,
                    pRecLong, sessStartTime, tzOffset);
  raw_to_user_plots(HeartRatePlot, nRecs, pRecDistance, pRecHeartRate, pRecLat,
                    pRecLong, sessStartTime, tzOffset);
  raw_to_user_plots(AltitudePlot, nRecs, pRecDistance, pRecAltitude, pRecLat,
                    pRecLong, sessStartTime, tzOffset);
  raw_to_user_plots(LapPlot, nLaps, pLapTotalDistance, pLapTotalElapsedTime,
                    pLapStartPositionLat, pLapStartPositionLong, sessStartTime,
                    tzOffset);

  /* Convert the raw values to user-facing values. */
  raw_to_user_session(
      psd, sessTimestamp, sessStartTime, sessStartPositionLat,
      sessStartPositionLong, sessTotalElapsedTime, sessTotalTimerTime,
      sessTotalDistance, sessNecLat, sessNecLong, sessSwcLat, sessSwcLong,
      sessTotalWork, sessTotalMovingTime, sessAvgLapTime, sessTotalCalories,
      sessAvgSpeed, sessMaxSpeed, sessTotalAscent, sessTotalDescent,
      sessAvgAltitude, sessMaxAltitude, sessMinAltitude, sessMaxHeartRate,
      sessAvgHeartRate, sessMaxCadence, sessAvgCadence, sessAvgTemperature,
      sessMaxTemperature, sessMinHeartRate, sessTotalAnaerobicTrainingEffect,
      tzOffset);
  /* Update the summary page. */
  update_summary();
  return TRUE;
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
void plfbox(PLFLT x0, PLFLT y0, PLINT color) {
  PLFLT x[4], y[4];

  x[0] = x0;
  y[0] = 0.;
  x[1] = x0;
  y[1] = y0;
  x[2] = x0 + 1.;
  y[2] = y0;
  x[3] = x0 + 1.;
  y[3] = 0.;
  plcol0(color);
  plfill(4, x, y);
  plcol0(15);
  pllsty(1);
  plline(4, x, y);
}

/* Draw a bar chart */
void draw_bar(int width, int height) {
  char string[8];
  plwind(0.0, (float)plap->num_pts - 1.0, plap->ymin, plap->ymax);
  plscol0a(15, 128, 128, 128, 0.9); // light gray for background
  plcol0(15);
  plbox("bc", 1.0, 0, "bcnv", 1.0, 0);
  pllab(plap->xaxislabel, plap->yaxislabel, plap->start_time);
  // Normal color.
  plscol0a(2, plap->linecolor[0], plap->linecolor[1], plap->linecolor[2], 0.3);
  // Highlight (progress) color.
  plscol0a(3, plap->linecolor[0], plap->linecolor[1], plap->linecolor[2], 0.5);
  float tot_dist = 0.0;
  for (int i = 0; i < plap->num_pts - 1; i++) {
    tot_dist = plap->x[i] + tot_dist;
    plcol0(15);
    plpsty(0);
    if (ppace->x[currIdx] > tot_dist) {
      plfbox(i, plap->y[i], 3);
    } else {
      plfbox(i, plap->y[i], 2);
    }
    /* x axis */
    sprintf(string, "%1.0f", (float)i + 1.0);
    float bar_width = 1.0 / ((float)(plap->num_pts) - 1.0);
    float xposn = (i + 0.5) * bar_width;
    plmtex("b", 1.0, xposn, 0.5, string);
    /* bar label */
    double secs, mins;
    secs = modf(plap->y[i], &mins);
    secs *= 60.0;
    snprintf(string, 8, "%2.0f:%02.0f", mins, secs);
    plptex((float)i + 0.5, (1.1 * plap->ymin), 0.0, 90.0, 0.0, string);
  }
}

/* Convenience function to find active radio button. */
enum PlotType checkRadioButtons() {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_Pace)) == TRUE)
    return PacePlot;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_Cadence)) == TRUE)
    return CadencePlot;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_HeartRate)) == TRUE)
    return HeartRatePlot;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_Altitude)) == TRUE)
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
#ifdef _WIN32
G_MODULE_EXPORT
#endif
gboolean on_da_draw(GtkWidget *widget, GdkEventExpose *event, gpointer *data) {

  PLINT width, height;
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
  // plsdev("extcairo");
  plsdev("svg");
  /* Device attributes */
  FILE *fp;
  fp = fopen("runplotter.svg", "w");
  plsfile(fp);
  plinit();

  pl_cmd(PLESC_DEVINIT, cr);

  GtkAllocation *alloc = g_new(GtkAllocation, 1);
  gtk_widget_get_allocation(widget, alloc);
  width = alloc->width;
  height = alloc->height;
  g_free(alloc);
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

  /* Reload svg to cairo context. */
  GError **error = NULL;
  RsvgHandle *handle = rsvg_handle_new_from_file("runplotter.svg", error);
  RsvgRectangle viewport = {0, 0, 0, 0};
  rsvg_handle_render_document(handle, cr, &viewport, error);

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
#ifdef _WIN32
G_MODULE_EXPORT
#endif
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
#ifdef _WIN32
G_MODULE_EXPORT
#endif
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
#ifdef _WIN32
G_MODULE_EXPORT
#endif
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
static int init_map() {

  // Load start, stop image for map points of interest.
  starImage = gdk_pixbuf_new_from_file_at_size("poi.png", 24, 24, NULL);

  // Geographical center of contiguous US
  float defaultLatitude = 39.8355;
  float defaultLongitude = -99.0909;
  int defaultzoom = 4;
  /*
  OSM_GPS_MAP_SOURCE_NULL,
  OSM_GPS_MAP_SOURCE_OPENSTREETMAP,
  OSM_GPS_MAP_SOURCE_OPENSTREETMAP_RENDERER,
  OSM_GPS_MAP_SOURCE_OPENAERIALMAP,
  OSM_GPS_MAP_SOURCE_MAPS_FOR_FREE,
  OSM_GPS_MAP_SOURCE_OPENCYCLEMAP,
  OSM_GPS_MAP_SOURCE_OSM_PUBLIC_TRANSPORT,
  OSM_GPS_MAP_SOURCE_GOOGLE_STREET,
  OSM_GPS_MAP_SOURCE_GOOGLE_SATELLITE,
  OSM_GPS_MAP_SOURCE_GOOGLE_HYBRID,
  OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_STREET,
  OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_SATELLITE,
  OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_HYBRID,
  OSM_GPS_MAP_SOURCE_OSMC_TRAILS,
  OSM_GPS_MAP_SOURCE_LAST
  */

  // OsmGpsMapSource_t source = OSM_GPS_MAP_SOURCE_OPENSTREETMAP;
  OsmGpsMapSource_t source = OSM_GPS_MAP_SOURCE_GOOGLE_STREET;
  // if ( !osm_gps_map_source_is_valid(source) )
  //       return 1;

  GtkWidget *wid = g_object_new(
      OSM_TYPE_GPS_MAP, "map-source", source, "tile-cache", "/tmp/",
      //      "user-agent",
      //      "runplotter.c", // Always set user-agent, for better tile-usage
      //      compliance
      NULL);
  //  GtkRequisition req;
  //  gtk_widget_get_preferred_size(wid, &req, NULL);
  //  printf("%d %d\n", req.width,req.height);
  map = OSM_GPS_MAP(wid);
  osm_gps_map_set_center_and_zoom(OSM_GPS_MAP(map), defaultLatitude,
                                  defaultLongitude, defaultzoom);
  /* Add the global widget to the global GTKFrame named viewport */
  gtk_container_add(GTK_CONTAINER(viewport), wid);

  return 0;
}

/* Convenience routine to move marker. */
static void move_marker(gdouble new_lat, gdouble new_lng) {
  if (posnTrackMarker != NULL) {
    osm_gps_map_image_remove(map, posnTrackMarker);
    posnTrackMarker = osm_gps_map_image_add(map, new_lat, new_lng, starImage);
  }
}

/* Update the map. */
static void create_map() {
  // Geographical center of contiguous US
  float defaultLatitude = 39.8355;
  float defaultLongitude = -99.0909;
  GdkRGBA routeTrackColor;

  /* Define colors for start, end markers.*/
  //  clutter_color_from_string(&my_green, "rgba(77, 175, 74, 0.9)");
  //  clutter_color_from_string(&my_magenta, "rgba(156, 100, 134, 0.9)");
  //  clutter_color_from_string(&my_blue, "rgba(31, 119, 180, 0.9)");
  if ((map != NULL) && (pd != NULL) && (pd->lat != NULL) && (pd->lng != NULL)) {
    /* Center at the start. */
    osm_gps_map_set_center(OSM_GPS_MAP(map), pd->lat[0], pd->lng[0]);
    /*Create a "track" for the run. */
    if (routeTrack != NULL) {
      osm_gps_map_track_remove(map, routeTrack);
    }
    routeTrack = osm_gps_map_track_new();
    gdk_rgba_parse(&routeTrackColor, "rgba(156,100,134,0.9)");
    osm_gps_map_track_set_color(routeTrack, &routeTrackColor);
    osm_gps_map_track_add(OSM_GPS_MAP(map), routeTrack);
    for (int i = 0; i < pd->num_pts; i++) {
      OsmGpsMapPoint *mapPoint =
          osm_gps_map_point_new_degrees(pd->lat[i], pd->lng[i]);
      osm_gps_map_track_add_point(routeTrack, mapPoint);
    }
    /* Add start and end markers. */

    if (startTrackMarker != NULL) {
      osm_gps_map_image_remove(map, startTrackMarker);
    }
    if (endTrackMarker != NULL) {
      osm_gps_map_image_remove(map, endTrackMarker);
    }
    if (posnTrackMarker != NULL) {
      osm_gps_map_image_remove(map, posnTrackMarker);
    }
    startTrackMarker =
        osm_gps_map_image_add(map, pd->lat[0], pd->lng[0], starImage);
    endTrackMarker = osm_gps_map_image_add(map, pd->lat[pd->num_pts - 1],
                                           pd->lng[pd->num_pts - 1], starImage);
    /* Add current position marker */

    posnTrackMarker = osm_gps_map_image_add(map, pd->lat[currIdx],
                                            pd->lng[currIdx], starImage);

  } else {
    /* Start-up. */
    osm_gps_map_set_center(OSM_GPS_MAP(map), defaultLatitude, defaultLongitude);
  }
}

/* Zoom in. */
static void zoom_in(GtkWidget *widget) {
  osm_gps_map_zoom_in(OSM_GPS_MAP(map));
}

/* Zoom out. */
static void zoom_out(GtkWidget *widget) {
  osm_gps_map_zoom_out(OSM_GPS_MAP(map));
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
#ifdef _WIN32
G_MODULE_EXPORT
#endif
void on_rb_pace(GtkToggleButton *togglebutton) {
  pd = ppace;
  gtk_widget_queue_draw(GTK_WIDGET(da));
  g_signal_emit_by_name(sc_IdxPct, "value-changed");
}

/* User has selected Cadence Graph. */
#ifdef _WIN32
G_MODULE_EXPORT
#endif
void on_rb_cadence(GtkToggleButton *togglebutton) {
  pd = pcadence;
  gtk_widget_queue_draw(GTK_WIDGET(da));
  g_signal_emit_by_name(sc_IdxPct, "value-changed");
}

/* User has selected Heartrate Graph. */
#ifdef _WIN32
G_MODULE_EXPORT
#endif
void on_rb_heartrate(GtkToggleButton *togglebutton) {
  pd = pheart;
  gtk_widget_queue_draw(GTK_WIDGET(da));
  g_signal_emit_by_name(sc_IdxPct, "value-changed");
}

/* User has selected Altitude Graph. */
#ifdef _WIN32
G_MODULE_EXPORT
#endif
void on_rb_altitude(GtkToggleButton *togglebutton) {
  pd = paltitude;
  gtk_widget_queue_draw(GTK_WIDGET(da));
  g_signal_emit_by_name(sc_IdxPct, "value-changed");
}

/* User has selected Splits Graph. */
#ifdef _WIN32
G_MODULE_EXPORT
#endif
void on_rb_splits(GtkToggleButton *togglebutton) {
  gtk_widget_queue_draw(GTK_WIDGET(da));
  g_signal_emit_by_name(sc_IdxPct, "value-changed");
}

/* User has pressed open a new file. */
#ifdef _WIN32
G_MODULE_EXPORT
#endif
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
/*
 *  Update the map, graph, and indicator label based on the
 *  slider position.
 */
void on_update_index(GtkScale *widget, gpointer *data) {
  GtkAdjustment *adj;
  // What's the new value in percent of scale?
  adj = gtk_range_get_adjustment((GtkRange *)widget);
  gdouble val = gtk_adjustment_get_value(adj);
  // Slider from zero to 100 - normalized.  Calculate portion of activity.
  currIdx = (int)floor(val / 100.0 * (float)ppace->num_pts);
  // Redraw graph.
  gtk_widget_queue_draw(GTK_WIDGET(da));
  // Redraw the position marker on the map.
  if ((map != NULL) && (posnTrackMarker != NULL)) {
    move_marker(pd->lat[currIdx], pd->lng[currIdx]);
  }
  // Update the label below the graph.
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
  textbuffer1 = GTK_TEXT_BUFFER(gtk_builder_get_object(builder, "textbuffer1"));
  viewport = GTK_FRAME(gtk_builder_get_object(builder, "viewport"));
  da = GTK_DRAWING_AREA(gtk_builder_get_object(builder, "da"));
  rb_Pace = GTK_RADIO_BUTTON(gtk_builder_get_object(builder, "rb_Pace"));
  rb_Cadence = GTK_RADIO_BUTTON(gtk_builder_get_object(builder, "rb_Cadence"));
  rb_HeartRate =
      GTK_RADIO_BUTTON(gtk_builder_get_object(builder, "rb_HeartRate"));
  rb_Altitude =
      GTK_RADIO_BUTTON(gtk_builder_get_object(builder, "rb_Altitude"));
  rb_Splits = GTK_RADIO_BUTTON(gtk_builder_get_object(builder, "rb_Splits"));
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
  // if (gtk_clutter_init(&argc, &argv) != CLUTTER_INIT_SUCCESS)
  //  return 1;
  if (init_map() != 0) {
    return 1;
  }
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
                   NULL);
  g_signal_connect(GTK_BUTTON(btn_Zoom_Out), "clicked", G_CALLBACK(zoom_out),
                   NULL);
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
#ifdef _WIN32
G_MODULE_EXPORT
#endif
void on_window1_destroy() {
  destroy_plots();
  gtk_main_quit();
}
