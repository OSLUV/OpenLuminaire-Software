/**
 * @file      radar.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for mmWave Radar driver
 *  
 */

#ifndef _D_RADAR_H_
#define _D_RADAR_H_


/* Exported typedef ----------------------------------------------------------*/

/**
 * @struct RADAR_REPORT_T
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
} RADAR_REPORT_T;

/**
 * @struct RADAR_MESSAGE_T
 * @brief 
 * 
 */
typedef struct __packed
{
	uint8_t 		preamble[4];
	uint16_t 		length;
	RADAR_REPORT_T  inner;
	uint8_t 		postamble[4];
} RADAR_MESSAGE_T;


/* Exported functions prototypes ---------------------------------------------*/

void radar_init(void);
void radar_update(void);
void radar_debug(void);
int radar_get_distance_cm(void); // or -1 if stale
int radar_get_moving_target_cm(void);
int radar_get_stationary_target_cm(void);

RADAR_REPORT_T* radar_debug_get_report(void);
int radar_debug_get_report_time(void);


#endif /* _D_RADAR_H_ */

/*** END OF FILE ***/
