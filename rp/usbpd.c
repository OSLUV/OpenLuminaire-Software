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
	if (get_lamp_commanded_power() != PWR_OFF) return;
	
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

#include <stdbool.h>

// ----- STUSB4500: FTP/NVM registers -----
#define REG_RW_BUFFER   0x53
#define REG_FTP_KEY     0x95
#define REG_FTP_CTRL0   0x96
#define REG_FTP_CTRL1   0x97

// DPM live regs you already use
#ifndef R_DPM_PDO_NUMB
#define R_DPM_PDO_NUMB  0x70
#endif
#ifndef PDO_BASE_REG
#define PDO_BASE_REG(n) (0x85 + (4*(n)))   // n = 0..2 (PDO1..3)
#endif
#define R_TX_HEADER_LOW 0x51
#define R_PD_COMMAND    0x1A

// FTP opcodes (CTRL1[2:0])
#define OPC_READ_ARRAY     0x00
#define OPC_SHIFT_IN_PL    0x01
#define OPC_SHIFT_IN_ER    0x02
#define OPC_SHIFT_OUT_PL   0x03
#define OPC_SHIFT_OUT_ER   0x04
#define OPC_ERASE_ARRAY    0x05
#define OPC_PROG_WORD      0x06
#define OPC_SOFT_PROG      0x07

// Build CTRL1 with sector mask in [7:3]
static inline uint8_t ctrl1_with_ser(uint8_t ser_mask5, uint8_t opc) {
    return (uint8_t)((ser_mask5 << 3) | (opc & 0x07));
}
// Sector bit positions (5-bit mask): b0=S0, b1=S1, b2=S2, b3=S3, b4=S4
#define SER_S0 (1u<<0)
#define SER_S1 (1u<<1)
#define SER_S2 (1u<<2)
#define SER_S3 (1u<<3)
#define SER_S4 (1u<<4)

// ----- tiny I²C helpers -----
static inline void i2c_w8(uint8_t reg, uint8_t v){ usbpd_write(reg,1,&v); }
static inline void i2c_rN(uint8_t reg, uint8_t *buf, int len){ usbpd_read(reg,len,buf); }

// ----- FTP/NVM control -----
static void ftp_unlock_powerup(void){
    i2c_w8(REG_FTP_KEY, 0x47);      // unlock key
    i2c_w8(REG_FTP_CTRL0, 0x40);    // RST_N=1 (macro ready)
    sleep_ms(1);
}
static void ftp_req_and_wait(void){
    i2c_w8(REG_FTP_CTRL0, 0x50);    // REQ=1, RST_N=1
    sleep_ms(2);
    i2c_w8(REG_FTP_CTRL0, 0x40);    // REQ=0, RST_N=1
    sleep_ms(1);
}
static void ftp_powerdown_lock(void){
    i2c_w8(REG_FTP_CTRL0, 0x40);    // idle
    i2c_w8(REG_FTP_KEY,   0x00);    // lock
}

// Read 8-byte sector Sx into buf (x=0..4)
static void nvm_read_sector(uint8_t sx, uint8_t buf[8]){
    ftp_unlock_powerup();
    i2c_w8(REG_FTP_CTRL1, ctrl1_with_ser(1u<<sx, OPC_READ_ARRAY));
    ftp_req_and_wait();
    i2c_rN(REG_RW_BUFFER, buf, 8);
    ftp_powerdown_lock();
}
// Program 8-byte sector Sx (after targeted erase)
static void nvm_prog_sector(uint8_t sx, const uint8_t buf[8]){
    ftp_unlock_powerup();
    usbpd_write(REG_RW_BUFFER, 8, (uint8_t*)buf);     // stage payload
    i2c_w8(REG_FTP_CTRL1, ctrl1_with_ser(0, OPC_SHIFT_IN_PL));
    ftp_req_and_wait();

    i2c_w8(REG_FTP_CTRL1, ctrl1_with_ser(1u<<sx, OPC_PROG_WORD));
    ftp_req_and_wait();
    ftp_powerdown_lock();
}
// Targeted erase for a sector-mask (only S3/S4 here)
static void nvm_erase_mask(uint8_t ser_mask5){
    ftp_unlock_powerup();
    i2c_w8(REG_FTP_CTRL1, ctrl1_with_ser(ser_mask5, OPC_SHIFT_IN_ER)); // select sectors
    ftp_req_and_wait();

    i2c_w8(REG_FTP_CTRL1, ctrl1_with_ser(0, OPC_SOFT_PROG));  // per ST timing
    ftp_req_and_wait();
    i2c_w8(REG_FTP_CTRL1, ctrl1_with_ser(0, OPC_ERASE_ARRAY));
    ftp_req_and_wait();
    ftp_powerdown_lock();
}

// ---- decode helpers for S3/S4 (customer map) ----
// S3(D8..DF): DA[3:2] = SNK_DPM_SNK_PDO_NUMB (1..3 PDOs)
static inline uint8_t s3_get_pdo_count(const uint8_t s3[8]){
    return (uint8_t)((s3[2] >> 2) & 0x3);
}
static inline void s3_set_pdo_count_two(uint8_t s3[8]){
    s3[2] = (uint8_t)((s3[2] & ~(0x3<<2)) | (0x2<<2));
}

// S4(E0..E7): FLEX1_V (PDO2 V), FLEX2_V (PDO3 V), FLEX_I (common I)
// FLEX1_V = E1:E0[1:0] ; FLEX2_V = E3[7:6]:E2 ; FLEX_I = E4[3:0]:E3[5:0]
static void s4_get(uint16_t *v2, uint16_t *v3, uint16_t *i, const uint8_t s4[8]){
    uint16_t flex1_v = (uint16_t)(((uint16_t)s4[1] << 2) | (s4[0] & 0x03));              // 50 mV
    uint16_t flex2_v = (uint16_t)((((s4[3] >> 6) & 0x3) << 8) | s4[2]);                  // 50 mV
    uint16_t flex_i  = (uint16_t)((((s4[4] & 0x0F) << 6) | (s4[3] & 0x3F)));             // 10 mA
    *v2 = flex1_v; *v3 = flex2_v; *i = flex_i;
}
static void s4_set(uint8_t s4[8], uint16_t v_50mV, uint16_t i_10mA){
    // PDO2 voltage
    s4[0] = (uint8_t)((s4[0] & ~0x03) | (v_50mV & 0x03));
    s4[1] = (uint8_t)((v_50mV >> 2) & 0xFF);
    // PDO3 voltage + low bits of current
    s4[2] = (uint8_t)(v_50mV & 0xFF);
    s4[3] = (uint8_t)(((v_50mV >> 8) & 0x03) << 6) | (i_10mA & 0x3F);
    // high bits of current (keep other fields in E4 high nibble)
    s4[4] = (uint8_t)((s4[4] & 0xF0) | ((i_10mA >> 6) & 0x0F));
}

// ---- live PDO dump & debugger ----
static void dump_live_pd(void){
    uint8_t n=0; uint32_t p1=0,p2=0,p3=0;
    usbpd_read(R_DPM_PDO_NUMB,1,&n); n&=0x07;
    usbpd_read(PDO_BASE_REG(0),4,(uint8_t*)&p1);
    usbpd_read(PDO_BASE_REG(1),4,(uint8_t*)&p2);
    usbpd_read(PDO_BASE_REG(2),4,(uint8_t*)&p3);
    printf("LIVE: n=%u PDO1=%08lx PDO2=%08lx PDO3=%08lx\n", n,
        (unsigned long)p1,(unsigned long)p2,(unsigned long)p3);
}
void stusb4500_nvm_debug_dump(void){
    uint8_t s3[8], s4[8];
    nvm_read_sector(3,s3); nvm_read_sector(4,s4);
    uint16_t v2,v3,i10; s4_get(&v2,&v3,&i10,s4);
    printf("NVM S3: D8..DF  DA=0x%02X  -> PDO_NUMB=%u\n", s3[2], s3_get_pdo_count(s3));
    printf("NVM S4: E0..E7  FLEX: PDO2_V=%u(50mV) PDO3_V=%u(50mV) I=%u(10mA)\n", v2,v3,i10);
    dump_live_pd();
}

// ---- only write when needed; verify by read-back ----
int stusb4500_nvm_ensure_12v_profile(uint16_t mv, uint16_t mA, bool mirror_to_pdo3)
{
    const uint16_t wantV_50 = (uint16_t)(mv/50);   // e.g. 12000 -> 240
    const uint16_t wantI_10 = (uint16_t)(mA/10);   // e.g. 2500  -> 250

    // Read current NVM S3/S4
    uint8_t s3[8], s4[8];
    nvm_read_sector(3,s3); nvm_read_sector(4,s4);
    uint8_t s3_old = s3[2];
    uint16_t curV2,curV3,curI10; s4_get(&curV2,&curV3,&curI10,s4);

    bool need_s3 = (s3_get_pdo_count(s3) != 0x02);           // want 2 PDOs (5V + 12V)
    bool need_s4 = (curV2 != wantV_50) ||
                   (mirror_to_pdo3 && curV3 != wantV_50) ||
                   (curI10 < wantI_10);

    if (!need_s3 && !need_s4) {
        printf("NVM already 12V@>=I and PDO_NUMB=2; skipping write\n");
        return 0;
    }

    // Patch copies
    if (need_s3) s3_set_pdo_count_two(s3);
    if (need_s4) s4_set(s4, wantV_50, wantI_10);

    // Targeted erase S3|S4, then program & verify
    nvm_erase_mask(SER_S3 | SER_S4);
    nvm_prog_sector(3,s3);
    nvm_prog_sector(4,s4);

    // Verify
    uint8_t vs3[8], vs4[8]; nvm_read_sector(3,vs3); nvm_read_sector(4,vs4);
    uint16_t vv2,vv3,vi10; s4_get(&vv2,&vv3,&vi10,vs4);
    bool ok = (s3_get_pdo_count(vs3) == 0x02) &&
              (vv2 == wantV_50) && (!mirror_to_pdo3 || vv3 == wantV_50) &&
              (vi10 >= wantI_10);

    printf("NVM write %s. S3 DA: 0x%02X->0x%02X  S4 V2:%u V3:%u I:%u\n",
           ok?"OK":"FAILED", s3_old, vs3[2], vv2, vv3, vi10);

    // For THIS session, also update live PDOs + PD soft reset so you get 12V immediately.
    if (ok) {
        // Write live PDO2 (& PDO3 if requested)
        uint32_t live12 = 0;
        live12 |= ((uint32_t)wantV_50 & 0x3FF) << 10;
        live12 |= ((uint32_t)wantI_10 & 0x3FF);
        usbpd_write(PDO_BASE_REG(1), 4, (uint8_t*)&live12);
        if (mirror_to_pdo3) usbpd_write(PDO_BASE_REG(2), 4, (uint8_t*)&live12);
        uint8_t two = 0x02;
        usbpd_write(R_DPM_PDO_NUMB, 1, &two);

        uint8_t hdr=0x0D, go=0x26;
        usbpd_write(R_TX_HEADER_LOW,1,&hdr);
        usbpd_write(R_PD_COMMAND,   1,&go);
    }
    return ok ? 0 : -1;
}
