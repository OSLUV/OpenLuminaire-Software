#pragma once

void init_lamp();
void set_switched_12v(bool on);
void set_switched_24v(bool on);
bool get_switched_12v();
bool get_switched_24v();
void update_lamp();

enum lamp_type {
	LAMP_TYPE_UNKNOWN = 0,
	LAMP_TYPE_DIMMABLE,
	LAMP_TYPE_NONDIMMABLE
};

enum lamp_type get_lamp_type();

enum pwr_level {
	PWR_OFF = 0,
	PWR_20PCT,
	PWR_40PCT,
	PWR_70PCT,
	PWR_100PCT,
	NUM_REAL_PWR_SETTING,
	PWR_UNKNOWN = NUM_REAL_PWR_SETTING,
	NUM_ALL_PWR_SETTING
};

enum lamp_state {
	STATE_OFF = 0,
	STATE_STARTING,
	STATE_RUNNING,
	STATE_FULLPOWER_TEST,
	STATE_RESTRIKE_COOLDOWN_1,
	STATE_RESTRIKE_ATTEMPT_1,
	STATE_RESTRIKE_COOLDOWN_2,
	STATE_RESTRIKE_ATTEMPT_2,
	STATE_RESTRIKE_COOLDOWN_3,
	STATE_RESTRIKE_ATTEMPT_3,
	STATE_FAILED_OFF
};

bool request_lamp_power(enum pwr_level);
enum pwr_level get_lamp_requested_power();
enum pwr_level get_lamp_commanded_power();
bool get_lamp_reported_power(enum pwr_level* out); // returns false if unsure
int get_lamp_raw_freq();
void lamp_perform_type_test();
const char* pwr_level_str(enum pwr_level l);
enum lamp_state get_lamp_state();
const char* lamp_state_str(enum lamp_state s);
int get_lamp_state_elapsed_ms();
void lamp_toggle();
bool lamp_is_switchable();
bool lamp_is_on();