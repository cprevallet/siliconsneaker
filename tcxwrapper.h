#include "tcx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#define ZERO_THRESHOLD 0.1

/*  parses only YYYY-MM-DDTHH:MM:SSZ */
time_t
parseiso8601utc (const char *date)
{
  struct tm tt = { 0 };
  double seconds;
  if (sscanf (date, "%04d-%02d-%02dT%02d:%02d:%lfZ", &tt.tm_year, &tt.tm_mon,
              &tt.tm_mday, &tt.tm_hour, &tt.tm_min, &seconds) != 6)
    return -1;
  tt.tm_sec = (int) seconds;
  tt.tm_mon -= 1;
  tt.tm_year -= 1900;
  //tt.tm_isdst = -1;
  tt.tm_isdst = 0;
  return mktime (&tt) - timezone;
}

int
create_arrays_from_tcx_file (char *fname,
                             float *prec_distance,
                             float *prec_speed,
                             float *prec_altitude,
                             float *prec_cadence,
                             float *prec_heartrate,
                             float *prec_lat,
                             float *prec_long,
                             long int *num_pts,
                             float *plap_total_distance,
                             float *plap_start_position_lat,
                             float *plap_start_position_long,
                             float *plap_total_elapsed_time,
                             long int *num_laps,
                             long int *sess_start_time)
{
  activity_t *activity = NULL;
  lap_t *lap = NULL;
  track_t *track = NULL;
  trackpoint_t *trackpoint = NULL;

  /* Grab some memory to store the results and generate a pointer. */
  tcx_t *tcx = calloc (1, sizeof (tcx_t));
  printf("%s", fname);
  if (parse_tcx_file (tcx, fname) == 0)
    {
      /* Calculate derived values. */
      calculate_summary (tcx);

      /* Calculate the actual size of the results. */
      //*sess_start_time = parseiso8601utc (activity->started_at);
      sess_start_time = 0;
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

                      /* Check for "bad" GPS readings.  You don't run off the coast of Africa. */
                      if ( !(trackpoint->latitude >= -ZERO_THRESHOLD && trackpoint->latitude <= ZERO_THRESHOLD) &&
                           !(trackpoint->longitude >= -ZERO_THRESHOLD && trackpoint->longitude <= ZERO_THRESHOLD) )
                      {
                      timestamp = parseiso8601utc (trackpoint->time);
                      prec_distance[j] = (float) trackpoint->distance;
                      if (timestamp && prev_timestamp)
                        {
                          prec_speed[j] =
                              ((float) trackpoint->distance - prev_distance) /
                              ((double) timestamp - (double) prev_timestamp);
                        }
                      else
                        { 
                          if (j > 0) {
                            prec_speed[j] = prec_speed[j-1];
                          } else {
                            prec_speed[j] = NAN;
                          }
                        }
                      prec_altitude[j] = (float) trackpoint->elevation;
                      prec_cadence[j] = (float) trackpoint->cadence;
                      prec_heartrate[j] = (float) trackpoint->heart_rate;
                      prec_lat[j] = (float) trackpoint->latitude;
                      prec_long[j] = (float) trackpoint->longitude;
                      prev_timestamp = timestamp;
                      prev_distance = prec_distance[j];
                      } else {
                        *num_pts = *num_pts - 1;
                      }
                      trackpoint = trackpoint->next;
                      j++;

                    }
                  track = track->next;
                }
              plap_start_position_lat[k] =
                  lap->tracks[0].trackpoints[0].latitude;
              plap_start_position_long[k] =
                  lap->tracks[0].trackpoints[0].longitude;
              plap_total_elapsed_time[k] = lap->total_time;
              plap_total_distance[k] = lap->distance;
              k++;
              lap = lap->next;
            }
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
