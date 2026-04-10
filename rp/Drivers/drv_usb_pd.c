/**
 * @file      drv_usb_pd.c
 * @author    The OSLUV Project
 * @brief     Driver for power negociations IC controller (STUSB4500)
 * @schematic lamp_controller.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <hardware/watchdog.h>
#include <pico/stdlib.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "Drivers/drv_usb_pd.h"
#include "Drivers/drv_adc_volt.h"
#include "Drivers/drv_i2c.h"
#include "Drivers/drv_lamp.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define D_USB_PD_IC_ADDR_C 			      0x28

#define D_USB_PD_REG_TYPEC_STATUS_C       0x15
#define D_USB_PD_REG_PD_COMMAND_CTRL_C    0x1A
#define D_USB_PD_REG_TX_HEADER_LOW_C      0x51
#define D_USB_PD_REG_DPM_PDO_NUMB_C       0x70
#define D_USB_PD_REG_DPM_SNK_PDO1_0_C     0x85
#define D_USB_PD_REG_RDO_REG_STATUS_0_C   0x91
#define D_USB_PD_PDO_BASE_REG(pdo_num)    (D_USB_PD_REG_DPM_SNK_PDO1_0_C + (pdo_num * 4))

#define D_USB_PD_WRITE_LIT(addr, lit)     {uint8_t arr[] = lit; \
										   drv_usb_pd_write(addr, sizeof(arr), arr);}


/* Private typedef -----------------------------------------------------------*/

typedef struct {
	int mv;		/* Voltage in millivolts */
	int ma;		/* Minimum required current in milliamps */
} drv_usb_pd_candidate_t;


/* Global variables  ---------------------------------------------------------*/

extern bool g_mod_pow_hw_is_rev1_2_b;


/* Private variables  --------------------------------------------------------*/

static bool 	drv_usb_pd_is_trying_up_b    = false;
static uint32_t drv_drv_usb_pd_negotiated_mv = 5000;
static bool 	drv_usb_pd_was_connected_b   = false;

/* V1.2: worst-case I3 ballast requirements (from controller_pcb_v1.2_software_notes.md) */
static const drv_usb_pd_candidate_t drv_usb_pd_v12_candidates[] = {
	{ 20000, 1000 },
	{ 15000, 1400 },
	{ 12000, 1800 },
	{  9000, 2300 },
	{  5000, 4200 },
};
#define D_USB_PD_V12_CANDIDATE_COUNT_C  (sizeof(drv_usb_pd_v12_candidates) / \
										 sizeof(drv_usb_pd_v12_candidates[0]))

/* V1.1: single candidate — never negotiate above 12V (20V on 12V rail is dangerous) */
static const drv_usb_pd_candidate_t drv_usb_pd_v11_candidates[] = {
	{ 12000, 1800 },
};
#define D_USB_PD_V11_CANDIDATE_COUNT_C  (sizeof(drv_usb_pd_v11_candidates) / \
										 sizeof(drv_usb_pd_v11_candidates[0]))


/* Private function prototypes -----------------------------------------------*/

static inline int drv_usb_pd_read(uint8_t addr_l, uint32_t len, uint8_t* out);
static inline int drv_usb_pd_write(uint8_t addr_l, uint32_t len, uint8_t* value);
static inline void drv_usb_pd_software_reset(void);
static void drv_usb_pd_configure_pdo(usbpd_pdo_t* pdo, uint32_t mv, uint32_t ma);
static void drv_usb_pd_print_pdo(usbpd_pdo_t pdo);
static void drv_usb_pd_print_rdo(usbpd_rdo_t rdo);
//void dbgf(const char *fmt, ...);


/* Exported functions --------------------------------------------------------*/

void drv_usb_pd_init(void)
{
	drv_i2c_init();
}

/**
 * @brief Performs a software reset to external USB PD device
 * 
 */
void drv_usb_pd_reset(void)
{
	drv_usb_pd_software_reset();
}

/**
 * @brief Sets a PDO configuration from requested parameters
 * 
 * @param mv Millivolts to set
 * @param ma Milliamps to set
 */
void drv_usb_pd_set_pdo(uint32_t mv, uint32_t ma)
{
	usbpd_pdo_t pdo;
	
	pdo.u32 = 0;
	drv_usb_pd_configure_pdo(&pdo, mv, ma);
	
	drv_usb_pd_write(D_USB_PD_PDO_BASE_REG(1), sizeof(pdo), (uint8_t*)&pdo);
	D_USB_PD_WRITE_LIT(D_USB_PD_REG_DPM_PDO_NUMB_C, {0x02});
}

/**
 * @brief Sets voltage negotiation with step-down fallback
 *
 * When up=true: checks for USB-C connection, then steps through voltage
 * candidates from highest to lowest until one meets the minimum current
 * requirement. V1.1 only tries 12V (safety). V1.2 tries 20V→15V→12V→9V.
 * If no USB-C detected (barrel jack), skips negotiation entirely.
 *
 * When up=false: requests 5V only (PDO count = 1).
 *
 * @param up   true: negotiate higher voltage, false: 5V only
 */
void drv_usb_pd_negotiate(bool up)
{
	usbpd_pdo_t pdo;

	if (drv_lamp_get_commanded_power_level() != D_LAMP_PWR_OFF_C)
	{
		return;
	}

	if (!up)
	{
		printf("drv_usb_pd_negotiate: 5V only\n");
		D_USB_PD_WRITE_LIT(D_USB_PD_REG_DPM_PDO_NUMB_C, {0x01});
		drv_usb_pd_software_reset();
		drv_usb_pd_is_trying_up_b = false;
		drv_drv_usb_pd_negotiated_mv = 5000;
		return;
	}

	/* Check if USB-C is connected — if not, assume barrel jack */
	if (!drv_usb_pd_is_connected())
	{
		printf("drv_usb_pd_negotiate: no USB-C detected, configuring safe PDOs for hot-plug\n");

		/* Write board-appropriate PDOs so any USB hot-plug negotiates a safe
		 * voltage. Without this, STUSB4500 NVM defaults may request 20V,
		 * which is dangerous on V1.1 (20V on the 12V rail). */
		pdo.u32 = 0;
		if (g_mod_pow_hw_is_rev1_2_b)
		{
			drv_usb_pd_configure_pdo(&pdo, 20000, 1000);
		}
		else
		{
			drv_usb_pd_configure_pdo(&pdo, 12000, 1800);
		}
		drv_usb_pd_write(D_USB_PD_PDO_BASE_REG(1), sizeof(pdo), (uint8_t*)&pdo);
		D_USB_PD_WRITE_LIT(D_USB_PD_REG_DPM_PDO_NUMB_C, {0x02});
		drv_usb_pd_software_reset();

		drv_usb_pd_is_trying_up_b = false;
		drv_drv_usb_pd_negotiated_mv = 0;
		return;
	}

	/* Select candidate table based on board type */
	const drv_usb_pd_candidate_t *candidates;
	int count;

	if (g_mod_pow_hw_is_rev1_2_b)
	{
		candidates = drv_usb_pd_v12_candidates;
		count = D_USB_PD_V12_CANDIDATE_COUNT_C;
	}
	else
	{
		candidates = drv_usb_pd_v11_candidates;
		count = D_USB_PD_V11_CANDIDATE_COUNT_C;
	}

	/* Step down through candidates — try highest voltage first */
	for (int i = 0; i < count; i++)
	{
		int mv = candidates[i].mv;
		int ma = candidates[i].ma;

		printf("drv_usb_pd_negotiate: trying %dmV / %dmA min\n", mv, ma);

		pdo.u32 = 0;
		drv_usb_pd_configure_pdo(&pdo, mv, ma);
		drv_usb_pd_write(D_USB_PD_PDO_BASE_REG(1), sizeof(pdo), (uint8_t*)&pdo);
		D_USB_PD_WRITE_LIT(D_USB_PD_REG_DPM_PDO_NUMB_C, {0x02});
		drv_usb_pd_software_reset();

		sleep_ms(1000);
		watchdog_update();

		drv_adc_volt_update();
		int got_mv = (int)(g_adc_v_vbus * 1000);
		int got_ma = drv_usb_pd_get_negotiated_ma();

		printf("drv_usb_pd_negotiate: got %dmV/%dmA (need %dmV/%dmA)\n",
			   got_mv, got_ma, mv, ma);

		if (got_mv >= (mv - 1000) && got_ma >= ma)
		{
			printf("drv_usb_pd_negotiate: accepted %dmV / %dmA\n", got_mv, got_ma);
			drv_usb_pd_is_trying_up_b = true;
			drv_drv_usb_pd_negotiated_mv = mv;
			return;
		}
	}

	/* Nothing worked — fall back to 5V */
	printf("drv_usb_pd_negotiate: no candidate met requirements, falling back to 5V\n");
	D_USB_PD_WRITE_LIT(D_USB_PD_REG_DPM_PDO_NUMB_C, {0x01});
	drv_usb_pd_software_reset();
	drv_usb_pd_is_trying_up_b = false;
	drv_drv_usb_pd_negotiated_mv = 5000;
}

/**
 * @brief Checks if a USB-C cable is connected by reading TYPEC_STATUS register
 *
 * @return true   USB-C cable detected
 * @return false  No USB-C cable (barrel jack or unplugged)
 */
bool drv_usb_pd_is_connected(void)
{
	uint8_t c_status = 0;
	drv_usb_pd_read(D_USB_PD_REG_TYPEC_STATUS_C, 1, &c_status);
	return (c_status != 0);
}

/**
 * @brief Sets the baseline USB connection state for hot-plug edge detection.
 *        Call once after drv_usb_pd_negotiate() so that drv_usb_pd_update() doesn't
 *        falsely trigger on the first loop iteration.
 */
void drv_usb_pd_init_update(void)
{
	drv_usb_pd_was_connected_b = drv_usb_pd_is_connected();
}

/**
 * @brief Reads PD IC status to get if 12V has been negotiated
 *
 * @return true
 * @return false
 */
#if 0
bool drv_usb_pd_get_is_12v(void)
{
	uint8_t mv_from_status1 = 0;

	drv_usb_pd_read(0x21, 1, &mv_from_status1);

	// Register 0x21 reports voltage in 0.1V units (120 = 12.0V, 200 = 20.0V)
	// Check if we got at least what we asked for (12V for V1.1, 20V for V1.2)
	return (mv_from_status1 >= 120);
}

/**
 * @brief Returns whether we attempted high-voltage negotiation
 *
 * @return true   Negotiated above 5V (or attempted to)
 * @return false  5V only or barrel jack
 */
bool drv_usb_pd_get_is_trying_for_hv(void)
{
	return drv_usb_pd_is_trying_up_b;
}
#endif

/**
 * @brief Returns the voltage that was successfully negotiated (in mV)
 *
 * @return int  Negotiated voltage in millivolts (0 = barrel jack, 5000 = fallback)
 */
uint32_t drv_usb_pd_get_negotiated_mv(void)
{
	return drv_drv_usb_pd_negotiated_mv;
}

/**
 * @brief Returns negotiated operating current from the RDO
 *
 * @return int  Current in milliamps (0 if no USB-C connection)
 */
uint32_t drv_usb_pd_get_negotiated_ma(void)
{
	uint8_t c_status = 0;
	usbpd_rdo_t rdo = {0};

	drv_usb_pd_read(D_USB_PD_REG_TYPEC_STATUS_C, 1, &c_status);

	if (c_status == 0) 
	{
		return 0;
	}

	drv_usb_pd_read(D_USB_PD_REG_RDO_REG_STATUS_0_C, 4, (uint8_t*)&rdo);

	if (rdo.fixed.capability_mismatch)
	{
		printf("drv_usb_pd_get_negotiated_ma: capability mismatch — source can't meet current request\n");
		return 0;
	}

	return rdo.fixed.operating_current * 10;

}


/* Private functions ---------------------------------------------------------*/

/**
 * @brief Reads a register from PD device
 * 
 * @param addr_l Register's address to read from
 * @param len    Data length to read
 * @param out    Data read
 * @return int   Operation result (0: failed, 1: succeed)
 */
static inline int drv_usb_pd_read(uint8_t addr_l, uint32_t len, uint8_t* out)
{
	int e = 0;

	e = drv_i2c_wr_tmout_us(D_USB_PD_IC_ADDR_C, &addr_l, 1, true, 1000);
    if (e < 0)
    {
        printf("drv_usb_pd_read fail: addr: %d\n", e);

        return 1;
    }

	e = drv_i2c_rd_tmout_us(D_USB_PD_IC_ADDR_C, out, len, false, 1000);
    if (e < 0)
    {
        printf("drv_usb_pd_read fail: data: %d\n", e);

        return 1;
    }

    return 0;
}

/**
 * @brief Writes data to a PD's register
 * 
 * @param addr_l Register's address to write to
 * @param len    Data length to write
 * @param value  Data to write
 * @return int   Operation result (1: failed, 0: succeed)
 */
static inline int drv_usb_pd_write(uint8_t addr_l, uint32_t len, uint8_t* value)
{
    uint8_t buf[len + 1];
    int e = 0;

    buf[0] = addr_l;
    memcpy(buf+1, value, len);

	e = drv_i2c_wr_tmout_us(D_USB_PD_IC_ADDR_C, buf, len+1, false, 1000);
    if (e < 0)
    {
        printf("drv_usb_pd_write fail: %d\n", e);

        return 1;
    }

    return 0;
}

/**
 * @brief Issues a software reset command to PD IC
 * 
 */
static inline void drv_usb_pd_software_reset(void)
{
	D_USB_PD_WRITE_LIT(D_USB_PD_REG_TX_HEADER_LOW_C, {0x0D});
	D_USB_PD_WRITE_LIT(D_USB_PD_REG_PD_COMMAND_CTRL_C, {0x26});
}

/**
 * @brief Sets PDO configuration
 * 
 * @param pdo PDO configuration to set
 * @param mv  Voltage to set in millivolts
 * @param ma  Current to set int milliamps
 */
static void drv_usb_pd_configure_pdo(usbpd_pdo_t* pdo, uint32_t mv, uint32_t ma)
{
	pdo->fixed.voltage = mv / 50;
	pdo->fixed.operational_current = ma / 10;
}

/**
 * @brief Prints PDO to debug output
 * 
 * @param pdo PDO configuration to print
 */
static void drv_usb_pd_print_pdo(usbpd_pdo_t pdo)
{
	printf("-PDO=%08x\n", pdo.u32);
	printf("-.typetag = %d\n", pdo.fixed.typetag);
	printf("-.dual_role_power = %d\n", pdo.fixed.dual_role_power);
	printf("-.higher_capability = %d\n", pdo.fixed.higher_capability);
	printf("-.unconstrained_power = %d\n", pdo.fixed.unconstrained_power);
	printf("-.usb_comms_capable = %d\n", pdo.fixed.usb_comms_capable);
	printf("-.dual_role_data = %d\n", pdo.fixed.dual_role_data);
	printf("-.fast_role_swap_required_current = %d\n", pdo.fixed.fast_role_swap_required_current);
	printf("-.reserved = %d\n", pdo.fixed.reserved);
	printf("-.voltage = %d (%dmV)\n", pdo.fixed.voltage, pdo.fixed.voltage*50);
	printf("-.operational_current = %d (%dmA)\n", pdo.fixed.operational_current, pdo.fixed.operational_current*10);
}

/**
 * @brief Prints RDO to debug output
 * 
 * @param rdo RDO configuration to print
 */
static void drv_usb_pd_print_rdo(usbpd_rdo_t rdo)
{
	printf("-RDO=%08x\n", rdo.u32);
	printf("-.reserved_1 = %d\n", rdo.fixed.reserved_1);
	printf("-.object_position = %d\n", rdo.fixed.object_position);
	printf("-.giveback_flag = %d\n", rdo.fixed.giveback_flag);
	printf("-.capability_mismatch = %d\n", rdo.fixed.capability_mismatch);
	printf("-.usb_comms_capable = %d\n", rdo.fixed.usb_comms_capable);
	printf("-.no_usb_suspend = %d\n", rdo.fixed.no_usb_suspend);
	printf("-.unchuncked_ext_msg_supported = %d\n", rdo.fixed.unchuncked_ext_msg_supported);
	printf("-.reserved_2 = %d\n", rdo.fixed.reserved_2);
	printf("-.operating_current = %d (%dmA)\n", rdo.fixed.operating_current, rdo.fixed.operating_current*10);
	printf("-.max_operating_current = %d (%dmA)\n", rdo.fixed.max_operating_current, rdo.fixed.max_operating_current*10);
}

/*** END OF FILE ***/
