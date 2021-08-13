//
// Comment FIT_CONVERT_CHECK_FILE_HDR_DATA_TYPE in fit_config.h
// Uncomment FIT_USE_STDINT_H in fit_config.h 
// See: https://www.thisisant.com/forum/view/viewthread/6444
// Compile gcc decode.c ../../fit.c  ../../fit_convert.c  ../../fit_crc.c   ../../fit_product.c   -I../../ -o decode -lm
//

////////////////////////////////////////////////////////////////////////////////
// The following FIT Protocol software provided may be used with FIT protocol
// devices only and remains the copyrighted property of Dynastream Innovations Inc.
// The software is being provided on an "as-is" basis and as an accommodation,
// and therefore all warranties, representations, or guarantees of any kind
// (whether express, implied or statutory) including, without limitation,
// warranties of merchantability, non-infringement, or fitness for a particular
// purpose, are specifically disclaimed.
//
// Copyright 2008 Dynastream Innovations Inc.
////////////////////////////////////////////////////////////////////////////////

#define _CRT_SECURE_NO_WARNINGS

#include <time.h>
#include "math.h"
#include "stdio.h"
#include "string.h"

#include "./fit/fit_convert.h"

int get_fit_records(char* fname, float* p_speed, float* p_distance, 
    float* p_lat, float* p_lng, float* p_cadence, float* p_heart_rate, 
    float* p_altitude, long int* p_time_stamp, int* p_num_recs,
    float* plap_start_lat, float* plap_start_lng, 
    float* plap_end_lat, float* plap_end_lng, 
    float* plap_total_distance, float* plap_total_calories,
    float* plap_total_elapsed_time, float* plap_total_timer_time,
    long int* plap_time_stamp, int* plap_num_recs )
{
   FILE *file;
   FIT_UINT8 buf[8];
   FIT_CONVERT_RETURN convert_return = FIT_CONVERT_CONTINUE;
   FIT_UINT32 buf_size;
   //FIT_UINT32 mesg_index = 0;
   #if defined(FIT_CONVERT_MULTI_THREAD)
      FIT_CONVERT_STATE state;
   #endif

   /*   
   if (argc < 2)
   {
      printf("usage: decode.exe <fit file>");
      return FIT_FALSE;
   }

   printf("Testing file conversion using %s file...\n", argv[1]);
   */

   #if defined(FIT_CONVERT_MULTI_THREAD)
      FitConvert_Init(&state, FIT_TRUE);
   #else
      FitConvert_Init(FIT_TRUE);
   #endif

   if((file = fopen(fname, "rb")) == NULL)
   {
      printf("Error opening file %s.\n", fname);
      return FIT_FALSE;
   }

   while(!feof(file) && (convert_return == FIT_CONVERT_CONTINUE))
   {
      for(buf_size=0;(buf_size < sizeof(buf)) && !feof(file); buf_size++)
      {
         buf[buf_size] = (FIT_UINT8)getc(file);
      }

      do
      {
         #if defined(FIT_CONVERT_MULTI_THREAD)
            convert_return = FitConvert_Read(&state, buf, buf_size);
         #else
            convert_return = FitConvert_Read(buf, buf_size);
         #endif

         switch (convert_return)
         {
            case FIT_CONVERT_MESSAGE_AVAILABLE:
            {
               #if defined(FIT_CONVERT_MULTI_THREAD)
                  const FIT_UINT8 *mesg = FitConvert_GetMessageData(&state);
                  FIT_UINT16 mesg_num = FitConvert_GetMessageNumber(&state);
               #else
                  const FIT_UINT8 *mesg = FitConvert_GetMessageData();
                  FIT_UINT16 mesg_num = FitConvert_GetMessageNumber();
               #endif
//printf("Mesg %d (%d) - ", mesg_index++, mesg_num);

               switch(mesg_num)
               {
                  case FIT_MESG_NUM_FILE_ID:
                  {
                     //const FIT_FILE_ID_MESG *id = (FIT_FILE_ID_MESG *) mesg;
                     //printf("File ID: type=%u, number=%u\n", id->type, id->number);
                     break;
                  }

                  case FIT_MESG_NUM_USER_PROFILE:
                  {
                     //const FIT_USER_PROFILE_MESG *user_profile = (FIT_USER_PROFILE_MESG *) mesg;
                     //printf("User Profile: weight=%0.1fkg\n", user_profile->weight / 10.0f);
                     break;
                  }

                  case FIT_MESG_NUM_ACTIVITY:
                  {  /*
                     const FIT_ACTIVITY_MESG *activity = (FIT_ACTIVITY_MESG *) mesg;
                     printf("Activity: timestamp=%u, type=%u, event=%u, event_type=%u, num_sessions=%u\n", activity->timestamp, activity->type, activity->event, activity->event_type, activity->num_sessions);
                     {
                        FIT_ACTIVITY_MESG old_mesg;
                        old_mesg.num_sessions = 1;
                        #if defined(FIT_CONVERT_MULTI_THREAD)
                           FitConvert_RestoreFields(&state, &old_mesg);
                        #else
                           FitConvert_RestoreFields(&old_mesg);
                        #endif
                        //printf("Restored num_sessions=1 - Activity: timestamp=%u, type=%u, event=%u, event_type=%u, num_sessions=%u\n", activity->timestamp, activity->type, activity->event, activity->event_type, activity->num_sessions);
                     }
                     */
                     break;
                  }

                  case FIT_MESG_NUM_SESSION:
                  {
                     //const FIT_SESSION_MESG *session = (FIT_SESSION_MESG *) mesg;
                     //printf("Session: timestamp=%u\n", session->timestamp);
                     break;
                  }

                  case FIT_MESG_NUM_LAP:
                  {
                     const FIT_LAP_MESG *lap = (FIT_LAP_MESG *) mesg;
                     //printf("Lap: timestamp=%u\n", lap->timestamp);
                     *plap_time_stamp = lap->timestamp + 631065600;
                     plap_time_stamp++;

                     *plap_start_lat = (float) (lap->start_position_lat * (180.0/pow(2,31)));
                     plap_start_lat++; 
                     *plap_start_lng = (float) (lap->start_position_long * (180.0/pow(2,31)));
                     plap_start_lng++; 
                     *plap_end_lat = (float) (lap->end_position_lat * (180.0/pow(2,31)));
                     plap_end_lat++; 
                     *plap_end_lng = (float) (lap->end_position_long * (180.0/pow(2,31)));
                     plap_end_lng++; 

                     *plap_total_distance = (float) (lap->total_distance/100.0);
                     plap_total_distance++;
                     *plap_total_calories = (float) (lap->total_calories);
                     plap_total_calories++;
                     *plap_total_elapsed_time = (float) (lap->total_elapsed_time / 1000.0 ); 
                     plap_total_elapsed_time++; 
                     *plap_total_timer_time = (float) (lap->total_timer_time /  1000.0 ) ;
                     plap_total_timer_time++;
                     *plap_num_recs = *plap_num_recs + 1;
                     break;
                  }

                  case FIT_MESG_NUM_RECORD:
                  {
                     const FIT_RECORD_MESG *record = (FIT_RECORD_MESG *) mesg;

                     /* While the following may not be useful to most
                        people, in the event that you need to convert FIT
                        file timestamps produced by a Garmin product, this 
                        is a time-saver. For whatever reason, Garmin created
                        their own epoch, which began at midnight on
                        Sunday, Dec 31st, 1989 (the year of Garmin’s founding).
                        And as far as I know, all Garmin products log data using
                        this date as a reference.
                        The conversion is simple, just add 631065600 seconds to
                        a Garmin numeric timestamp, then you have the number of
                        seconds since the 1970 Unix epoch. */

                     //time_t unix_time = record->timestamp + 631065600;
                     //printf("Record: timestamp=%lu unix_secs", unix_time);
                     *p_time_stamp = record->timestamp + 631065600;
                     p_time_stamp++;
                     //printf(", position_long = %0.8f", record->position_long * (180.0/pow(2,31)));
                     *p_lng = (float) (record->position_long * (180.0/pow(2,31)));
                     p_lng++;
                     //printf(", position_lat = %0.8f", record->position_lat * (180.0/pow(2,31)));
                     *p_lat = (float) (record->position_lat * (180.0/pow(2,31)));
                     p_lat++;
                     //printf("record = %0.3fm/s", record->speed/1000.0);
                     *p_speed = (float) (record->speed/1000.0);
                     p_speed++;
                     // altitude, meters offset of 500
                     // printf("record = %4.0fm \n", (float)record->speed / 5.0 - 500.0);
                     *p_altitude = (float)record->altitude/5.0 - 500.0;
                     p_altitude++;
                     //printf(", distance = %0.3fm", record->distance/100.0);
                     *p_distance = (float) (record->distance/100.0);
                     p_distance++;
                     //printf(", calories = %dkcal", record->calories);
                     //printf(", cadence = %d", record->cadence);
                     *p_cadence = (float) (record->cadence);
                     p_cadence++;
                     //printf(", heart_rate = %d", record->heart_rate);
                     *p_heart_rate = (float) (record->heart_rate);
                     p_heart_rate++;
                     *p_num_recs = *p_num_recs + 1;
                     break;
                  }

                  case FIT_MESG_NUM_EVENT:
                  {
                     //const FIT_EVENT_MESG *event = (FIT_EVENT_MESG *) mesg;
                     //printf("Event: timestamp=%u\n", event->timestamp);
                     break;
                  }

                  case FIT_MESG_NUM_DEVICE_INFO:
                  {
                     //const FIT_DEVICE_INFO_MESG *device_info = (FIT_DEVICE_INFO_MESG *) mesg;
                     break;
                  }

                  default:
                    //printf("Unknown\n");
                    break;
               }
               break;
            }

            default:
               break;
         }
      } while (convert_return == FIT_CONVERT_MESSAGE_AVAILABLE);
   }

   if (convert_return == FIT_CONVERT_ERROR)
   {
      printf("Error decoding file.\n");
      fclose(file);
      return 1;
   }

   if (convert_return == FIT_CONVERT_CONTINUE)
   { 
      
      printf("Unexpected end of file.\n");
      fclose(file);
      return 1;
   }

   if (convert_return == FIT_CONVERT_DATA_TYPE_NOT_SUPPORTED)
   {
      printf("File is not FIT.\n");
      fclose(file);
      return 1;
   }

   if (convert_return == FIT_CONVERT_PROTOCOL_VERSION_NOT_SUPPORTED)
   {
      printf("Protocol version not supported.\n");
      fclose(file);
      return 1;
   }

   if (convert_return == FIT_CONVERT_END_OF_FILE)
      //printf("File converted successfully.\n");

   fclose(file);

   return 100;
}
