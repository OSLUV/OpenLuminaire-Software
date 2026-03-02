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


/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/

bool trying_up = false;


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
 * @brief Sets voltage negotiation
 * 
 * @param up Voltage to negotiate (0: 5V, 1: 12V)
 */
void usbpd_negotiate(bool up)
{
	usbpd_pdo_t pdo_12v;

	if (lamp_get_commanded_power_level() != LAMP_PWR_OFF_C) 					// Is lamp on ?
	{
		return;
	}
	
	printf("Do negotiate...\n");

	pdo_12v.u32 = 0;

	usbpd_configure_pdo(&pdo_12v, 12000, 2500); 								// Configuration for 12V negotiation
	usbpd_write(USBPD_PDO_BASE_REG(1), sizeof(pdo_12v), (uint8_t*)&pdo_12v);

	if (up)
	{
		USBPD_WRITE_LIT(USBPD_REG_DPM_PDO_NUMB_C, {0x02});                      // 12V negotitation
	}
	else
	{
		USBPD_WRITE_LIT(USBPD_REG_DPM_PDO_NUMB_C, {0x01});                      // 5V negotitation
	}

	usbpd_software_reset();

	trying_up = up;
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

	return (mv_from_status1 == 120);
}

/**
 * @brief Returns 12V negotiation setting
 * 
 * @return true
 * @return false
 */
bool usbpd_get_is_trying_for_12v(void)
{
	return trying_up;
}

/**
 * @brief Checks if PD IC has negotiated 10 mA
 * 
 * @return int (0: false, 1: true)
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
