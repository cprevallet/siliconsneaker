#include "tcx.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define ZERO_THRESHOLD 0.1

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
create_arrays_from_tcx_file (
    char *fname, float *prec_distance, float *prec_speed, float *prec_altitude,
    float *prec_cadence, float *prec_heartrate, float *prec_lat,
    float *prec_long, long int *num_pts, float *plap_total_distance,
    float *plap_start_position_lat, float *plap_start_position_long,
    float *plap_total_elapsed_time, long int *num_laps,
    time_t *sess_start_time,
    float* sess_start_position_lat,
    float* sess_start_position_long,
    float* sess_total_elapsed_time,
    float* sess_total_timer_time,
    float* sess_total_distance,
    float* sess_nec_latitude,
    float* sess_nec_longitude,
    float* sess_swc_latitude,
    float* sess_swc_longitude,
    float* sess_total_work,
    float* sess_total_moving_time,
    float* sess_average_lap_time,
    float* sess_total_calories,
    float* sess_avg_speed,
    float* sess_max_speed,
    float* sess_total_ascent,
    float* sess_total_descent,
    float* sess_avg_altitude,
    float* sess_max_altitude,
    float* sess_min_altitude,
    float* sess_avg_heartrate,
    float* sess_max_heartrate,
    float* sess_min_heartrate,
    float* sess_avg_cadence,
    float* sess_max_cadence,
    float* sess_avg_temperature,
    float* sess_max_temperature,
    float* sess_total_anaerobic_training_effect
    )
{
  activity_t *activity = NULL;
  lap_t *lap = NULL;
  track_t *track = NULL;
  trackpoint_t *trackpoint = NULL;

  /* Grab some memory to store the results and generate a pointer. */
  tcx_t *tcx = calloc (1, sizeof (tcx_t));
  if (parse_tcx_file (tcx, fname) == 0)
    {
      /* Calculate derived values. */
      calculate_summary (tcx);

      /* Calculate the actual size of the results. */
      //*sess_start_time = parseiso8601utc (activity->started_at);
      *num_pts = 0;
      *num_laps = 0;
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
                  *num_pts = *num_pts + track->num_trackpoints;
                  trackpoint = track->trackpoints;
                  while (trackpoint != NULL)
                    {
                      trackpoint = trackpoint->next;
                    }
                  track = track->next;
                }
              *num_laps = *num_laps + 1;
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
                          *sess_start_time = timestamp;
                          prec_distance[j] = (float)trackpoint->distance;
                          if (timestamp && prev_timestamp)
                            {
                              prec_speed[j] = ((float)trackpoint->distance
                                               - prev_distance)
                                              / ((double)timestamp
                                                 - (double)prev_timestamp);
                            }
                          else
                            {
                              if (j > 0)
                                {
                                  prec_speed[j] = prec_speed[j - 1];
                                }
                              else
                                {
                                  prec_speed[j] = 1.0; // dummy one up?
                                }
                            }
                          prec_altitude[j] = (float)trackpoint->elevation;
                          prec_cadence[j] = (float)trackpoint->cadence;
                          prec_heartrate[j] = (float)trackpoint->heart_rate;
                          prec_lat[j] = (float)trackpoint->latitude;
                          prec_long[j] = (float)trackpoint->longitude;
                          prev_timestamp = timestamp;
                          prev_distance = prec_distance[j];
                          j++;
                        }
                      else
                        {
                          *num_pts = *num_pts - 1;
                        }
                      trackpoint = trackpoint->next;
                    }
                  track = track->next;
                }
              plap_start_position_lat[k]
                  = lap->tracks[0].trackpoints[0].latitude;
              plap_start_position_long[k]
                  = lap->tracks[0].trackpoints[0].longitude;
              plap_total_elapsed_time[k] = lap->total_time;
              plap_total_distance[k] = lap->distance;
              k++;
              lap = lap->next;
            }
              /*
              printf("  started_at           : %s\n", activity->started_at);
              printf("  ended_at             : %s\n", activity->ended_at);
              printf("  total_time           : %.2f\n", activity->total_time);
              printf("  starting latitude    : %.6f\n", activity->start_point->latitude);
              printf("  starting longitude   : %.6f\n", activity->start_point->longitude);
              printf("  ending latitude      : %.6f\n", activity->end_point->latitude);
              printf("  ending longitude     : %.6f\n", activity->end_point->longitude);
              printf("  total_calories       : %d\n", activity->total_calories);
              printf("  total_distance       : %.2f\n", activity->total_distance);
              printf("  total_elevation_gain : %.2f\n", activity->total_elevation_gain);
              printf("  total_elevation_loss : %.2f\n", activity->total_elevation_loss);
              printf("  speed_average        : %.2f\n", activity->speed_average);
              printf("  speed_maximum        : %.2f\n", activity->speed_maximum);
              printf("  speed_minimum        : %.2f\n", activity->speed_minimum);
              printf("  elevation_maximum    : %.2f\n", activity->elevation_maximum);
              printf("  elevation_minimum    : %.2f\n", activity->elevation_minimum);
              printf("  cadence_average      : %d\n", activity->cadence_average);
              printf("  cadence_maximum      : %d\n", activity->cadence_maximum);
              printf("  cadence_minimum      : %d\n", activity->cadence_minimum);
              printf("  heart_rate_average   : %d\n", activity->heart_rate_average);
              printf("  heart_rate_minimum   : %d\n", activity->heart_rate_minimum);
              printf("  heart_rate_maximum   : %d\n", activity->heart_rate_maximum);
              */
              *sess_start_position_lat = activity->start_point->latitude;
              *sess_start_position_long = activity->start_point->longitude;
              *sess_total_elapsed_time = activity->total_time;
              //*sess_total_timer_time = ?;

              *sess_total_distance = activity->total_distance;
              /*
              *sess_nec_latitude = 
              *sess_nec_longitude = 
              *sess_swc_latitude = 
              *sess_swc_longitude = 
              *sess_total_work = 
              *sess_total_moving_time = 
              *sess_average_lap_time = 
              */
              *sess_total_calories = activity->total_calories;
              *sess_avg_speed = activity->speed_average;
              *sess_max_speed = activity->speed_maximum;
              *sess_total_ascent = activity->total_elevation_gain;
              *sess_total_descent = activity->total_elevation_loss;
              //*sess_avg_altitude = 
              *sess_max_altitude =  activity->elevation_maximum;
              *sess_min_altitude =  activity->elevation_minimum;
              *sess_avg_heartrate = activity->heart_rate_average;
              *sess_max_heartrate = activity->heart_rate_maximum;
              *sess_min_heartrate = activity->heart_rate_minimum;
              *sess_avg_cadence = activity->cadence_average;
              *sess_max_cadence = activity->cadence_maximum;
              /*
              *sess_avg_temperature = 
              *sess_max_temperature = 
              *sess_total_anaerobic_training_effect = 
              */

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
