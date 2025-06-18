
typedef enum {
	BUTTON_UP = 1<<0,
	BUTTON_DOWN = 1<<1,
	BUTTON_LEFT = 1<<2,
	BUTTON_RIGHT = 1<<3,
	BUTTON_CENTER = 1<<4
} buttons_t;

void init_buttons();
void update_buttons();
extern buttons_t buttons_pressed, buttons_released, buttons_down, buttons_pulsed;
