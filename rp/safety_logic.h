#include "lamp.h"

int get_tilt_break(); // degrees
bool get_is_high_tilt();
void update_safety_logic();
char* get_safety_logic_state_desc();
bool get_safety_logic_enabled();
void set_safety_logic_enabled(bool);
void set_safety_logic_cap(LAMP_PWR_LEVEL_E);
void toggle_radar();