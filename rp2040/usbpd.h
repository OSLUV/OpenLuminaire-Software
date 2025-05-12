#include <stdint.h>
#include <assert.h>

void update_usbpd();
void usbpd_negotiate(bool up);
bool usbpd_get_is_12v();
bool usbpd_get_is_trying_for_12v();
int usbpd_get_negotiated_mA();

// Adapted from https://github.com/usb-c/STUSB4500/blob/master/Firmware/Project/Inc/USB_PD_defines_STUSB-GEN1S.h#L510 under MIT
typedef union
{
	uint32_t u32;

	struct
	{
		uint32_t operational_current :10;
		uint32_t voltage :10;
		uint32_t reserved :3;
		uint32_t fast_role_swap_required_current :2;  /* must be set to 0 in 2.0*/
		uint32_t dual_role_data :1;
		uint32_t usb_comms_capable :1;
		uint32_t unconstrained_power :1;
		uint32_t higher_capability :1;
		uint32_t dual_role_power :1;
		uint32_t typetag :2;
	} fixed;

	struct
	{
		uint32_t operating_current :10;
		uint32_t min_voltage :10;
		uint32_t max_voltage :10;
		uint32_t typetag :2; 
	} variable;

	struct
	{
		uint32_t operating_power :10;
		uint32_t min_voltage :10;
		uint32_t max_volage :10;
		uint32_t typetag :2; 
	} battery;

	struct
	{
		uint32_t bits :30;
		uint32_t typetag :2;
	} unknown;

} usbpd_pdo_t;

static_assert(sizeof(usbpd_pdo_t) == 4);

typedef union
{
	uint32_t u32;

	struct
	{
		uint32_t max_operating_current :10;
		uint32_t operating_current :10;
		uint32_t reserved_2 :3;
		uint32_t unchuncked_ext_msg_supported :1;
		uint32_t no_usb_suspend :1;
		uint32_t usb_comms_capable :1;
		uint32_t capability_mismatch :1;
		uint32_t giveback_flag :1;
		uint32_t object_position :3;
		uint32_t reserved_1 :1;
	} fixed;

} usbpd_rdo_t;

static_assert(sizeof(usbpd_rdo_t) == 4);
