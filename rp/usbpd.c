/**
 * @file      usbpd.c
 * @author    The OSLUV Project
 * @brief     Driver for power negociations IC controller (STUSB4500)
 * @schematic lamp_controller.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <hardware/i2c.h>
#include <pico/stdlib.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "pins.h"
#include "usbpd.h"
#include "lamp.h"
#include "board.h"
#include "sense.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define USBPD_I2C_PORT_C 		        I2C_INST
#define USBPD_ADDR_C 			        0x28

#define USBPD_REG_TYPEC_STATUS_C        0x15
#define USBPD_REG_PD_COMMAND_CTRL_C     0x1A
#define USBPD_REG_TX_HEADER_LOW_C       0x51
#define USBPD_REG_DPM_PDO_NUMB_C        0x70
#define USBPD_REG_DPM_SNK_PDO1_0_C      0x85
#define USBPD_REG_RDO_REG_STATUS_0_C    0x91
#define USBPD_PDO_BASE_REG(pdo_num)     (USBPD_REG_DPM_SNK_PDO1_0_C + (pdo_num * 4))

#define USBPD_WRITE_LIT(addr, lit)      {uint8_t arr[] = lit; usbpd_write(addr, sizeof(arr), arr);}


/* Private typedef -----------------------------------------------------------*/

typedef struct {
	int mv;		/* Voltage in millivolts */
	int ma;		/* Minimum required current in milliamps */
} usbpd_candidate_t;


/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/

static bool trying_up = false;
static int  negotiated_mv = 5000;
static bool usbpd_was_connected = false;

/* V1.2: worst-case I3 ballast requirements (from controller_pcb_v1.2_software_notes.md) */
static const usbpd_candidate_t usbpd_v12_candidates[] = {
	{ 20000, 1000 },
	{ 15000, 1400 },
	{ 12000, 1700 },
	{  9000, 2300 },
	{  5000, 4200 },
};
#define USBPD_V12_CANDIDATE_COUNT_C  (sizeof(usbpd_v12_candidates) / sizeof(usbpd_v12_candidates[0]))

/* V1.1: single candidate — never negotiate above 12V (20V on 12V rail is dangerous) */
static const usbpd_candidate_t usbpd_v11_candidates[] = {
	{ 12000, 2500 },
};
#define USBPD_V11_CANDIDATE_COUNT_C  (sizeof(usbpd_v11_candidates) / sizeof(usbpd_v11_candidates[0]))


/* Private function prototypes -----------------------------------------------*/

static inline int usbpd_read(uint8_t addr_l, int len, uint8_t* out);
static inline int usbpd_write(uint8_t addr_l, int len, uint8_t* value);
static void usbpd_software_reset(void);
static void usbpd_configure_pdo(usbpd_pdo_t* pdo, int mv, int ma);
static void usbpd_print_pdo(usbpd_pdo_t pdo);
static void usbpd_print_rdo(usbpd_rdo_t rdo);
//void dbgf(const char *fmt, ...);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief USB PD controller task handling
 * 
 */
void usbpd_update(void)
{
	bool is_connected = usbpd_is_connected();

	if (!usbpd_was_connected && is_connected)
	{
		/* USB just hot-plugged — write board-safe PDOs and reset so the
		 * STUSB4500 renegotiates with our values instead of NVM defaults.
		 * On V1.1, NVM defaults may request 20V which would damage the
		 * 12V rail. */
		printf("USB-PD: hot-plug detected, configuring safe PDOs\n");

		usbpd_pdo_t pdo;
		pdo.u32 = 0;
		if (board_is_v1_2())
			usbpd_configure_pdo(&pdo, 20000, 1000);
		else
			usbpd_configure_pdo(&pdo, 12000, 2500);
		usbpd_write(USBPD_PDO_BASE_REG(1), sizeof(pdo), (uint8_t*)&pdo);
		USBPD_WRITE_LIT(USBPD_REG_DPM_PDO_NUMB_C, {0x02});
		usbpd_software_reset();

		negotiated_mv = board_is_v1_2() ? 20000 : 12000;
		trying_up = true;
	}

	usbpd_was_connected = is_connected;

	// printf("\n\n\n\n\n\n\n");

	// uint8_t c_status = 0;

	// usbpd_read(USBPD_REG_TYPEC_STATUS_C, 1, &c_status);

	// // printf("USBPD_REG_TYPEC_STATUS_C: %02x\n", c_status);

	// uint8_t pdo_count = 0;

	// usbpd_read(USBPD_REG_DPM_PDO_NUMB_C, 1, &pdo_count);
	// pdo_count &= 0b111;

	// // printf("USBPD_REG_DPM_PDO_NUMB_C: %02x\n", pdo_count);

	// usbpd_pdo_t pdos[3];

	// for (int i = 0; i < 3; i++)
	// {
	// 	usbpd_pdo_t pdo = {0};
	// 	usbpd_read(USBPD_PDO_BASE_REG(i), 4, (uint8_t*)&pdo);
	// 	pdos[i] = pdo;

	// 	if (i < pdo_count)
	// 	{
	// 		// printf("---PDO %d [0x%02x]---\n", i+1, USBPD_PDO_BASE_REG(i));
	// 		// usbpd_print_pdo(pdo);
	// 	}
	// }

	// usbpd_rdo_t rdo = {0};
	// usbpd_read(USBPD_REG_RDO_REG_STATUS_0_C, 4, (uint8_t*)&rdo);
	// // printf("---RDO 'status' [0x91]---\n");
	// // usbpd_print_rdo(rdo);

	// // dbgf("USB: C:0x%02x %dPDOs enab\n", c_status, pdo_count);
	// int op = rdo.fixed.object_position;
	// // dbgf("USB: RDO.op=%d .cm=%d %.1fA/%.1fA\n", op, rdo.fixed.capability_mismatch, ((float)rdo.fixed.operating_current)*0.010, ((float)rdo.fixed.max_operating_current)*0.010);

	// op &= 0b11;

	// // This simply does not seem to work as described
	// // if (op == 0)
	// // {
	// // 	dbgf("USB: No RDO configured        ");
	// // }
	// // else if (op > 3)
	// // {
	// // 	dbgf("USB: RDO.op ???");
	// // }
	// // else
	// // {
	// // 	dbgf("USB: PDO@%d: %.1fV/%.1fA            ", op-1, ((float)pdos[op-1].fixed.voltage)*0.050, ((float)pdos[op-1].fixed.operational_current)*0.010);
	// // }

	// uint8_t mv_from_status1 = 0;
	// usbpd_read(0x21, 1, &mv_from_status1);

	// dbgf("USB: V[0x21]: %d/%.1fV\n", mv_from_status1, ((float)mv_from_status1)*0.100);
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
void usbpd_negotiate(bool up)
{
	usbpd_pdo_t pdo;

	if (lamp_get_commanded_power_level() != LAMP_PWR_OFF_C)
	{
		return;
	}

	if (!up)
	{
		printf("USB-PD negotiate: 5V only\n");
		USBPD_WRITE_LIT(USBPD_REG_DPM_PDO_NUMB_C, {0x01});
		usbpd_software_reset();
		trying_up = false;
		negotiated_mv = 5000;
		return;
	}

	/* Check if USB-C is connected — if not, assume barrel jack */
	if (!usbpd_is_connected())
	{
		printf("USB-PD: no USB-C detected, configuring safe PDOs for hot-plug\n");

		/* Write board-appropriate PDOs so any USB hot-plug negotiates a safe
		 * voltage. Without this, STUSB4500 NVM defaults may request 20V,
		 * which is dangerous on V1.1 (20V on the 12V rail). */
		pdo.u32 = 0;
		if (board_is_v1_2())
			usbpd_configure_pdo(&pdo, 20000, 1000);
		else
			usbpd_configure_pdo(&pdo, 12000, 2500);
		usbpd_write(USBPD_PDO_BASE_REG(1), sizeof(pdo), (uint8_t*)&pdo);
		USBPD_WRITE_LIT(USBPD_REG_DPM_PDO_NUMB_C, {0x02});
		usbpd_software_reset();

		trying_up = false;
		negotiated_mv = 0;
		return;
	}

	/* Select candidate table based on board type */
	const usbpd_candidate_t *candidates;
	int count;

	if (board_is_v1_2())
	{
		candidates = usbpd_v12_candidates;
		count = USBPD_V12_CANDIDATE_COUNT_C;
	}
	else
	{
		candidates = usbpd_v11_candidates;
		count = USBPD_V11_CANDIDATE_COUNT_C;
	}

	/* Step down through candidates — try highest voltage first */
	for (int i = 0; i < count; i++)
	{
		int mv = candidates[i].mv;
		int ma = candidates[i].ma;

		printf("USB-PD negotiate: trying %dmV / %dmA min\n", mv, ma);

		pdo.u32 = 0;
		usbpd_configure_pdo(&pdo, mv, ma);
		usbpd_write(USBPD_PDO_BASE_REG(1), sizeof(pdo), (uint8_t*)&pdo);
		USBPD_WRITE_LIT(USBPD_REG_DPM_PDO_NUMB_C, {0x02});
		usbpd_software_reset();

		sleep_ms(1000);

		sense_update();
		int got_mv = (int)(g_sense_vbus * 1000);
		int got_ma = usbpd_get_negotiated_mA();

		printf("USB-PD negotiate: got %dmV/%dmA (need %dmV/%dmA)\n",
			   got_mv, got_ma, mv, ma);

		if (got_mv >= (mv - 1000) && got_ma >= ma)
		{
			printf("USB-PD negotiate: accepted %dmV / %dmA\n", got_mv, got_ma);
			trying_up = true;
			negotiated_mv = mv;
			return;
		}
	}

	/* Nothing worked — fall back to 5V */
	printf("USB-PD negotiate: no candidate met requirements, falling back to 5V\n");
	USBPD_WRITE_LIT(USBPD_REG_DPM_PDO_NUMB_C, {0x01});
	usbpd_software_reset();
	trying_up = false;
	negotiated_mv = 5000;
}

/**
 * @brief Checks if a USB-C cable is connected by reading TYPEC_STATUS register
 *
 * @return true   USB-C cable detected
 * @return false  No USB-C cable (barrel jack or unplugged)
 */
bool usbpd_is_connected(void)
{
	uint8_t c_status = 0;
	usbpd_read(USBPD_REG_TYPEC_STATUS_C, 1, &c_status);
	return (c_status != 0);
}

/**
 * @brief Sets the baseline USB connection state for hot-plug edge detection.
 *        Call once after usbpd_negotiate() so that usbpd_update() doesn't
 *        falsely trigger on the first loop iteration.
 */
void usbpd_init_update(void)
{
	usbpd_was_connected = usbpd_is_connected();
}

/**
 * @brief Reads PD IC status to get if 12V has been negotiated
 *
 * @return true
 * @return false
 */
bool usbpd_get_is_12v(void)
{
	uint8_t mv_from_status1 = 0;

	usbpd_read(0x21, 1, &mv_from_status1);

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
bool usbpd_get_is_trying_for_hv(void)
{
	return trying_up;
}

/**
 * @brief Returns the voltage that was successfully negotiated (in mV)
 *
 * @return int  Negotiated voltage in millivolts (0 = barrel jack, 5000 = fallback)
 */
int usbpd_get_negotiated_mV(void)
{
	return negotiated_mv;
}

/**
 * @brief Returns negotiated operating current from the RDO
 *
 * @return int  Current in milliamps (0 if no USB-C connection)
 */
int usbpd_get_negotiated_mA(void)
{
	uint8_t c_status = 0;
	usbpd_rdo_t rdo = {0};

	usbpd_read(USBPD_REG_TYPEC_STATUS_C, 1, &c_status);

	if (c_status == 0) 
	{
		return 0;
	}

	usbpd_read(USBPD_REG_RDO_REG_STATUS_0_C, 4, (uint8_t*)&rdo);

	if (rdo.fixed.capability_mismatch)
	{
		printf("USB-PD: capability mismatch — source can't meet current request\n");
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
static inline int usbpd_read(uint8_t addr_l, int len, uint8_t* out)
{
	int e = 0;

	e = i2c_write_timeout_us(USBPD_I2C_PORT_C, USBPD_ADDR_C, &addr_l, 1, true, 1000);
    if (e < 0)
    {
        printf("usbpd_read fail: addr: %d\n", e);

        return 1;
    }

	e = i2c_read_timeout_us(USBPD_I2C_PORT_C, USBPD_ADDR_C, out, len, false, 1000);
    if (e < 0)
    {
        printf("usbpd_read fail: data: %d\n", e);

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
 * @return int   Operation result (0: failed, 1: succeed)
 */
static inline int usbpd_write(uint8_t addr_l, int len, uint8_t* value)
{
    uint8_t buf[len + 1];
    int e = 0;

    buf[0] = addr_l;
    memcpy(buf+1, value, len);

	e = i2c_write_timeout_us(USBPD_I2C_PORT_C, USBPD_ADDR_C, buf, len+1, false, 1000);
    if (e < 0)
    {
        printf("usbpd_write fail: %d\n", e);

        return 1;
    }

    return 0;
}

/**
 * @brief Issues a software reset command to PD IC
 * 
 */
static void usbpd_software_reset(void)
{
	USBPD_WRITE_LIT(USBPD_REG_TX_HEADER_LOW_C, {0x0D});
	USBPD_WRITE_LIT(USBPD_REG_PD_COMMAND_CTRL_C, {0x26});
}

/**
 * @brief Sets PDO configuration
 * 
 * @param pdo PDO configuration to set
 * @param mv  Voltage to set in millivolts
 * @param ma  Current to set int milliamps
 */
static void usbpd_configure_pdo(usbpd_pdo_t* pdo, int mv, int ma)
{
	pdo->fixed.voltage = mv / 50;
	pdo->fixed.operational_current = ma / 10;
}

/**
 * @brief Prints PDO to debug output
 * 
 * @param pdo PDO configuration to print
 */
static void usbpd_print_pdo(usbpd_pdo_t pdo)
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
static void usbpd_print_rdo(usbpd_rdo_t rdo)
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
