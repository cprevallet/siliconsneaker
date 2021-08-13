//////////////////////////////////////////////////////////////////////////////////
// Public Function Prototypes
//////////////////////////////////////////////////////////////////////////////////

int get_fit_records(char* fname, float* p_speed, float* p_distance, 
    float* p_lat, float* p_lng, float* p_cadence, float* p_heart_rate, 
    float* p_altitude, long int* p_time_stamp, int* p_num_recs,
    float* plap_start_lat, float* plap_start_lng, 
    float* plap_end_lat, float* plap_end_lng, 
    float* plap_total_distance, float* plap_total_calories,
    float* plap_total_elapsed_time, float* plap_total_timer_time,
    long int* plap_time_stamp, int* plap_num_recs );
