
void init_radar();
void init_radar_comms();
void update_radar();
void radar_debug();
int get_radar_distance_cm(); // or -1 if stale

struct __packed radar_report 
{
	uint8_t type;
	uint8_t _head;
	struct __packed
	{
		uint8_t target_state;
		uint16_t moving_target_distance_cm;
		uint8_t moving_target_energy;
		uint16_t stationary_target_distance_cm;
		uint8_t stationary_target_energy;
		uint16_t detection_distance_cm;
	} report;
	uint8_t _end;
	uint8_t _check;
};

struct __packed radar_message
{
	uint8_t preamble[4];
	uint16_t length;
	struct radar_report inner;
	uint8_t postamble[4];
};

struct radar_report* debug_get_radar_report();
int debug_get_radar_report_time();
