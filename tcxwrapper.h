#include "tcx.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define ZERO_THRESHOLD 0.1

/* Return type for parse_fit_file */
typedef struct result_type
{
  long int failed;
  float *prec_distance;
  float *prec_speed;
  float *prec_altitude;
  float *prec_cadence;
  float *prec_heartrate;
  float *prec_lat;
  float *prec_long;
  float *plap_total_distance;
  float *plap_start_position_lat;
  float *plap_start_position_long;
  float *plap_total_elapsed_time;
  long int nRecs;
  long int nLaps;
  time_t time_zone_offset;
  time_t sess_timestamp;
  time_t sess_start_time;
  float sess_start_position_lat;
  float sess_start_position_long;
  float sess_total_elapsed_time;
  float sess_total_timer_time;
  float sess_total_distance;
  float sess_nec_latitude;
  float sess_nec_longitude;
  float sess_swc_latitude;
  float sess_swc_longitude;
  float sess_total_work;
  float sess_total_moving_time;
  float sess_average_lap_time;
  float sess_total_calories;
  float sess_avg_speed;
  float sess_max_speed;
  float sess_total_ascent;
  float sess_total_descent;
  float sess_avg_altitude;
  float sess_max_altitude;
  float sess_min_altitude;
  float sess_avg_heartrate;
  float sess_max_heartrate;
  float sess_min_heartrate;
  float sess_avg_cadence;
  float sess_max_cadence;
  float sess_avg_temperature;
  float sess_max_temperature;
  float sess_total_anaerobic_training_effect;
} result_type;

/*  parses only YYYY-MM-DDTHH:MM:SSZ */
time_t
parseiso8601utc (const char *date)
{
  struct tm tt = { 0 };
  double seconds;
  if (sscanf (date, "%04d-%02d-%02dT%02d:%02d:%lfZ", &tt.tm_year, &tt.tm_mon,
              &tt.tm_mday, &tt.tm_hour, &tt.tm_min, &seconds)
      != 6)
    return -1;
  tt.tm_sec = (int)seconds;
  tt.tm_mon -= 1;
  tt.tm_year -= 1900;
  // tt.tm_isdst = -1;
  tt.tm_isdst = 0;
  return mktime (&tt) - timezone;
}

int
create_arrays_from_tcx_file (char *fname, int NSIZE, int LSIZE, result_type *r)
{
  activity_t *activity = NULL;
  lap_t *lap = NULL;
  track_t *track = NULL;
  trackpoint_t *trackpoint = NULL;

  /* Grab some memory to store the results. */
  r->prec_distance = (float *)malloc (NSIZE * sizeof (float));
  r->prec_speed = (float *)malloc (NSIZE * sizeof (float));
  r->prec_altitude = (float *)malloc (NSIZE * sizeof (float));
  r->prec_cadence = (float *)malloc (NSIZE * sizeof (float));
  r->prec_heartrate = (float *)malloc (NSIZE * sizeof (float));
  r->prec_lat = (float *)malloc (NSIZE * sizeof (float));
  r->prec_long = (float *)malloc (NSIZE * sizeof (float));
  r->plap_total_distance = (float *)malloc (LSIZE * sizeof (float));
  r->plap_start_position_lat = (float *)malloc (LSIZE * sizeof (float));
  r->plap_start_position_long = (float *)malloc (LSIZE * sizeof (float));
  r->plap_total_elapsed_time = (float *)malloc (LSIZE * sizeof (float));
  r->nRecs = 0;
  r->nLaps = 0;
  r->time_zone_offset = 0;
  r->sess_timestamp = 0;
  r->sess_start_time = 0;
  r->sess_start_position_lat = NAN;
  r->sess_start_position_long = NAN;
  r->sess_total_elapsed_time = NAN;
  r->sess_total_timer_time = NAN;
  r->sess_total_distance = NAN;
  r->sess_nec_latitude = NAN;
  r->sess_nec_longitude = NAN;
  r->sess_swc_latitude = NAN;
  r->sess_swc_longitude = NAN;
  r->sess_total_work = NAN;
  r->sess_total_moving_time = NAN;
  r->sess_average_lap_time = NAN;
  r->sess_total_calories = NAN;
  r->sess_avg_speed = NAN;
  r->sess_max_speed = NAN;
  r->sess_total_ascent = NAN;
  r->sess_total_descent = NAN;
  r->sess_avg_altitude = NAN;
  r->sess_max_altitude = NAN;
  r->sess_min_altitude = NAN;
  r->sess_avg_heartrate = NAN;
  r->sess_max_heartrate = NAN;
  r->sess_min_heartrate = NAN;
  r->sess_avg_cadence = NAN;
  r->sess_max_cadence = NAN;
  r->sess_avg_temperature = NAN;
  r->sess_max_temperature = NAN;
  r->sess_total_anaerobic_training_effect = NAN;

  tcx_t *tcx = calloc (1, sizeof (tcx_t));

  if (parse_tcx_file (tcx, fname) == 0)
    {
      /* Calculate derived values. */
      calculate_summary (tcx);

      /* Calculate the actual size of the results. */
      activity = tcx->activities;
      /* Walk the linked list to get the number of points. */
      while (activity != NULL)
        {
          lap = activity->laps;
          while (lap != NULL)
            {
              track = lap->tracks;
              while (track != NULL)
                {
                  r->nRecs = r->nRecs + track->num_trackpoints;
                  trackpoint = track->trackpoints;
                  while (trackpoint != NULL)
                    {
                      trackpoint = trackpoint->next;
                    }
                  track = track->next;
                }
              r->nLaps = r->nLaps + 1;
              lap = lap->next;
            }
          activity = activity->next;
        }

      /* Convert the linked lists to arrays. */
      long int prev_timestamp = 0;
      float prev_distance = 0.0;
      time_t timestamp;
      activity = tcx->activities;
      int j = 0;
      int k = 0;
      while (activity != NULL)
        {
          lap = activity->laps;
          while (lap != NULL)
            {
              track = lap->tracks;
              while (track != NULL)
                {
                  trackpoint = track->trackpoints;
                  while (trackpoint != NULL)
                    {
                      /* Check for "bad" GPS readings.  You don't run off the
                       * coast of Africa. */
                      if (!(trackpoint->latitude >= -ZERO_THRESHOLD
                            && trackpoint->latitude <= ZERO_THRESHOLD)
                          && !(trackpoint->longitude >= -ZERO_THRESHOLD
                               && trackpoint->longitude <= ZERO_THRESHOLD))
                        {
                          timestamp = parseiso8601utc (trackpoint->time);
                          //r->sess_start_time = timestamp;
                          r->prec_distance[j] = (float)trackpoint->distance;
                          if (timestamp && prev_timestamp)
                            {
                              r->prec_speed[j] = ((float)trackpoint->distance
                                                  - prev_distance)
                                                 / ((double)timestamp
                                                    - (double)prev_timestamp);
                            }
                          else
                            {
                              if (j > 0)
                                {
                                  r->prec_speed[j] = r->prec_speed[j - 1];
                                }
                              else
                                {
                                  r->prec_speed[j] = 1.0; // dummy one up?
                                }
                            }
                          r->prec_altitude[j] = (float)trackpoint->elevation;
                          r->prec_cadence[j] = (float)trackpoint->cadence;
                          r->prec_heartrate[j] = (float)trackpoint->heart_rate;
                          r->prec_lat[j] = (float)trackpoint->latitude;
                          r->prec_long[j] = (float)trackpoint->longitude;
                          prev_timestamp = timestamp;
                          prev_distance = r->prec_distance[j];
                          j++;
                        }
                      else
                        {
                          r->nRecs = r->nRecs - 1;
                        }
                      trackpoint = trackpoint->next;
                    }
                  track = track->next;
                }
              r->plap_start_position_lat[k]
                  = lap->tracks[0].trackpoints[0].latitude;
              r->plap_start_position_long[k]
                  = lap->tracks[0].trackpoints[0].longitude;
              r->plap_total_elapsed_time[k] = lap->total_time;
              r->plap_total_distance[k] = lap->distance;
              k++;
              lap = lap->next;
            }
          r->sess_start_time = parseiso8601utc (activity->started_at);
          r->sess_timestamp = parseiso8601utc(activity->ended_at);
          r->sess_start_position_lat = activity->start_point->latitude;
          r->sess_start_position_long = activity->start_point->longitude;
          r->sess_total_elapsed_time = activity->total_time;
          r->sess_total_distance = activity->total_distance;
          r->sess_total_calories = activity->total_calories;
          r->sess_avg_speed = activity->speed_average;
          r->sess_max_speed = activity->speed_maximum;
          r->sess_total_ascent = activity->total_elevation_gain;
          r->sess_total_descent = activity->total_elevation_loss;
          r->sess_max_altitude = activity->elevation_maximum;
          r->sess_min_altitude = activity->elevation_minimum;
          r->sess_avg_heartrate = activity->heart_rate_average;
          r->sess_max_heartrate = activity->heart_rate_maximum;
          r->sess_min_heartrate = activity->heart_rate_minimum;
          r->sess_avg_cadence = activity->cadence_average;
          r->sess_max_cadence = activity->cadence_maximum;

          activity = activity->next;
        }
      /* Successful parse. */
      free (trackpoint);
      free (track);
      free (lap);
      free (activity);
      free (tcx);
      return 0;
    }
  else
    {
      /* Failed to parse. */
      return 1;
    }
}
