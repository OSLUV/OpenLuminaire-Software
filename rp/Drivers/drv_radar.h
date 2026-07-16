/**
 * @file      drv_radar.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for mmWave Radar driver
 *  
 */

#ifndef _D_RADAR_H_
#define _D_RADAR_H_


/* Exported typedef ----------------------------------------------------------*/

/**
 * @struct D_RADAR_REPORT_T
 * @brief 
 * 
 */
typedef struct __packed 
{
	uint8_t type;
	uint8_t _head;
	struct __packed
	{
		uint8_t  target_state;
		uint16_t moving_target_distance_cm;
		uint8_t  moving_target_energy;
		uint16_t stationary_target_distance_cm;
		uint8_t  stationary_target_energy;
		uint16_t detection_distance_cm;
	} report;
	uint8_t _end;
	uint8_t _check;
} D_RADAR_REPORT_T;

/**
 * @struct D_RADAR_MESSAGE_T
 * @brief 
 * 
 */
typedef struct __packed
{
	union {
		uint8_t		  _u8[4];
		uint32_t	  _u32;
	} 				  preamble;
	uint16_t 		  length;
	D_RADAR_REPORT_T  inner;
	union {
		uint8_t		  _u8[4];
		uint32_t	  _u32;
	}				  postamble;
} D_RADAR_MESSAGE_T;


/* Exported functions prototypes ---------------------------------------------*/

void drv_radar_init(void);
void drv_radar_update(void);
int drv_radar_get_distance_cm(void); // or -1 if stale
int drv_radar_get_moving_target_cm(void);
int drv_radar_get_stationary_target_cm(void);

void drv_radar_debug(void);
D_RADAR_REPORT_T* drv_radar_debug_get_report(void);
int drv_radar_debug_get_report_time(void);


#endif /* _D_RADAR_H_ */

/*** END OF FILE ***/
