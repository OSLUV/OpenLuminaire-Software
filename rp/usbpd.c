#include <hardware/i2c.h>
#include <pico/stdlib.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "pins.h"
#include "usbpd.h"
#include "lamp.h"

#define USBPD_ADDR 0x28
#define USBPD_I2C I2C_INST

static inline int usbpd_read(uint8_t addr_l, int len, uint8_t* out)
{
	int e = 0;

    if ((e = i2c_write_timeout_us(USBPD_I2C, USBPD_ADDR, &addr_l, 1, true, 1000)) < 0)
    {
        printf("usbpd_read fail: addr: %d\n", e);
        return 1;
    }

    if ((e = i2c_read_timeout_us(USBPD_I2C, USBPD_ADDR, out, len, false, 1000)) < 0)
    {
        printf("usbpd_read fail: data: %d\n", e);
        return 1;
    }

    return 0;
}

static inline int usbpd_write(uint8_t addr_l, int len, uint8_t* value)
{
    uint8_t buf[len+1] = {};
    buf[0] = addr_l;
    memcpy(buf+1, value, len);
    int e = 0;

    if ((e = i2c_write_timeout_us(USBPD_I2C, USBPD_ADDR, buf, len+1, false, 1000)) < 0)
    {
        printf("usbpd_write fail: %d\n", e);
        return 1;
    }

    return 0;
}

#define USBPD_WRITE_LIT(addr, lit) {uint8_t arr[] = lit; usbpd_write(addr, sizeof(arr), arr);}

#define R_TYPEC_STATUS 0x15
#define R_PD_COMMAND_CTRL 0x1A
#define R_TX_HEADER_LOW 0x51
#define R_DPM_PDO_NUMB 0x70

void usbpd_software_reset()
{
	USBPD_WRITE_LIT(R_TX_HEADER_LOW, {0x0D});
	USBPD_WRITE_LIT(R_PD_COMMAND_CTRL, {0x26});
}

#define PDO_BASE_REG(pdo_0num) (0x85 + (pdo_0num * 4))
#define RDO_STATUS_REG 0x91

void usbpd_configure_pdo(usbpd_pdo_t* pdo, int mv, int ma)
{
	pdo->fixed.voltage = mv / 50;
	pdo->fixed.operational_current = ma / 10;
}

void print_pdo(usbpd_pdo_t pdo)
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

void print_rdo(usbpd_rdo_t rdo)
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

void dbgf(const char *fmt, ...);

void update_usbpd()
{
	// printf("\n\n\n\n\n\n\n");

	// uint8_t c_status = 0;

	// usbpd_read(R_TYPEC_STATUS, 1, &c_status);

	// // printf("R_TYPEC_STATUS: %02x\n", c_status);

	// uint8_t pdo_count = 0;

	// usbpd_read(R_DPM_PDO_NUMB, 1, &pdo_count);
	// pdo_count &= 0b111;

	// // printf("R_DPM_PDO_NUMB: %02x\n", pdo_count);

	// usbpd_pdo_t pdos[3];

	// for (int i = 0; i < 3; i++)
	// {
	// 	usbpd_pdo_t pdo = {0};
	// 	usbpd_read(PDO_BASE_REG(i), 4, (uint8_t*)&pdo);
	// 	pdos[i] = pdo;

	// 	if (i < pdo_count)
	// 	{
	// 		// printf("---PDO %d [0x%02x]---\n", i+1, PDO_BASE_REG(i));
	// 		// print_pdo(pdo);
	// 	}
	// }

	// usbpd_rdo_t rdo = {0};
	// usbpd_read(RDO_STATUS_REG, 4, (uint8_t*)&rdo);
	// // printf("---RDO 'status' [0x91]---\n");
	// // print_rdo(rdo);

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

bool trying_up = false;

void usbpd_negotiate(bool up)
{
	if (lamp_get_commanded_power_level() != LAMP_PWR_OFF_C) return;
	
	printf("Do negotiate...\n");

	usbpd_pdo_t pdo_12v;
	pdo_12v.u32 = 0;

	usbpd_configure_pdo(&pdo_12v, 12000, 2500);
	usbpd_write(PDO_BASE_REG(1), sizeof(pdo_12v), (uint8_t*)&pdo_12v);

	if (up)
	{
		USBPD_WRITE_LIT(R_DPM_PDO_NUMB, {0x02});
	}
	else
	{
		USBPD_WRITE_LIT(R_DPM_PDO_NUMB, {0x01});
	}

	usbpd_software_reset();

	trying_up = up;
}

bool usbpd_get_is_12v()
{
	uint8_t mv_from_status1 = 0;
	usbpd_read(0x21, 1, &mv_from_status1);

	return mv_from_status1 == 120;
}

bool usbpd_get_is_trying_for_12v()
{
	return trying_up;
}

int usbpd_get_negotiated_mA()
{
	uint8_t c_status = 0;

	usbpd_read(R_TYPEC_STATUS, 1, &c_status);

	if (c_status == 0) return 0;

	usbpd_rdo_t rdo = {0};
	usbpd_read(RDO_STATUS_REG, 4, (uint8_t*)&rdo);
	return rdo.fixed.operating_current*10;

}