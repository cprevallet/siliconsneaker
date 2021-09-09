/*
 * This program provides a GTK graphical user interface to PLPlot graphical
 * plotting routines in order to plot running files stored in the Garmin
 * (Dynasteam) FIT format.
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
 *	libgtk-3.so.0
 *  libplplot.so.17
 *	libcairo.so.2
 *  libosmgpsmap-1.0
 *  librsvg-2.0
 */

/* Utilities */
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 * GUI
 */
#include <gdk/gdk.h>
#include <glib.h>
#include <gtk/gtk.h>
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
#define VERSION 1.0
// Maximum readable records from a fit file.
// 2880 is large enough for 4 hour marathon at 5 sec intervals
#define NSIZE 2880
// Maximum readable laps from a fit file.
#define LSIZE 400

// the result structure defined by fitwrapper.h
struct parse_fit_file_return result;

enum ZoomState
{
  Press = 0,
  Move = 1,
  Release = 2
};
enum UnitSystem
{
  Metric = 1,
  English = 0
};
enum PlotType
{
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
typedef struct PlotData
{
  enum PlotType ptype;
  int num_pts;
  PLFLT *x; // xy data pairs, world coordinates
  PLFLT *y;
  PLFLT xmin;
  PLFLT xmax;
  PLFLT ymin;
  PLFLT ymax;
  PLFLT vw_xmin; // view, world coordinates
  PLFLT vw_xmax;
  PLFLT vw_ymin;
  PLFLT vw_ymax;
  PLFLT zm_xmin; // zoom limits, world coordinates
  PLFLT zm_xmax;
  PLFLT zm_ymin;
  PLFLT zm_ymax;
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

/* Similar to above but for an entire workout
 * session to be displayed as a summary.
 */
typedef struct SessionData
{
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

/* The data structures for the data plots.  There is one for each
 * type of plot and an additional pointer, pd, that is assigned
 * from one of the other four depending on what the user is currently
 * displaying.  Finally, there is another pointer for the overall
 * session data displayed by the summary.
 */
typedef struct AllData
{
  struct PlotData *ppace;
  struct PlotData *pcadence;
  struct PlotData *pheart;
  struct PlotData *paltitude;
  struct PlotData *plap;
  struct PlotData *pd;
  struct SessionData *psd;
} AllData;

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
GtkButton *btn_Zoom_In, *btn_Zoom_Out;
GtkComboBoxText *cb_Units;
GtkScale *sc_IdxPct;
GtkLabel *lbl_val;

/* Declaration for the fit filename. */
char *fname = NULL;

/* Declarations for OsmGps maps.
 * Since these are updated by the slider, we'll make them
 * globals.  Otherwise we'd have to pass them around with
 * AllData, and that doesn't seem appropriate since that
 * structure is intended primarily for data models not UI.
 */
OsmGpsMap *map;
static GdkPixbuf *starImage = NULL;
/* Map marker, start of run. */
OsmGpsMapImage *start_track_marker = NULL;
/* Map marker, end of run. */
OsmGpsMapImage *end_track_marker = NULL;
/* Map marker, current position based on slider. */
OsmGpsMapImage *posn_track_marker = NULL;
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

/* The array index into the x,y arrays based on the slider position. */
int curr_idx = 0;
//
// Convenience functions.
//
void
printfloat (float x, char *name)
{
  printf ("%s = %f \n", name, x);
}

/* Return a fully qualified path to a temporary directory for either
 * Windows or Linux.
 */
#ifdef __linux__
char *
path_to_temp_dir ()
{
  char *tmpdir = malloc (sizeof (char) * 4096); // 4096 is the longest ext4 path
  if (getenv ("TMPDIR"))
    strcpy (tmpdir, getenv ("TMPDIR"));
  else if (getenv ("TMP"))
    strcpy (tmpdir, getenv ("TMP"));
  else if (getenv ("TEMP"))
    strcpy (tmpdir, getenv ("TEMP"));
  else if (getenv ("TEMPDIR"))
    strcpy (tmpdir, getenv ("TEMPDIR"));
  else
    strcpy (tmpdir, "/tmp/");
  return tmpdir;
}
#endif
#ifdef _WIN32
char *
path_to_temp_dir ()
{
  char *tmpdir = malloc (sizeof (char) * 260); // 260 is the longest NTFS path
  if (getenv ("TMP"))
    strcpy (tmpdir, getenv ("TMP"));
  else if (getenv ("TEMP"))
    strcpy (tmpdir, getenv ("TEMP"));
  else if (getenv ("USERPROFILE"))
    strcpy (tmpdir, getenv ("USERPROFILE"));
  else
    strcpy (tmpdir, "C:\\Temp\\");
  return tmpdir;
}
#endif

//
// Summary routines.
//

/* Convenience routine to print a floating point line. */
void
print_float_val (float val, char *plabel, char *punit, FILE *fp)
{
  if ((plabel != NULL) && (punit != NULL) && (val < FLT_MAX - 1.0))
    {
      fprintf (fp, "%-30s", plabel);
      fprintf (fp, "%3s", " = ");
      fprintf (fp, "%10.2f", val);
      fprintf (fp, "%3s", " ");
      fprintf (fp, "%-20s", punit);
      fprintf (fp, "\n");
    }
}

/* Convenience routine to print a formatted timer value. */
void
print_timer_val (float timer, char *plabel, FILE *fp)
{
  if (timer < FLT_MAX - 1.0)
    {
      double hours, secs, mins, extra;
      extra = modf (timer / 3600.0, &hours);
      secs = modf (extra * 60.0, &mins);
      secs *= 60.0;
      fprintf (fp, "%-30s", plabel);
      fprintf (fp, "%3s", " = ");
      fprintf (fp, "%4.0f:%02.0f:%02.0f", hours, mins, secs);
      fprintf (fp, "\n");
    }
}

/* Generate the summary report */
void
create_summary (FILE *fp, SessionData *psd)
{
  if ((fp != NULL) && (psd != NULL))
    {
      fprintf (fp, "%-30s", "Start time");
      fprintf (fp, "%3s", " = ");
      fprintf (fp, "%s", psd->start_time);
      print_float_val (psd->start_position_lat, "Starting latitude", "deg", fp);
      print_float_val (psd->start_position_long, "Starting longitude", "deg",
                       fp);
      print_timer_val (psd->total_elapsed_time, "Total elapsed time", fp);
      print_timer_val (psd->total_timer_time, "Total timer time", fp);
      if (psd->units == English)
        print_float_val (psd->total_distance, "Total distance", "miles", fp);
      else
        print_float_val (psd->total_distance, "Total distance", "kilometers",
                         fp);

      // print_float_val(psd->nec_lat , "nec_lat","" ,fp);
      // print_float_val(psd->nec_long , "nec_long","" ,fp);
      // print_float_val(psd->swc_lat , "swc_lat","" ,fp);
      // print_float_val(psd->swc_long , "swc_long","" ,fp);
      // TODO Why is this coming out bogus???
      // print_float_val(psd->total_work , "total_work","" ,fp);
      print_timer_val (psd->total_moving_time, "Total moving time", fp);
      print_timer_val (psd->avg_lap_time, "Average lap time", fp);
      print_float_val (psd->total_calories, "Total calories", "kcal", fp);
      if (psd->units == English)
        {
          print_float_val (psd->avg_speed, "Average speed", "miles/hour", fp);
          print_float_val (psd->max_speed, "Maximum speed", "miles/hour", fp);
        }
      else
        {
          print_float_val (psd->avg_speed, "Average speed", "kilometers/hour",
                           fp);
          print_float_val (psd->max_speed, "Maxium speed", "kilometers/hour",
                           fp);
        }
      if (psd->units == English)
        {
          print_float_val (psd->total_ascent, "Total ascent", "feet", fp);
          print_float_val (psd->total_descent, "Total descent", "feet", fp);
          print_float_val (psd->avg_altitude, "Average altitude", "feet", fp);
          print_float_val (psd->max_altitude, "Maximum altitude", "feet", fp);
          print_float_val (psd->min_altitude, "Minimum altitude", "feet", fp);
        }
      else
        {
          print_float_val (psd->total_ascent, "Total ascent", "meters", fp);
          print_float_val (psd->total_descent, "Total descent", "meters", fp);
          print_float_val (psd->avg_altitude, "Average altitude", "meters", fp);
          print_float_val (psd->max_altitude, "Maximum altitude", "meters", fp);
          print_float_val (psd->min_altitude, "Minimum altitude", "meters", fp);
        }
      print_float_val (psd->max_heart_rate, "Maximum heart rate", "", fp);
      print_float_val (psd->avg_heart_rate, "Average heart rate", "", fp);
      print_float_val (psd->max_cadence, "Maximum cadence", "", fp);
      print_float_val (psd->avg_cadence, "Average cadence", "", fp);
      if (psd->units == English)
        {
          print_float_val (psd->avg_temperature, "Average temperature", "deg F",
                           fp);
          print_float_val (psd->max_temperature, "Maximum temperature", "deg F",
                           fp);
        }
      else
        {
          print_float_val (psd->avg_temperature, "Average temperature", "deg C",
                           fp);
          print_float_val (psd->max_temperature, "Maximum temperature", "deg C",
                           fp);
        }
      print_float_val (psd->min_heart_rate, "Minimum heart_rate", "", fp);
      print_float_val (psd->total_anaerobic_training_effect,
                       "Total anaerobic training effect", "", fp);
      fprintf (fp, "%-30s", "End time");
      fprintf (fp, "%3s", " = ");
      fprintf (fp, "%s", psd->timestamp);
    }
}

/* Create a summary report to disk and display. */
void
update_summary (SessionData *psd)
{
  GtkTextMark *mark;
  GtkTextIter iter;
  GtkTextIter start;
  GtkTextIter end;
  char line[80];
  /*Create a new summary file.*/
  char *tmpfile = path_to_temp_dir ();
  strcat (tmpfile, "runplotter.txt");
  FILE *fp = fopen (tmpfile, "w");
  create_summary (fp, psd);
  fclose (fp);
  fp = fopen (tmpfile, "r");
  /* Display the summary file in the textbuffer, textbuffer1*/
  /* Clear out anything already in the text buffer. */
  gtk_text_buffer_get_bounds (textbuffer1, &start, &end);
  gtk_text_buffer_delete (textbuffer1, &start, &end);
  /* Read the output from the file a line at a time and display it.
   */
  while (fgets (line, sizeof (line), fp) != NULL)
    {
      mark = gtk_text_buffer_get_insert (textbuffer1);
      gtk_text_buffer_get_iter_at_mark (textbuffer1, &iter, mark);
      gtk_text_buffer_insert (textbuffer1, &iter, line, -1);
    }
  fclose (fp);
}

//
// Plot routines.
//

/* Set the view limits to the data extents. */
void
reset_view_limits (PlotData *pd)
{
  if (pd == NULL)
    return;
  pd->vw_xmax = pd->xmax;
  pd->vw_ymin = pd->ymin;
  pd->vw_ymax = pd->ymax;
  pd->vw_xmin = pd->xmin;
}

/* Set zoom back to zero. */
void
reset_zoom (PlotData *pd)
{
  if (pd == NULL)
    return;
  pd->zm_startx = 0;
  pd->zm_starty = 0;
  pd->zm_endx = 0;
  pd->zm_endy = 0;
}

/* Smooth the data via a 5 element Savitzky-Golay filter (destructively).
 * Ref: https://en.wikipedia.org/wiki/Savitzky%E2%80%93Golay_filter */
void
sg_smooth (PlotData *pdest)
{
  /* Set up an array with 4 extra elements to handle the start and
   * end of the series. */
  PLFLT *smooth_arr = NULL;
  int np = pdest->num_pts;
  smooth_arr = (PLFLT *) malloc ((np + 4) * sizeof (PLFLT));
  smooth_arr[0] = pdest->y[2];
  smooth_arr[1] = pdest->y[1];
  smooth_arr[np + 1] = pdest->y[np - 1];
  smooth_arr[np + 2] = pdest->y[np - 2];
  for (int i = 0; i < np; i++)
    {
      smooth_arr[i + 2] = pdest->y[i];
    }
  for (int i = 0; i < pdest->num_pts; i++)
    {
      pdest->y[i] = 1.0 / 35.0 *
                    ((-3.0 * smooth_arr[i]) + (12.0 * smooth_arr[i + 1]) +
                     (17.0 * smooth_arr[i + 2]) + (12.0 * smooth_arr[i + 3]) +
                     (-3.0 * smooth_arr[i + 4]));
    }
  free (smooth_arr);
}

/*  This routine is where the bulk of the session report
 *  initialization occurs.
 *
 *  We take the raw values from the fit file conversion
 *  routines and convert them to display-appropriate values based
 *  on the selected unit system and local time zone.
 */
void
raw_to_user_session (SessionData *psd,
                     time_t sess_timestamp,
                     time_t sess_start_time,
                     float sess_start_position_lat,
                     float sess_start_position_long,
                     float sess_total_elapsed_time,
                     float sess_total_timer_time,
                     float sess_total_distance,
                     float sess_nec_lat,
                     float sess_nec_long,
                     float sess_swc_lat,
                     float sess_swc_long,
                     float sess_total_work,
                     float sess_total_moving_time,
                     float sess_avg_lap_time,
                     float sess_total_calories,
                     float sess_avg_speed,
                     float sess_max_speed,
                     float sess_total_ascent,
                     float sess_total_descent,
                     float sess_avg_altitude,
                     float sess_max_altitude,
                     float sess_min_altitude,
                     float sess_max_heart_rate,
                     float sess_avg_heart_rate,
                     float sess_max_cadence,
                     float sess_avg_cadence,
                     float sess_avg_temperature,
                     float sess_max_temperature,
                     float sess_min_heart_rate,
                     float sess_total_anaerobic_training_effect,
                     time_t tz_offset)
{
  /* Correct the start and end times to local time. */
  time_t l_time = sess_start_time + tz_offset;
  psd->start_time = strdup (asctime (gmtime (&l_time)));
  l_time = sess_timestamp + tz_offset;
  psd->timestamp = strdup (asctime (gmtime (&l_time)));
  psd->start_position_lat = sess_start_position_lat;
  psd->start_position_long = sess_start_position_long;
  psd->total_elapsed_time = sess_total_elapsed_time;
  psd->total_timer_time = sess_total_timer_time;
  if (psd->units == English)
    {
      psd->total_distance =
          sess_total_distance * 0.00062137119; // meters to miles
    }
  else
    {
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

  if (psd->units == English)
    {
      psd->avg_speed = sess_avg_speed * 2.2369363; // meters/s to miles/hr
      psd->max_speed = sess_max_speed * 2.2369363; // meters/s to miles/hr
    }
  else
    {
      psd->avg_speed = sess_avg_speed * 3.6; // meters/s to kilometers/hr
      psd->max_speed = sess_max_speed * 3.6; // meters/s to kilometers/hr
    }

  if (psd->units == English)
    {
      psd->total_ascent = sess_total_ascent * 3.2808399;   // meters to feet
      psd->total_descent = sess_total_descent * 3.2808399; // meters to feet
      psd->avg_altitude = sess_avg_altitude * 3.2808399;   // meters to feet
      psd->max_altitude = sess_max_altitude * 3.2808399;   // meters to feet
      psd->min_altitude = sess_min_altitude * 3.2808399;   // meters to feet
    }
  else
    {
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

  if (psd->units == English)
    {
      psd->avg_temperature = 1.8 * sess_avg_temperature + 32.0;
      psd->max_temperature = 1.8 * sess_max_temperature + 32.0;
    }
  else
    {
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
void
raw_to_user_plots (PlotData *pdest,
                   int num_recs,
                   float x_raw[NSIZE],
                   float y_raw[NSIZE],
                   float lat_raw[NSIZE],
                   float lng_raw[NSIZE],
                   time_t sess_start_time,
                   time_t tz_offset)
{
  float x_cnv, y_cnv;
  /* Housekeeping. Release any memory previously allocated before
   * reinitializing.
   */
  if (pdest->x != NULL)
    free (pdest->x);
  if (pdest->y != NULL)
    free (pdest->y);
  if (pdest->lat != NULL)
    free (pdest->lat);
  if (pdest->lng != NULL)
    free (pdest->lng);
  /* How big are we? */
  pdest->num_pts = num_recs;
  /* Allocate new memory for the converted values. */
  pdest->lat = (PLFLT *) malloc (pdest->num_pts * sizeof (PLFLT));
  pdest->lng = (PLFLT *) malloc (pdest->num_pts * sizeof (PLFLT));
  pdest->x = (PLFLT *) malloc (pdest->num_pts * sizeof (PLFLT));
  pdest->y = (PLFLT *) malloc (pdest->num_pts * sizeof (PLFLT));
  /* Assign the conversion factors by plot type. */
  switch (pdest->ptype)
    {
    case PacePlot:
      if (pdest->units == English)
        {
          x_cnv = 0.00062137119; // meters to miles
          y_cnv = 0.037282272;   // meters per sec to miles per min
        }
      else
        {
          x_cnv = 0.001; // meters to kilometers
          y_cnv = 0.06;  // meters per sec to kilometers per min
        }
      break;
    case CadencePlot:
      if (pdest->units == English)
        {
          x_cnv = 0.00062137119; // meters to miles
          y_cnv = 1.0;           // steps to steps
        }
      else
        {
          x_cnv = 0.001; // meters to kilometers
          y_cnv = 1.0;   // steps to steps
        }
      break;
    case HeartRatePlot:
      if (pdest->units == English)
        {
          x_cnv = 0.00062137119; // meters to miles
          y_cnv = 1.0;           // bpm to bpm
        }
      else
        {
          x_cnv = 0.001; // meters to kilometers
          y_cnv = 1.0;   // bpm to bpm
        }
      break;
    case AltitudePlot:
      if (pdest->units == English)
        {
          x_cnv = 0.00062137119; // meters to miles
          y_cnv = 3.28084;       // meters to feet
        }
      else
        {
          x_cnv = 0.001; // meters to kilometers
          y_cnv = 1.0;   // meters to meters
        }
      break;
    case LapPlot:
      if (pdest->units == English)
        {
          x_cnv = 0.00062137119; // meters to miles
          y_cnv = 1.0 / 60.0;    // seconds/lap to minutes/lap
        }
      else
        {
          x_cnv = 0.001;      // meters to kilometers
          y_cnv = 1.0 / 60.0; // seconds/lap to minutes/lap
        }
    }
  /* Convert (or in the case of positions/time, copy) the raw values to the
   * displayed values.
   */
  for (int i = 0; i < pdest->num_pts; i++)
    {
      pdest->x[i] = (PLFLT) x_raw[i] * x_cnv;
      pdest->y[i] = (PLFLT) y_raw[i] * y_cnv;
    }
  for (int i = 0; i < pdest->num_pts; i++)
    {
      pdest->lat[i] = (PLFLT) lat_raw[i];
      pdest->lng[i] = (PLFLT) lng_raw[i];
    }
  /* Smooth the Y values. */
  // TODO Needs more testing.  I think there are some bugs.
  gboolean filter = FALSE;
  if (filter)
    sg_smooth (pdest);
  /* Set start time in local time (for title) */
  time_t l_time = sess_start_time + tz_offset;
  pdest->start_time = strdup (asctime (gmtime (&l_time)));

  /* Find plot data min, max */
  pdest->xmin = FLT_MAX;
  pdest->xmax = -FLT_MAX;
  pdest->ymin = FLT_MAX;
  pdest->ymax = -FLT_MAX;
  for (int i = 0; i < pdest->num_pts; i++)
    {
      if (pdest->x[i] < pdest->xmin)
        pdest->xmin = pdest->x[i];
      if (pdest->x[i] > pdest->xmax)
        pdest->xmax = pdest->x[i];
      if (pdest->y[i] < pdest->ymin)
        pdest->ymin = pdest->y[i];
      if (pdest->y[i] > pdest->ymax)
        pdest->ymax = pdest->y[i];
    }
  /* Set axis labels based on plot type and unit system. */
  switch (pdest->ptype)
    {
    case PacePlot:
      if (pdest->units == English)
        {
          pdest->xaxislabel = "Distance(miles)";
          pdest->yaxislabel = "Pace(min/mile)";
        }
      else
        {
          pdest->xaxislabel = "Distance(km)";
          pdest->yaxislabel = "Pace(min/km)";
        }
      break;
    case CadencePlot:
      if (pdest->units == English)
        {
          pdest->xaxislabel = "Distance(miles)";
          pdest->yaxislabel = "Cadence(steps/min)";
        }
      else
        {
          pdest->xaxislabel = "Distance(km)";
          pdest->yaxislabel = "Cadence(steps/min)";
        }
      break;
    case AltitudePlot:
      if (pdest->units == English)
        {
          pdest->xaxislabel = "Distance(miles)";
          pdest->yaxislabel = "Altitude (feet)";
        }
      else
        {
          pdest->xaxislabel = "Distance(km)";
          pdest->yaxislabel = "Altitude(meters)";
        }
      break;
    case HeartRatePlot:
      if (pdest->units == English)
        {
          pdest->xaxislabel = "Distance(miles)";
          pdest->yaxislabel = "Heart rate (bpm)";
        }
      else
        {
          pdest->xaxislabel = "Distance(km)";
          pdest->yaxislabel = "Heart rate (bpm)";
        }
      break;
    case LapPlot:
      if (pdest->units == English)
        {
          pdest->xaxislabel = "Lap";
          pdest->yaxislabel = "Elapsed Split Time(min)";
        }
      else
        {
          pdest->xaxislabel = "Lap";
          pdest->yaxislabel = "Elapsed Split Time(min)";
        }
    }
  /* Set the view to the data extents. */
  pdest->vw_xmax = pdest->xmax;
  pdest->vw_ymin = pdest->ymin;
  pdest->vw_ymax = pdest->ymax;
  pdest->vw_xmin = pdest->xmin;
  pdest->zm_startx = 0;
  pdest->zm_starty = 0;
  pdest->zm_endx = 0;
  pdest->zm_endy = 0;
  return;
}

/* Read the raw file data, call helper routines to convert to user-facing
   values. */
gboolean
init_plot_data (AllData *pall)
{
  /* Unit system first. */
  gchar *user_units = gtk_combo_box_text_get_active_text (cb_Units);
  if (!strcmp (user_units, "Metric"))
    {
      pall->ppace->units = Metric;
      pall->pcadence->units = Metric;
      pall->pheart->units = Metric;
      pall->paltitude->units = Metric;
      pall->plap->units = Metric;
      pall->psd->units = Metric;
    }
  else
    {
      pall->ppace->units = English;
      pall->pcadence->units = English;
      pall->pheart->units = English;
      pall->paltitude->units = English;
      pall->plap->units = English;
      pall->psd->units = English;
    }
  g_free (user_units);
  /* Parse the data from the fit file in a cGO routine and return the
   * result as a structure defined by fitwrapper.go.
   */
  result = parse_fit_file (fname, NSIZE, LSIZE);
  //  long  *pRecTimestamp = result.r1;
  float *prec_distance = result.r3;
  float *prec_speed = result.r5;
  float *prec_altitude = result.r7;
  float *prec_cadence = result.r9;
  float *prec_heartrate = result.r11;
  float *prec_lat = result.r13;
  float *prec_long = result.r15;
  long nRecs = result.r16;
  //  long  *pLapTimestamp = result.r18;
  float *plap_total_distance = result.r20;
  float *plap_start_position_lat = result.r22;
  float *plap_start_position_long = result.r24;
  //  float *plap_end_position_lat = result.r26;
  //  float *plap_end_position_long = result.r28;
  //  float *plap_total_calories = result.r30;
  float *plap_total_elapsed_time = result.r32;
  //  float *pLapTotalTimerTime = result.r34;
  long nLaps = result.r35;
  long sess_timestamp = result.r36;
  long sess_start_time = result.r37;
  float sess_start_position_lat = result.r38;
  float sess_start_position_long = result.r39;
  float sess_total_elapsed_time = result.r40;
  float sess_total_timer_time = result.r41;
  float sess_total_distance = result.r42;
  float sess_nec_latitude = result.r43;
  float sess_nec_longitude = result.r44;
  float sess_swc_latitude = result.r45;
  float sess_swc_longitude = result.r46;
  float sess_total_work = result.r47;
  float sess_total_moving_time = result.r48;
  float sess_average_lap_time = result.r49;
  float sess_total_calories = result.r50;
  float sess_avg_speed = result.r51;
  float sess_max_speed = result.r52;
  float sess_total_ascent = result.r53;
  float sess_total_descent = result.r54;
  float sess_avg_altitude = result.r55;
  float sess_max_altitude = result.r56;
  float sess_min_altitude = result.r57;
  float sess_avg_heartrate = result.r58;
  float sess_max_heartrate = result.r59;
  float sess_min_heartrate = result.r60;
  float sess_avg_cadence = result.r61;
  float sess_max_cadence = result.r62;
  float sess_avg_temperature = result.r63;
  float sess_max_temperature = result.r64;
  float sess_total_anaerobic_training_effect = result.r65;
  long time_zone_offset = result.r66;

  /* Convert the raw values to user-facing values. */
  raw_to_user_plots (pall->ppace, nRecs, prec_distance, prec_speed, prec_lat,
                     prec_long, sess_start_time, time_zone_offset);
  raw_to_user_plots (pall->pcadence, nRecs, prec_distance, prec_cadence,
                     prec_lat, prec_long, sess_start_time, time_zone_offset);
  raw_to_user_plots (pall->pheart, nRecs, prec_distance, prec_heartrate,
                     prec_lat, prec_long, sess_start_time, time_zone_offset);
  raw_to_user_plots (pall->paltitude, nRecs, prec_distance, prec_altitude,
                     prec_lat, prec_long, sess_start_time, time_zone_offset);
  raw_to_user_plots (pall->plap, nLaps, plap_total_distance,
                     plap_total_elapsed_time, plap_start_position_lat,
                     plap_start_position_long, sess_start_time,
                     time_zone_offset);

  /* Convert the raw values to user-facing values. */
  raw_to_user_session (
      pall->psd, sess_timestamp, sess_start_time, sess_start_position_lat,
      sess_start_position_long, sess_total_elapsed_time, sess_total_timer_time,
      sess_total_distance, sess_nec_latitude, sess_nec_longitude,
      sess_swc_latitude, sess_swc_longitude, sess_total_work,
      sess_total_moving_time, sess_average_lap_time, sess_total_calories,
      sess_avg_speed, sess_max_speed, sess_total_ascent, sess_total_descent,
      sess_avg_altitude, sess_max_altitude, sess_min_altitude,
      sess_max_heartrate, sess_avg_heartrate, sess_max_cadence,
      sess_avg_cadence, sess_avg_temperature, sess_max_temperature,
      sess_min_heartrate, sess_total_anaerobic_training_effect,
      time_zone_offset);
  return TRUE;
}

/* A custom axis labeling function for a pace plot. */
void
pace_plot_labeler (
    PLINT axis, PLFLT value, char *label, PLINT length, PLPointer label_data)
{
  PLFLT label_val = 0.0;
  PLFLT pace_units = 0.0;
  label_val = value;

  if (axis == PL_Y_AXIS)
    {
      if (label_val > 0)
        pace_units = 1 / label_val;
      else
        pace_units = 999.0;
      double secs, mins;
      secs = modf (pace_units, &mins);
      secs *= 60.0;
      snprintf (label, (size_t) length, "%02.0f:%02.0f", mins, secs);
    }

  if (axis == PL_X_AXIS)
    snprintf (label, (size_t) length, "%3.2f", value);
}

/* A custom axis labeling function for a cadence plot. */
void
cadence_plot_labeler (
    PLINT axis, PLFLT value, char *label, PLINT length, PLPointer label_data)
{
  if (axis == PL_Y_AXIS)
    snprintf (label, (size_t) length, "%3.2f", value);
  if (axis == PL_X_AXIS)
    snprintf (label, (size_t) length, "%3.2f", value);
}

/* A custom axis labeling function for a heart rate plot. */
void
heart_rate_plot_labeler (
    PLINT axis, PLFLT value, char *label, PLINT length, PLPointer label_data)
{
  if (axis == PL_Y_AXIS)
    snprintf (label, (size_t) length, "%3.0f", value);
  if (axis == PL_X_AXIS)
    snprintf (label, (size_t) length, "%3.2f", value);
}

/* A custom axis labeling function for an altitude plot. */
void
altitude_plot_labeler (
    PLINT axis, PLFLT value, char *label, PLINT length, PLPointer label_data)
{
  if (axis == PL_Y_AXIS)
    snprintf (label, (size_t) length, "%3.0f", value);
  if (axis == PL_X_AXIS)
    snprintf (label, (size_t) length, "%3.2f", value);
}

/* Draw an xy plot. */

void
draw_xy (PlotData *pd, int width, int height)
{
  float ch_size = 4.0; // mm
  float scf = 1.0;     // dimensionless
  PLFLT n_xmin, n_xmax, n_ymin, n_ymax;
  PLFLT x_hair = 0;
  PLFLT hairline_x[2], hairline_y[2];
  if ((pd->x != NULL) && (pd->y != NULL))
    {
      /* Do your drawing. */
      /* Color */
      plscol0a (1, 65, 209, 65, 0.25);   // light green for selector
      plscol0a (15, 128, 128, 128, 0.9); // light gray for background
      plscol0a (2, pd->linecolor[0], pd->linecolor[1], pd->linecolor[2], 0.8);
      plwind (pd->vw_xmin, pd->vw_xmax, pd->vw_ymin, pd->vw_ymax);
      /* Adjust character size. */
      plschr (ch_size, scf);
      plcol0 (15);
      /* Setup a custom axis tick label function. */
      switch (pd->ptype)
        {
        case PacePlot:
          plslabelfunc (pace_plot_labeler, NULL);
          break;
        case CadencePlot:
          plslabelfunc (cadence_plot_labeler, NULL);
          break;
        case AltitudePlot:
          plslabelfunc (altitude_plot_labeler, NULL);
          break;
        case HeartRatePlot:
          plslabelfunc (heart_rate_plot_labeler, NULL);
          break;
        case LapPlot:
          break;
        }
      /* Create a labelled box to hold the plot using custom x,y labels. */
      // We want finer control here, so we ignore the convenience function.
      char *xopt = "bnost";
      char *yopt = "bgnost";
      // TODO valgrind reports mem lost on below line...
      plaxes (pd->vw_xmin, pd->vw_ymin, xopt, 0, 0, yopt, 0, 0);
      /* Setup axis labels and titles. */
      pllab (pd->xaxislabel, pd->yaxislabel, pd->start_time);
      /* Set line color to the second pallette color. */
      plcol0 (2);
      /* Plot the data that was loaded. */
      plwidth (2);
      plline (pd->num_pts, pd->x, pd->y);
      /* Plot symbols for individual data points. */
      // TODO valgrind reports mem lost on below line...
      // plstring(pd->num_pts, pd->x, pd->y, pd->symbol);
      /* Calculate the zoom limits (in pixels) for the graph. */
      plgvpd (&n_xmin, &n_xmax, &n_ymin, &n_ymax);
      pd->zm_xmin = width * n_xmin;
      pd->zm_xmax = width * n_xmax;
      pd->zm_ymin = height * (n_ymin - 1.0) + height;
      pd->zm_ymax = height * (n_ymax - 1.0) + height;
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
      if ((pd->zm_startx != pd->zm_endx) && (pd->zm_starty != pd->zm_endy))
        {
          plcol0 (1);
          plfill (4, rb_x, rb_y);
        }
      /* Add a hairline */
      plcol0 (15);
      /* If we are between the view limits, draw a line from the
       * current index on the x scale from the bottom to the top
       * of the view. */
      x_hair = pd->x[curr_idx];
      if ((x_hair >= pd->vw_xmin) && (x_hair <= pd->vw_xmax))
        {
          hairline_x[0] = x_hair;
          hairline_x[1] = x_hair;
          hairline_y[0] = pd->vw_ymin;
          hairline_y[1] = pd->vw_ymax;
          pllsty (2);
          plline (2, hairline_x, hairline_y);
          pllsty (1);
        }
    }
}

/* Draw a filled box. */
void
plfbox (PLFLT x0, PLFLT y0, PLINT color)
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
  plcol0 (color);
  plfill (4, x, y);
  plcol0 (15);
  pllsty (1);
  plline (4, x, y);
}

/* Draw a bar chart */
void
draw_bar (PlotData *plap, PlotData *ppace, int width, int height)
{
  char string[8];
  plwind (0.0, (float) plap->num_pts - 1.0, plap->ymin, plap->ymax);
  plscol0a (15, 128, 128, 128, 0.9); // light gray for background
  plcol0 (15);
  plbox ("bc", 1.0, 0, "bcnv", 1.0, 0);
  pllab (plap->xaxislabel, plap->yaxislabel, plap->start_time);
  // Normal color.
  plscol0a (2, plap->linecolor[0], plap->linecolor[1], plap->linecolor[2], 0.3);
  // Highlight (progress) color.
  plscol0a (3, plap->linecolor[0], plap->linecolor[1], plap->linecolor[2], 0.5);
  float tot_dist = 0.0;
  for (int i = 0; i < plap->num_pts - 1; i++)
    {
      tot_dist = plap->x[i] + tot_dist;
      plcol0 (15);
      plpsty (0);
      if (ppace->x[curr_idx] > tot_dist)
        plfbox (i, plap->y[i], 3);
      else
        plfbox (i, plap->y[i], 2);
      /* x axis */
      sprintf (string, "%1.0f", (float) i + 1.0);
      float bar_width = 1.0 / ((float) (plap->num_pts) - 1.0);
      float xposn = (i + 0.5) * bar_width;
      plmtex ("b", 1.0, xposn, 0.5, string);
      /* bar label */
      double secs, mins;
      secs = modf (plap->y[i], &mins);
      secs *= 60.0;
      snprintf (string, 8, "%2.0f:%02.0f", mins, secs);
      plptex ((float) i + 0.5, (1.1 * plap->ymin), 0.0, 90.0, 0.0, string);
    }
}

/* Convenience function to find active radio button. */
enum PlotType
checkRadioButtons ()
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rb_Pace)) == TRUE)
    return PacePlot;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rb_Cadence)) == TRUE)
    return CadencePlot;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rb_HeartRate)) == TRUE)
    return HeartRatePlot;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rb_Altitude)) == TRUE)
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
gboolean
on_da_draw (GtkWidget *widget, GdkEventExpose *event, AllData *data)
{
  PLINT width, height;
  /* Can't plot uninitialized. */
  if ((data->pd == NULL) || (data->plap == NULL))
    return TRUE;
  /* "Convert" the G*t*kWidget to G*d*kWindow (no, it's not a GtkWindow!) */
  GdkWindow *window = gtk_widget_get_window (widget);
  cairo_region_t *cairoRegion = cairo_region_create ();
  GdkDrawingContext *drawingContext;
  drawingContext = gdk_window_begin_draw_frame (window, cairoRegion);
  /* Say: "I want to start drawing". */
  cairo_t *cr = gdk_drawing_context_get_cairo_context (drawingContext);
  /* Initialize plplot using the svg backend. */
  plsdev ("svg");
  /* Device attributes */
  char *tmpfile = path_to_temp_dir ();
  strcat (tmpfile, "runplotter.svg");
  FILE *fp = fopen (tmpfile, "w");
  plsfile (fp);
  plinit ();
  pl_cmd (PLESC_DEVINIT, cr);
  /* Find widget allocated width, height.*/
  GtkAllocation *alloc = g_new (GtkAllocation, 1);
  gtk_widget_get_allocation (widget, alloc);
  width = alloc->width;
  height = alloc->height;
  g_free (alloc);
  /* Viewport and window */
  pladv (0);
  plvasp ((float) height / (float) width);
  /* Draw an xy plot or a bar chart. */
  switch (checkRadioButtons ())
    {
    case PacePlot:
      draw_xy (data->pd, width, height);
      break;
    case CadencePlot:
      draw_xy (data->pd, width, height);
      break;
    case HeartRatePlot:
      draw_xy (data->pd, width, height);
      break;
    case AltitudePlot:
      draw_xy (data->pd, width, height);
      break;
    case LapPlot:
      draw_bar (data->plap, data->ppace, width, height);
      break;
    }
  /* Close PLplot library */
  plend ();
  /* Reload svg to cairo context. */
  GError **error = NULL;
  RsvgHandle *handle = rsvg_handle_new_from_file (tmpfile, error);
  RsvgRectangle viewport = { 0, 0, 0, 0 };
  rsvg_handle_render_document (handle, cr, &viewport, error);
  /* Say: "I'm finished drawing. */
  gdk_window_end_draw_frame (window, drawingContext);
  /* Cleanup */
  cairo_region_destroy (cairoRegion);
  return FALSE;
}

/* Calculate the graph ("world") x,y coordinates corresponding to the
 * GUI mouse ("device") coordinates.
 *
 * The plot view bounds (vw_xmin, vw_xmax, vw_ymin, vw_ymax) and the plot
 * zoom bounds (zm_xmin, zm_xmax, zm_ymin, zm_ymax) are calculated
 * by the draw routine.
 *
 */
void
gui_to_world (struct PlotData *pd, GdkEventButton *event, enum ZoomState state)
{
  if (pd == NULL)
    {
      return;
    }
  float fractx = (event->x - pd->zm_xmin) / (pd->zm_xmax - pd->zm_xmin);
  float fracty = (pd->zm_ymax - event->y) / (pd->zm_ymax - pd->zm_ymin);
  if (state == Press)
    {
      pd->zm_startx = fractx * (pd->vw_xmax - pd->vw_xmin) + pd->vw_xmin;
      pd->zm_starty = fracty * (pd->vw_ymax - pd->vw_ymin) + pd->vw_ymin;
    }
  if (state == Release || state == Move)
    {
      pd->zm_endx = fractx * (pd->vw_xmax - pd->vw_xmin) + pd->vw_xmin;
      pd->zm_endy = fracty * (pd->vw_ymax - pd->vw_ymin) + pd->vw_ymin;
    }
}

/* Convenience routine to change the cursor style. */
void
change_cursor (GtkWidget *widget, const gchar *name)
{
  GdkDisplay *display = gtk_widget_get_display (widget);
  GdkCursor *cursor;
  cursor = gdk_cursor_new_from_name (display, name);
  gdk_window_set_cursor (gtk_widget_get_window (widget), cursor);
  // Release the (memory) reference on the cursor.
  g_object_unref (cursor);
}

/* Handle mouse button press. */
#ifdef _WIN32
G_MODULE_EXPORT
#endif
gboolean
on_button_press (GtkWidget *widget, GdkEvent *event, AllData *data)
{
  guint buttonnum;
  if (data->pd == NULL)
    return FALSE;
  gdk_event_get_button (event, &buttonnum);
  if (buttonnum == 3)
    change_cursor (widget, "crosshair");
  if (buttonnum == 1)
    change_cursor (widget, "hand1");
  /* Set user selected starting x, y in world coordinates. */
  gui_to_world (data->pd, (GdkEventButton *) event, Press);
  return TRUE;
}

/* Handle mouse button release. */
#ifdef _WIN32
G_MODULE_EXPORT
#endif
gboolean
on_button_release (GtkWidget *widget, GdkEvent *event, AllData *data)
{
  guint buttonnum;
  if (data->pd == NULL)
    return FALSE;
  change_cursor (widget, "default");
  gdk_event_get_button (event, &buttonnum);
  /* Zoom out if right mouse button release. */
  if (buttonnum == 2)
    {
      reset_view_limits (data->pd);
      gtk_widget_queue_draw (GTK_WIDGET (da));
      reset_zoom (data->pd);
      return TRUE;
    }
  /* Zoom in if left mouse button release. */
  /* Set user selected ending x, y in world coordinates. */
  gui_to_world (data->pd, (GdkEventButton *) event, Release);
  if ((data->pd->zm_startx != data->pd->zm_endx) &&
      (data->pd->zm_starty != data->pd->zm_endy))
    {
      /* Zoom */
      if (buttonnum == 3)
        {
          data->pd->vw_xmin = fmin (data->pd->zm_startx, data->pd->zm_endx);
          data->pd->vw_ymin = fmin (data->pd->zm_starty, data->pd->zm_endy);
          data->pd->vw_xmax = fmax (data->pd->zm_startx, data->pd->zm_endx);
          data->pd->vw_ymax = fmax (data->pd->zm_starty, data->pd->zm_endy);
        }
      /* Pan */
      if (buttonnum == 1)
        {
          data->pd->vw_xmin =
              data->pd->vw_xmin + (data->pd->zm_startx - data->pd->zm_endx);
          data->pd->vw_xmax =
              data->pd->vw_xmax + (data->pd->zm_startx - data->pd->zm_endx);
          data->pd->vw_ymin =
              data->pd->vw_ymin + (data->pd->zm_starty - data->pd->zm_endy);
          data->pd->vw_ymax =
              data->pd->vw_ymax + (data->pd->zm_starty - data->pd->zm_endy);
        }
      gtk_widget_queue_draw (GTK_WIDGET (da));
      reset_zoom (data->pd);
    }
  return TRUE;
}

/* Handle mouse motion event by drawing a filled
 * polygon.
 */
#ifdef _WIN32
G_MODULE_EXPORT
#endif
gboolean
on_motion_notify (GtkWidget *widget, GdkEventButton *event, AllData *data)
{

  if (data->pd == NULL)
    return FALSE;
  if (event->state & GDK_BUTTON3_MASK)
    {
      gui_to_world (data->pd, event, Move);
      gtk_widget_queue_draw (GTK_WIDGET (da));
    }
  return TRUE;
}

//
// Map Stuff
//

/* Instantiate a global instance of a map widget.
 * Add it to a GTKFrame named viewport.
 */
static int
init_map ()
{

  // Load start, stop image for map points of interest.
  starImage = gdk_pixbuf_new_from_file_at_size ("poi.png", 24, 24, NULL);

  // Geographical center of contiguous US
  float default_latitude = 39.8355;
  float default_longitude = -99.0909;
  int defaultzoom = 4;

  GtkWidget *wid = g_object_new (OSM_TYPE_GPS_MAP, NULL);
  g_object_set (wid, "map-source", source, NULL);
  g_object_set (wid, "tile-cache", "/tmp/", NULL);
  map = OSM_GPS_MAP (wid);
  osm_gps_map_set_center_and_zoom (OSM_GPS_MAP (map), default_latitude,
                                   default_longitude, defaultzoom);
  /* Add the global widget to the global GTKFrame named viewport */
  gtk_container_add (GTK_CONTAINER (viewport), wid);

  return 0;
}

/* Convenience routine to move marker. */
static void
move_marker (gdouble new_lat, gdouble new_lng)
{
  if (posn_track_marker != NULL)
    {
      osm_gps_map_image_remove (map, posn_track_marker);
      posn_track_marker =
          osm_gps_map_image_add (map, new_lat, new_lng, starImage);
    }
}

/* Calculate center of latitude and longitude readings.*/
void
findCenter (int numPts,
            double lat[],
            double lng[],
            double center[],
            float *min_lat,
            float *min_lng,
            float *max_lat,
            float *max_lng)
{
  *min_lat = DBL_MAX;
  *max_lat = -DBL_MAX;
  *min_lng = DBL_MAX;
  *max_lng = -DBL_MAX;
  for (int i = 1; i < numPts; i++)
    {
      if (lat[i] < *min_lat)
        *min_lat = lat[i];
      if (lng[i] < *min_lng)
        *min_lng = lng[i];
      if (lat[i] > *max_lat)
        *max_lat = lat[i];
      if (lng[i] > *max_lng)
        *max_lng = lng[i];
    }
  center[0] = (*max_lat + *min_lat) / 2.0;
  center[1] = (*max_lng + *min_lng) / 2.0;
}

/* Return the latitude, longitude limits for a map at a particular
 * zoom level.
 */
void
mapLimits (float *min_map_lat,
           float *min_map_lng,
           float *max_map_lat,
           float *max_map_lng)
{
  OsmGpsMapPoint top_left;
  OsmGpsMapPoint bot_right;
  float tl_lat, tl_lng, br_lat, br_lng;
  osm_gps_map_get_bbox (map, &top_left, &bot_right);
  osm_gps_map_point_get_degrees (&top_left, &tl_lat, &tl_lng);
  osm_gps_map_point_get_degrees (&bot_right, &br_lat, &br_lng);
  *max_map_lat = fmaxf (tl_lat, br_lat);
  *min_map_lat = fminf (tl_lat, br_lat);
  *max_map_lng = fmaxf (tl_lng, br_lng);
  *min_map_lng = fminf (tl_lng, br_lng);
}

/* Calculate the center and zoom level based on the latitude
 * and longitude readings.
 */
void
setCenterAndZoom (AllData *data)
{
  double center[2] = { 0.0, 0.0 };
  float max_lat, min_lat, max_lng, min_lng;
  float max_map_lat, min_map_lat, max_map_lng, min_map_lng;
  int min_zoom, max_zoom, zoom;
  max_zoom = osm_gps_map_source_get_max_zoom (source);
  min_zoom = osm_gps_map_source_get_min_zoom (source);
  zoom = max_zoom;
  findCenter (data->pd->num_pts, data->pd->lat, data->pd->lng, center, &min_lat,
              &min_lng, &max_lat, &max_lng);
  osm_gps_map_set_center_and_zoom (OSM_GPS_MAP (map), center[0], center[1],
                                   zoom);
  mapLimits (&min_map_lat, &min_map_lng, &max_map_lat, &max_map_lng);
  /* Repeatedly zoom out until we cover the range of the run. */
  while (((max_map_lat < max_lat || max_map_lng < max_lng ||
           min_map_lat > min_lat || min_map_lng > min_lng) &&
          zoom > min_zoom))
    {
      zoom--;
      osm_gps_map_set_center_and_zoom (OSM_GPS_MAP (map), center[0], center[1],
                                       zoom);
      mapLimits (&min_map_lat, &min_map_lng, &max_map_lat, &max_map_lng);
    }
}

/* Calculate the mean and standard deviation. */
void
stats (double *arr, int arr_size, float *mean, float *stdev)
{
  float sum = 0.0;
  for (int i = 0; i < arr_size; i++)
    sum += arr[i];
  *mean = sum / ((float) arr_size);
  // printf("%.2f ", *mean);
  sum = 0.0;
  for (int i = 0; i < arr_size; i++)
    sum += pow ((arr[i] - *mean), 2);
  *stdev = sqrt ((sum / (float) (arr_size)));
  // printf("%.2f", *stdev);
}

/* Return a color based on how far an individual pace is
 * from the average.  This is used to construct a heat-map.
 */
GdkRGBA
pick_color (float average, float stdev, float speed, enum UnitSystem units)
{
  GdkRGBA slowest, slower, slow, fast, faster, fastest;
  gdk_rgba_parse (&slowest, "rgba(255,255,212, 1.0)");
  gdk_rgba_parse (&slower, "rgba(254,227,145, 1.0)");
  gdk_rgba_parse (&slow, "rgba(254,196,79, 1.0)");
  gdk_rgba_parse (&fast, "rgba(254,153,41, 1.0)");
  gdk_rgba_parse (&faster, "rgba(217,95,14, 1.0)");
  gdk_rgba_parse (&fastest, "rgba(153,52,4, 1.0)");
  /* Blue color gradients */
  /*
  gdk_rgba_parse(&fastest,  "rgba( 8,  81,156, 1.0)");
  gdk_rgba_parse(&faster,   "rgba( 49,130,189, 1.0)");
  gdk_rgba_parse(&fast,     "rgba(107,174,214, 1.0)");
  gdk_rgba_parse(&slow,     "rgba(158,202,225, 1.0)");
  gdk_rgba_parse(&slower,   "rgba(198,219,239, 1.0)");
  gdk_rgba_parse(&slowest,  "rgba(239,243,255, 1.0)");
*/
  float fastest_limit, faster_limit, fast_limit, slow_limit, slower_limit;
  if (speed <= 0.0)
    return slowest;
  /* Assume a normal curve.  38.2% between +/0.5 stddev
   * 30% between +/-0.5 and +/-1 stddev.
   */
  fastest_limit = average + (1.0 * stdev);
  faster_limit = average + (0.5 * stdev);
  fast_limit = average;
  slow_limit = average - (0.5 * stdev);
  slower_limit = average - (1.0 * stdev);
  if (speed > fastest_limit)
    return fastest;
  if (speed > faster_limit)
    return faster;
  if (speed > fast_limit)
    return fast;
  if (speed < slow_limit)
    return slow;
  if (speed < slower_limit)
    return slower;
  return slowest;
}

/* Update the map. */
static void
update_map (AllData *data)
{
  // Geographical center of contiguous US
  float default_latitude = 39.8355;
  float default_longitude = -99.0909;
  GdkRGBA track_color, prev_track_color;
  OsmGpsMapTrack *route_track;
  float avg_pace, stdev_pace;
  /* Get some statistics for use in generating a heatmap. */
  stats (data->ppace->y, data->ppace->num_pts, &avg_pace, &stdev_pace);
  if ((map != NULL) && (data->pd != NULL) && (data->pd->lat != NULL) &&
      (data->pd->lng != NULL))
    {
      /* Remove any previously displayed tracks. */
      osm_gps_map_track_remove_all (map);
      /* Zoom and center the map. */
      setCenterAndZoom (data);
      /* Display tracks based on speeds (aka heatmap). */
      for (int i = 0; i < data->pd->num_pts; i++)
        {
          track_color = pick_color (avg_pace, stdev_pace, data->ppace->y[i],
                                    data->ppace->units);
          if (&track_color != &prev_track_color)
            {
              route_track = osm_gps_map_track_new ();
              osm_gps_map_track_set_color (route_track, &track_color);
              osm_gps_map_track_add (OSM_GPS_MAP (map), route_track);
            }
          prev_track_color = track_color;
          OsmGpsMapPoint *mapPoint = osm_gps_map_point_new_degrees (
              data->pd->lat[i], data->pd->lng[i]);
          osm_gps_map_track_add_point (route_track, mapPoint);
        }
      /* Add start and end markers. */
      if (start_track_marker != NULL)
        osm_gps_map_image_remove (map, start_track_marker);
      if (end_track_marker != NULL)
        osm_gps_map_image_remove (map, end_track_marker);
      if (posn_track_marker != NULL)
        osm_gps_map_image_remove (map, posn_track_marker);
      start_track_marker = osm_gps_map_image_add (map, data->pd->lat[0],
                                                  data->pd->lng[0], starImage);
      end_track_marker = osm_gps_map_image_add (
          map, data->pd->lat[data->pd->num_pts - 1],
          data->pd->lng[data->pd->num_pts - 1], starImage);
      /* Add current position marker */
      posn_track_marker = osm_gps_map_image_add (
          map, data->pd->lat[curr_idx], data->pd->lng[curr_idx], starImage);
    }
  else
    {
      /* Start-up. */
      osm_gps_map_set_center (OSM_GPS_MAP (map), default_latitude,
                              default_longitude);
    }
}

/* Zoom in. */
static void
zoom_in (GtkWidget *widget)
{
  osm_gps_map_zoom_in (OSM_GPS_MAP (map));
}

/* Zoom out. */
static void
zoom_out (GtkWidget *widget)
{
  osm_gps_map_zoom_out (OSM_GPS_MAP (map));
}

//
// GTK GUI Stuff
//

/* Convenience function to reload data, update the internal data structures
 * and redraw all the widgets.
 */
void
reload_all (AllData *pall)
{
  if ((pall != NULL) && (fname != NULL))
    {
      /* Update the plots */
      init_plot_data (pall);
      /* Force a redraw on the drawing area. */
      gtk_widget_queue_draw (GTK_WIDGET (da));
      /* Update the summary table. */
      update_summary (pall->psd);
      /* Update the map. */
      update_map (pall);
      /* Update the slider and redraw. */
      g_signal_emit_by_name (sc_IdxPct, "value-changed");
    }
}

/* Default to the pace chart. */
gboolean
default_chart ()
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb_Pace), TRUE);
  return TRUE;
}

/* User has changed unit system. */
void
on_cb_units_changed (GtkComboBox *cb_Units, AllData *data)
{
  reload_all (data);
}

/* User has selected Pace Graph. */
#ifdef _WIN32
G_MODULE_EXPORT
#endif
void
on_rb_pace (GtkToggleButton *togglebutton, AllData *data)
{
  if ((data->ppace->x != NULL) && (data->ppace->y != NULL))
    {
      data->pd = data->ppace;
      gtk_widget_queue_draw (GTK_WIDGET (da));
      g_signal_emit_by_name (sc_IdxPct, "value-changed");
    }
}

/* User has selected Cadence Graph. */
#ifdef _WIN32
G_MODULE_EXPORT
#endif
void
on_rb_cadence (GtkToggleButton *togglebutton, AllData *data)
{
  if ((data->pcadence->x != NULL) && (data->pcadence->y != NULL))
    {
      data->pd = data->pcadence;
      gtk_widget_queue_draw (GTK_WIDGET (da));
      g_signal_emit_by_name (sc_IdxPct, "value-changed");
    }
}

/* User has selected Heartrate Graph. */
#ifdef _WIN32
G_MODULE_EXPORT
#endif
void
on_rb_heartrate (GtkToggleButton *togglebutton, AllData *data)
{
  if ((data->pheart->x != NULL) && (data->pheart->y != NULL))
    {
      data->pd = data->pheart;
      gtk_widget_queue_draw (GTK_WIDGET (da));
      g_signal_emit_by_name (sc_IdxPct, "value-changed");
    }
}

/* User has selected Altitude Graph. */
#ifdef _WIN32
G_MODULE_EXPORT
#endif
void
on_rb_altitude (GtkToggleButton *togglebutton, AllData *data)
{
  if ((data->paltitude->x != NULL) && (data->paltitude->y != NULL))
    {
      data->pd = data->paltitude;
      gtk_widget_queue_draw (GTK_WIDGET (da));
      g_signal_emit_by_name (sc_IdxPct, "value-changed");
    }
}

/* User has selected Splits Graph. */
#ifdef _WIN32
G_MODULE_EXPORT
#endif
void
on_rb_splits (GtkToggleButton *togglebutton, AllData *data)
{
  if ((data->plap->x != NULL) && (data->plap->y != NULL))
    {
      gtk_widget_queue_draw (GTK_WIDGET (da));
      g_signal_emit_by_name (sc_IdxPct, "value-changed");
    }
}

/* User has pressed open a new file. */
#ifdef _WIN32
G_MODULE_EXPORT
#endif
void
on_btnFileOpen_file_set (GtkFileChooserButton *btnFileOpen, AllData *pall)
{
  /* fname is a global */
  fname = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (btnFileOpen));
  if (pall != NULL)
    reload_all (pall);
}

//
// Slider/Index routines.
//
/*
 *  Update the map, graph, and indicator label based on the
 *  slider position.
 */
void
on_update_index (GtkScale *widget, AllData *data)
{
  GtkAdjustment *adj;
  // What's the new value in percent of scale?
  adj = gtk_range_get_adjustment ((GtkRange *) widget);
  gdouble val = gtk_adjustment_get_value (adj);
  // Slider from zero to 100 - normalized.  Calculate portion of activity.
  curr_idx = (int) floor (val / 100.0 * (float) data->ppace->num_pts);
  // Redraw graph.
  gtk_widget_queue_draw (GTK_WIDGET (da));
  // Redraw the position marker on the map.
  if ((map != NULL) && (posn_track_marker != NULL))
    move_marker (data->pd->lat[curr_idx], data->pd->lng[curr_idx]);
  // Update the label below the graph.
  char yval[15];
  char xval[15];
  switch (data->pd->ptype)
    {
    case PacePlot:
      pace_plot_labeler (PL_Y_AXIS, data->pd->y[curr_idx], yval, 15, NULL);
      pace_plot_labeler (PL_X_AXIS, data->pd->x[curr_idx], xval, 15, NULL);
      break;
    case CadencePlot:
      cadence_plot_labeler (PL_Y_AXIS, data->pd->y[curr_idx], yval, 15, NULL);
      cadence_plot_labeler (PL_X_AXIS, data->pd->x[curr_idx], xval, 15, NULL);
      break;
    case AltitudePlot:
      altitude_plot_labeler (PL_Y_AXIS, data->pd->y[curr_idx], yval, 15, NULL);
      altitude_plot_labeler (PL_X_AXIS, data->pd->x[curr_idx], xval, 15, NULL);
      break;
    case HeartRatePlot:
      heart_rate_plot_labeler (PL_Y_AXIS, data->pd->y[curr_idx], yval, 15,
                               NULL);
      heart_rate_plot_labeler (PL_X_AXIS, data->pd->x[curr_idx], xval, 15,
                               NULL);
      break;
    case LapPlot:
      break;
    }
  char *curr_vals;
  curr_vals = malloc (strlen (data->pd->xaxislabel) + 2 + strlen (xval) + 2 +
                      strlen (data->pd->yaxislabel) + 2 + strlen (yval) + 1);
  strcpy (curr_vals, data->pd->xaxislabel);
  strcat (curr_vals, "= ");
  strcat (curr_vals, xval);
  strcat (curr_vals, ", ");
  strcat (curr_vals, data->pd->yaxislabel);
  strcat (curr_vals, "= ");
  strcat (curr_vals, yval);
  gtk_label_set_text (lbl_val, curr_vals);
  free (curr_vals);
}

/* Call when the main window is closed.*/
#ifdef _WIN32
G_MODULE_EXPORT
#endif
void
on_window_destroy (AllData *data)
{
  gtk_main_quit ();
}

//
// Main
//

/*
 * This is the program entry point.  The builder reads an XML file (generated
 * by the Glade application and instantiate the associated (global) objects.
 *
 */
int
main (int argc, char *argv[])
{
  /*
   * Initialize instances of the main data structure.
   */

  static PlotData paceplot;
  static PlotData cadenceplot;
  static PlotData heartrateplot;
  static PlotData altitudeplot;
  static PlotData lapplot;
  static SessionData sess;

  paceplot.ptype = PacePlot;
  paceplot.symbol = "⏺";
  paceplot.xmin = 0;
  paceplot.xmax = 0;
  paceplot.ymin = 0;
  paceplot.ymax = 0;
  paceplot.num_pts = 0;
  paceplot.x = NULL;
  paceplot.y = NULL;
  paceplot.vw_xmax = 0;
  paceplot.vw_ymin = 0;
  paceplot.vw_ymax = 0;
  paceplot.vw_xmin = 0;
  paceplot.zm_xmin = 0;
  paceplot.zm_xmax = 0;
  paceplot.zm_ymin = 0;
  paceplot.zm_ymax = 0;
  paceplot.zm_startx = 0;
  paceplot.zm_starty = 0;
  paceplot.zm_endx = 0;
  paceplot.zm_endy = 0;
  paceplot.lat = NULL;
  paceplot.lng = NULL;
  paceplot.xaxislabel = NULL;
  paceplot.yaxislabel = NULL;
  paceplot.linecolor[0] = 156;
  paceplot.linecolor[1] = 100;
  paceplot.linecolor[2] = 134;
  paceplot.units = English;
  paceplot.start_time = NULL;

  cadenceplot.ptype = CadencePlot;
  cadenceplot.symbol = "⏺";
  cadenceplot.xmin = 0;
  cadenceplot.xmax = 0;
  cadenceplot.ymin = 0;
  cadenceplot.ymax = 0;
  cadenceplot.num_pts = 0;
  cadenceplot.x = NULL;
  cadenceplot.y = NULL;
  cadenceplot.vw_xmax = 0;
  cadenceplot.vw_ymin = 0;
  cadenceplot.vw_ymax = 0;
  cadenceplot.vw_xmin = 0;
  cadenceplot.zm_xmin = 0;
  cadenceplot.zm_xmax = 0;
  cadenceplot.zm_ymin = 0;
  cadenceplot.zm_ymax = 0;
  cadenceplot.zm_startx = 0;
  cadenceplot.zm_starty = 0;
  cadenceplot.zm_endx = 0;
  cadenceplot.zm_endy = 0;
  cadenceplot.lat = NULL;
  cadenceplot.lng = NULL;
  cadenceplot.xaxislabel = NULL;
  cadenceplot.yaxislabel = NULL;
  cadenceplot.linecolor[0] = 31;
  cadenceplot.linecolor[1] = 119;
  cadenceplot.linecolor[2] = 180;
  cadenceplot.units = English;
  cadenceplot.start_time = NULL;

  heartrateplot.ptype = HeartRatePlot;
  heartrateplot.symbol = "⏺";
  heartrateplot.xmin = 0;
  heartrateplot.xmax = 0;
  heartrateplot.ymin = 0;
  heartrateplot.ymax = 0;
  heartrateplot.num_pts = 0;
  heartrateplot.x = NULL;
  heartrateplot.y = NULL;
  heartrateplot.vw_xmax = 0;
  heartrateplot.vw_ymin = 0;
  heartrateplot.vw_ymax = 0;
  heartrateplot.vw_xmin = 0;
  heartrateplot.zm_xmin = 0;
  heartrateplot.zm_xmax = 0;
  heartrateplot.zm_ymin = 0;
  heartrateplot.zm_ymax = 0;
  heartrateplot.zm_startx = 0;
  heartrateplot.zm_starty = 0;
  heartrateplot.zm_endx = 0;
  heartrateplot.zm_endy = 0;
  heartrateplot.lat = NULL;
  heartrateplot.lng = NULL;
  heartrateplot.xaxislabel = NULL;
  heartrateplot.yaxislabel = NULL;
  heartrateplot.linecolor[0] = 247;
  heartrateplot.linecolor[1] = 250;
  heartrateplot.linecolor[2] = 191;
  heartrateplot.units = English;
  heartrateplot.start_time = NULL;

  altitudeplot.ptype = AltitudePlot;
  altitudeplot.symbol = "⏺";
  altitudeplot.xmin = 0;
  altitudeplot.xmax = 0;
  altitudeplot.ymin = 0;
  altitudeplot.ymax = 0;
  altitudeplot.num_pts = 0;
  altitudeplot.x = NULL;
  altitudeplot.y = NULL;
  altitudeplot.vw_xmax = 0;
  altitudeplot.vw_ymin = 0;
  altitudeplot.vw_ymax = 0;
  altitudeplot.vw_xmin = 0;
  altitudeplot.zm_xmin = 0;
  altitudeplot.zm_xmax = 0;
  altitudeplot.zm_ymin = 0;
  altitudeplot.zm_ymax = 0;
  altitudeplot.zm_startx = 0;
  altitudeplot.zm_starty = 0;
  altitudeplot.zm_endx = 0;
  altitudeplot.zm_endy = 0;
  altitudeplot.lat = NULL;
  altitudeplot.lng = NULL;
  altitudeplot.xaxislabel = NULL;
  altitudeplot.yaxislabel = NULL;
  altitudeplot.linecolor[0] = 77;
  altitudeplot.linecolor[1] = 175;
  altitudeplot.linecolor[2] = 74;
  altitudeplot.units = English;
  altitudeplot.start_time = NULL;

  lapplot.ptype = LapPlot;
  lapplot.symbol = "⏺";
  lapplot.xmin = 0;
  lapplot.xmax = 0;
  lapplot.ymin = 0;
  lapplot.ymax = 0;
  lapplot.num_pts = 0;
  lapplot.x = NULL;
  lapplot.y = NULL;
  lapplot.vw_xmax = 0;
  lapplot.vw_ymin = 0;
  lapplot.vw_ymax = 0;
  lapplot.vw_xmin = 0;
  lapplot.zm_xmin = 0;
  lapplot.zm_xmax = 0;
  lapplot.zm_ymin = 0;
  lapplot.zm_ymax = 0;
  lapplot.zm_startx = 0;
  lapplot.zm_starty = 0;
  lapplot.zm_endx = 0;
  lapplot.zm_endy = 0;
  lapplot.lat = NULL;
  lapplot.lng = NULL;
  lapplot.xaxislabel = NULL;
  lapplot.yaxislabel = NULL;
  lapplot.linecolor[0] = 255;
  lapplot.linecolor[1] = 127;
  lapplot.linecolor[2] = 14;
  lapplot.units = English;
  lapplot.start_time = NULL;

  /* Bundle the data structures in an instance of AllData and
   * establish a pointer to it. */
  static AllData allData;
  allData.ppace = &paceplot;
  allData.pcadence = &cadenceplot;
  allData.pheart = &heartrateplot;
  allData.paltitude = &altitudeplot;
  allData.plap = &lapplot;
  allData.pd = &paceplot;
  allData.psd = &sess;
  AllData *pall = &allData;

  GtkBuilder *builder;
  GtkWidget *window;

  gtk_init (&argc, &argv);

  builder = gtk_builder_new_from_file ("siliconsneaker.glade");

  window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
  textbuffer1 =
      GTK_TEXT_BUFFER (gtk_builder_get_object (builder, "textbuffer1"));
  viewport = GTK_FRAME (gtk_builder_get_object (builder, "viewport"));
  da = GTK_DRAWING_AREA (gtk_builder_get_object (builder, "da"));
  rb_Pace = GTK_RADIO_BUTTON (gtk_builder_get_object (builder, "rb_Pace"));
  rb_Cadence =
      GTK_RADIO_BUTTON (gtk_builder_get_object (builder, "rb_Cadence"));
  rb_HeartRate =
      GTK_RADIO_BUTTON (gtk_builder_get_object (builder, "rb_HeartRate"));
  rb_Altitude =
      GTK_RADIO_BUTTON (gtk_builder_get_object (builder, "rb_Altitude"));
  rb_Splits = GTK_RADIO_BUTTON (gtk_builder_get_object (builder, "rb_Splits"));
  btnFileOpen =
      GTK_FILE_CHOOSER_BUTTON (gtk_builder_get_object (builder, "btnFileOpen"));
  btn_Zoom_In = GTK_BUTTON (gtk_builder_get_object (builder, "btn_Zoom_In"));
  btn_Zoom_Out = GTK_BUTTON (gtk_builder_get_object (builder, "btn_Zoom_Out"));
  cb_Units = GTK_COMBO_BOX_TEXT (gtk_builder_get_object (builder, "cb_Units"));
  sc_IdxPct = GTK_SCALE (gtk_builder_get_object (builder, "sc_IdxPct"));
  lbl_val = GTK_LABEL (gtk_builder_get_object (builder, "lbl_val"));

  /* Select a default chart to start. */
  default_chart ();

  /* Initialize a map and add it to a frame.
   */
  if (init_map () != 0)
    return 1;
  gtk_widget_show_all (window);

  /* Signals and events */
  gtk_builder_connect_signals (builder, NULL);
  gtk_widget_add_events (GTK_WIDGET (da), GDK_BUTTON_PRESS_MASK);
  gtk_widget_add_events (GTK_WIDGET (da), GDK_BUTTON_RELEASE_MASK);
  gtk_widget_add_events (GTK_WIDGET (da), GDK_POINTER_MOTION_MASK);
  g_signal_connect (GTK_DRAWING_AREA (da), "button-press-event",
                    G_CALLBACK (on_button_press), pall);
  g_signal_connect (GTK_DRAWING_AREA (da), "button-release-event",
                    G_CALLBACK (on_button_release), pall);
  g_signal_connect (GTK_DRAWING_AREA (da), "motion-notify-event",
                    G_CALLBACK (on_motion_notify), pall);
  g_signal_connect (GTK_DRAWING_AREA (da), "draw", G_CALLBACK (on_da_draw),
                    pall);
  g_signal_connect (GTK_RADIO_BUTTON (rb_Pace), "toggled",
                    G_CALLBACK (on_rb_pace), pall);
  g_signal_connect (GTK_RADIO_BUTTON (rb_Cadence), "toggled",
                    G_CALLBACK (on_rb_cadence), pall);
  g_signal_connect (GTK_RADIO_BUTTON (rb_HeartRate), "toggled",
                    G_CALLBACK (on_rb_heartrate), pall);
  g_signal_connect (GTK_RADIO_BUTTON (rb_Altitude), "toggled",
                    G_CALLBACK (on_rb_altitude), pall);
  g_signal_connect (GTK_RADIO_BUTTON (rb_Splits), "toggled",
                    G_CALLBACK (on_rb_splits), pall);
  g_signal_connect (GTK_BUTTON (btn_Zoom_In), "clicked", G_CALLBACK (zoom_in),
                    NULL);
  g_signal_connect (GTK_BUTTON (btn_Zoom_Out), "clicked", G_CALLBACK (zoom_out),
                    NULL);
  g_signal_connect (GTK_COMBO_BOX_TEXT (cb_Units), "changed",
                    G_CALLBACK (on_cb_units_changed), pall);
  g_signal_connect (GTK_FILE_CHOOSER (btnFileOpen), "file-set",
                    G_CALLBACK (on_btnFileOpen_file_set), pall);
  g_signal_connect (GTK_SCALE (sc_IdxPct), "value-changed",
                    G_CALLBACK (on_update_index), pall);
  g_signal_connect (GTK_WIDGET (window), "destroy",
                    G_CALLBACK (on_window_destroy), pall);

  /* Release the builder memory. */
  g_object_unref (builder);

  /* Process command line options. */
  // TODO This may not be available for Windows.
  int c;
  opterr = 0;
  while ((c = getopt (argc, argv, "mf:hv")) != -1)
    switch (c)
      {
      case 'm':
        /* Set combo box to metric */
        gtk_combo_box_set_active (GTK_COMBO_BOX (cb_Units), Metric);
        break;
      case 'f':
        fname = optarg;
        /* This seems sketchy to run before the input event loop
         * but seems to work.
         */
        reload_all (pall);
        break;
      case '?':
        if (optopt == 'f')
          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        else if (isprint (optopt))
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
        return 1;
      case 'h':
        fprintf (stdout, "Usage: %s [OPTION]...[FILENAME]\n", argv[0]);
        fprintf (stdout, " -f  open filename\n");
        fprintf (stdout, " -m  use metric units\n");
        fprintf (stdout, " -h  print program help\n");
        fprintf (stdout, " -v  print program version\n");
        return 0;
        break;
      case 'v':
        fprintf (stdout, "%s v%4.2f\n", argv[0], VERSION);
        return 0;
        break;
      default:
        abort ();
      }

  for (int index = optind; index < argc; index++)
    printf ("Non-option argument %s\n", argv[index]);

  gtk_widget_show (window);
  gtk_main ();

  return 0;
}
