/**
 * @file      mod_lamp_ctrl.c
 * @author    The OSLUV Project
 * @brief     Lamp control module
 * @schematic lamp_controller.SchDoc
 * @schematic power.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <pico/stdlib.h>
#include "pico/time.h"
#include "Modules/mod_lamp_ctrl.h"
#include "Modules/system.h"
#include "Drivers/drv_lamp.h"
#include "Drivers/drv_config.h"
#include "Drivers/drv_debug.h"


/* Private typedef -----------------------------------------------------------*/

typedef struct {
	int pwm;
	int power;
} M_LAMP_PWR_CTL_T;


/* Private define ------------------------------------------------------------*/

#define M_LAMP_DBG_ID_STR_C        	  		"mod_lamp_ctrl       "
#define M_LAMP_DBG_PRINTF(...)    	  		debug_print_f(__VA_ARGS__)
#define M_LAMP_DBG_PRINT_TXT(...)    		debug_print_mod_f(M_LAMP_DBG_ID_STR_C, __VA_ARGS__)
#define M_LAMP_DBG_PRINT_ERR(...)			debug_print_err(M_LAMP_DBG_ID_STR_C, __VA_ARGS__)
#define M_LAMP_DBG_PRINT_WRN(...)			debug_print_warn(M_LAMP_DBG_ID_STR_C, __VA_ARGS__)
#define M_LAMP_DBG_PRINT_OK(...)			debug_print_ok(M_LAMP_DBG_ID_STR_C, __VA_ARGS__)

#define M_LAMP_RESTRIKE_COOLDOWN_MS_TIME_C 	5000
#define M_LAMP_START_MS_TIME_C 				10000


/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/

const int 					M_LAMP_STEPCOUNT_DIMMING_C   = 100;

const M_LAMP_PWR_CTL_T 		mod_lamp_pwr_settings[M_LAMP_PWR_MAX_SETTINGS_C] = {
								[M_LAMP_PWR_OFF_C]    = {0,     0},
								[M_LAMP_PWR_20PCT_C]  = {100,  20},
								[M_LAMP_PWR_40PCT_C]  = {83,   40},
								[M_LAMP_PWR_70PCT_C]  = {50,   70},
								[M_LAMP_PWR_100PCT_C] = {0,   100},
							};

static uint64_t 			mod_lamp_state_transition_time = 0;

static M_LAMP_PWR_LEVEL_E	mod_lamp_req_pwr_level;

static absolute_time_t  	mod_lamp_tmout;
static absolute_time_t  	mod_lamp_delay_tmout;
static absolute_time_t  	mod_lamp_pw_mon_dly_tmout;


/* Callback prototypes -------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static inline void mod_lamp_go_to_state(M_LAMP_STATE_E state);
static inline uint64_t mod_lamp_get_state_elapsed_ms(void);
static inline bool mod_lamp_is_warming(void);
static void mod_lamp_request_power_level(void);
static void mod_lamp_power_source_monitor(void);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief 	Lamp control initialization procedure
 * 
 * @return 	void 
 */
void mod_lamp_init(void)
{
	g_sys_stt.lamp_type 			= drv_cfg_get_factory_lamp_type();
	g_sys_ctl.lamp_req_pwr_level 	= M_LAMP_PWR_OFF_C;
	g_sys_stt.lamp_cmd_power_level	= M_LAMP_PWR_OFF_C;
	g_sys_stt.lamp_power_level		= M_LAMP_PWR_UNKNOWN_C;
	g_sys_stt.lamp_state 			= M_LAMP_STATE_OFF_C;

	mod_lamp_req_pwr_level 			= M_LAMP_PWR_OFF_C;

	M_LAMP_DBG_PRINT_TXT("Determined type from flash: %d", 
						 mod_lamp_get_lamp_type_str(g_sys_stt.lamp_type));

	drv_lamp_init();
}

/**
 * @brief 	Lamp control handler
 * 
 * @return 	void 
 */
void mod_lamp_ctrl_handler(void)
{
	static M_LAMP_PWR_LEVEL_E commanded_power_level = M_LAMP_PWR_OFF_C;
	static uint64_t last_update = 0;
	uint64_t now = time_us_64();

	mod_lamp_request_power_level();

	if ((now - last_update) > (1000*1000))
	{
		g_sys_stt.lamp_latched_freq_hz = drv_lamp_get_status_evts_count();
		last_update = now;

		g_sys_stt.lamp_power_level = M_LAMP_PWR_UNKNOWN_C;

		if (g_sys_stt.lamp_type == M_LAMP_TYPE_NON_DIMMABLE_C)
		{
			if (drv_lamp_is_enabled() && !drv_lamp_is_status_on())
			{
				g_sys_stt.lamp_power_level = M_LAMP_PWR_100PCT_C;
			}
			else
			{
				g_sys_stt.lamp_power_level = M_LAMP_PWR_OFF_C;
			}
		}
		else // Includes unknown case because this is used while testing
		{
			if (commanded_power_level == M_LAMP_PWR_OFF_C) 
			{
				g_sys_stt.lamp_power_level = M_LAMP_PWR_OFF_C;
			}
			else if (g_sys_stt.lamp_latched_freq_hz < 100)
			{
				if ((commanded_power_level != M_LAMP_PWR_OFF_C) && 
					(!drv_lamp_is_status_on())) 
				{
					g_sys_stt.lamp_power_level = M_LAMP_PWR_100PCT_C;
				}
				else if (drv_lamp_is_status_on()) 
				{
					g_sys_stt.lamp_power_level = M_LAMP_PWR_OFF_C;
				}
			}
			else if ((g_sys_stt.lamp_latched_freq_hz > 900) && 
					 (g_sys_stt.lamp_latched_freq_hz < 1100))
			{
				g_sys_stt.lamp_power_level = M_LAMP_PWR_70PCT_C;
			}
			else if ((g_sys_stt.lamp_latched_freq_hz > 400) &&
					 (g_sys_stt.lamp_latched_freq_hz < 600))
			{
				g_sys_stt.lamp_power_level = M_LAMP_PWR_40PCT_C;
			}
			else if ((g_sys_stt.lamp_latched_freq_hz > 150) &&
					 (g_sys_stt.lamp_latched_freq_hz < 250))
			{
				g_sys_stt.lamp_power_level = M_LAMP_PWR_20PCT_C;
			}
		}
	}

	uint64_t elapsed_ms_in_state = (time_us_64() - mod_lamp_state_transition_time) / 1000;

	switch (g_sys_stt.lamp_state)
	{
		case M_LAMP_STATE_STARTING_C:
			commanded_power_level = M_LAMP_PWR_100PCT_C;

			if (g_sys_stt.lamp_power_level == M_LAMP_PWR_100PCT_C)
			{
				mod_lamp_go_to_state(M_LAMP_STATE_RUNNING_C);
			}

			if (elapsed_ms_in_state > M_LAMP_START_MS_TIME_C)
			{
				mod_lamp_go_to_state(M_LAMP_STATE_RESTRIKE_COOLDOWN_1_C);
			}

			if (mod_lamp_req_pwr_level == M_LAMP_PWR_OFF_C)
			{
				mod_lamp_go_to_state(M_LAMP_PWR_OFF_C);
			}

			// Don't go to off once starting to avoid short cycling
		break;

		case M_LAMP_STATE_RUNNING_C:
			commanded_power_level = mod_lamp_req_pwr_level;

			if ((g_sys_stt.lamp_type == M_LAMP_TYPE_DIMMABLE_C) && 
				(elapsed_ms_in_state > (2*60*60*1000)))
			{
				M_LAMP_DBG_PRINT_TXT("Initiate full-power test");
				mod_lamp_go_to_state(M_LAMP_STATE_FULLPOWER_TEST_C);
			}

			if ((g_sys_stt.lamp_type == M_LAMP_TYPE_NON_DIMMABLE_C) && 
				(elapsed_ms_in_state > 1000) && 
				 (g_sys_stt.lamp_power_level != M_LAMP_PWR_100PCT_C))
			{
				// Can tell immediately if a non-dimmable lamp has gone out
				mod_lamp_go_to_state(M_LAMP_STATE_RESTRIKE_COOLDOWN_1_C);
			}

			if (mod_lamp_req_pwr_level == M_LAMP_PWR_OFF_C)
			{
				mod_lamp_go_to_state(M_LAMP_STATE_OFF_C);
			}

			//g_sys_ctl.lamp_req_pwr_level = M_LAMP_PWR_NONE_C; // TODO: Clear requirement here or in mod_lamp_request_power_level ?
		break;

		case M_LAMP_STATE_FULLPOWER_TEST_C:
			commanded_power_level = M_LAMP_PWR_100PCT_C;

			if (g_sys_stt.lamp_power_level == M_LAMP_PWR_100PCT_C)
			{
				M_LAMP_DBG_PRINT_TXT("Got a reported 100%% power, OK");
				mod_lamp_go_to_state(M_LAMP_STATE_RUNNING_C);
			}
			else if (elapsed_ms_in_state > M_LAMP_START_MS_TIME_C)
			{
				M_LAMP_DBG_PRINT_TXT("Timed out for 100%% test");
				mod_lamp_go_to_state(M_LAMP_STATE_RESTRIKE_COOLDOWN_1_C);
			}

			if (mod_lamp_req_pwr_level == M_LAMP_PWR_OFF_C)
			{
				mod_lamp_go_to_state(M_LAMP_PWR_OFF_C);
			}

			// Don't go to off while fullpower test -- open question?
		break;

		// WARNING: Don't go OFF in the middle of restrike attempt to avoid short cycling

		case M_LAMP_STATE_RESTRIKE_COOLDOWN_1_C:
			commanded_power_level = M_LAMP_PWR_OFF_C;

			if (mod_lamp_req_pwr_level == M_LAMP_PWR_OFF_C)
			{
				mod_lamp_go_to_state(M_LAMP_STATE_OFF_C);
			}
			if (elapsed_ms_in_state > M_LAMP_RESTRIKE_COOLDOWN_MS_TIME_C)
			{
				M_LAMP_DBG_PRINT_TXT("Going to restrike attempt #1");
				mod_lamp_go_to_state(M_LAMP_STATE_RESTRIKE_ATTEMPT_1_C);
			}
		break;

		case M_LAMP_STATE_RESTRIKE_ATTEMPT_1_C:
			commanded_power_level = M_LAMP_PWR_100PCT_C;

			if (g_sys_stt.lamp_power_level == M_LAMP_PWR_100PCT_C)
			{
				M_LAMP_DBG_PRINT_TXT("Restrike succeeded on attempt #1");
				mod_lamp_go_to_state(M_LAMP_STATE_STARTING_C);
			}
			if (elapsed_ms_in_state > M_LAMP_START_MS_TIME_C)
			{
				M_LAMP_DBG_PRINT_TXT("Timed out on restrike attempt #1");
				mod_lamp_go_to_state(M_LAMP_STATE_RESTRIKE_COOLDOWN_2_C);
			}
			if (mod_lamp_req_pwr_level == M_LAMP_PWR_OFF_C)
			{
				mod_lamp_go_to_state(M_LAMP_PWR_OFF_C);
			}
		break;

		case M_LAMP_STATE_RESTRIKE_COOLDOWN_2_C:
			commanded_power_level = M_LAMP_PWR_OFF_C;

			if (mod_lamp_req_pwr_level == M_LAMP_PWR_OFF_C)
			{
				mod_lamp_go_to_state(M_LAMP_STATE_OFF_C);
			}
			if (elapsed_ms_in_state > M_LAMP_RESTRIKE_COOLDOWN_MS_TIME_C)
			{
				M_LAMP_DBG_PRINT_TXT("Going to restrike attempt #2");
				mod_lamp_go_to_state(M_LAMP_STATE_RESTRIKE_ATTEMPT_2_C);
			}
		break;

		case M_LAMP_STATE_RESTRIKE_ATTEMPT_2_C:
			commanded_power_level = M_LAMP_PWR_100PCT_C;

			if (g_sys_stt.lamp_power_level == M_LAMP_PWR_100PCT_C)
			{
				M_LAMP_DBG_PRINT_TXT("Restrike succeeded on attempt #2");
				mod_lamp_go_to_state(M_LAMP_STATE_STARTING_C);
			}
			if (elapsed_ms_in_state > M_LAMP_START_MS_TIME_C)
			{
				M_LAMP_DBG_PRINT_TXT("Timed out on restrike attempt #2");
				mod_lamp_go_to_state(M_LAMP_STATE_RESTRIKE_COOLDOWN_3_C);
			}
			if (mod_lamp_req_pwr_level == M_LAMP_PWR_OFF_C)
			{
				mod_lamp_go_to_state(M_LAMP_PWR_OFF_C);
			}
		break;

		case M_LAMP_STATE_RESTRIKE_COOLDOWN_3_C:
			commanded_power_level = M_LAMP_PWR_OFF_C;

			if (mod_lamp_req_pwr_level == M_LAMP_PWR_OFF_C)
			{
				mod_lamp_go_to_state(M_LAMP_STATE_OFF_C);
			}
			if (elapsed_ms_in_state > M_LAMP_RESTRIKE_COOLDOWN_MS_TIME_C)
			{
				M_LAMP_DBG_PRINT_TXT("Going to restrike attempt #3");
				mod_lamp_go_to_state(M_LAMP_STATE_RESTRIKE_ATTEMPT_3_C);
			}
		break;

		case M_LAMP_STATE_RESTRIKE_ATTEMPT_3_C:
			commanded_power_level = M_LAMP_PWR_100PCT_C;

			if (g_sys_stt.lamp_power_level == M_LAMP_PWR_100PCT_C)
			{
				M_LAMP_DBG_PRINT_TXT("Restrike succeeded on attempt #3");
				mod_lamp_go_to_state(M_LAMP_STATE_STARTING_C);
			}
			if (elapsed_ms_in_state > M_LAMP_START_MS_TIME_C)
			{
				M_LAMP_DBG_PRINT_TXT("Timed out on restrike attempt #3");
				mod_lamp_go_to_state(M_LAMP_STATE_FAILED_OFF_C);
			}
			if (mod_lamp_req_pwr_level == M_LAMP_PWR_OFF_C)
			{
				mod_lamp_go_to_state(M_LAMP_PWR_OFF_C);
			}
		break;

		case M_LAMP_STATE_FAILED_OFF_C:
			commanded_power_level = M_LAMP_PWR_OFF_C;

			if (mod_lamp_req_pwr_level == M_LAMP_PWR_OFF_C)
			{
				mod_lamp_go_to_state(M_LAMP_STATE_OFF_C);
			}
		break;

		case M_LAMP_STATE_OFF_C:
			commanded_power_level = M_LAMP_PWR_OFF_C;
		break;

		default:
		break;
	}

	/* Report commanded power level to global system status  */
	if (g_sys_stt.lamp_cmd_power_level != commanded_power_level)
	{
		g_sys_stt.lamp_cmd_power_level = commanded_power_level;
	}

	/* Update PWM level */
	drv_lamp_set_pwm_level(mod_lamp_pwr_settings[g_sys_stt.lamp_cmd_power_level].pwm);

	/* Update lamp enable/disable state */
	if (g_sys_stt.lamp_cmd_power_level != M_LAMP_PWR_OFF_C)
	{
		drv_lamp_enable();
	}
	else
	{
		drv_lamp_disable();
	}

	mod_lamp_power_source_monitor();

	g_sys_stt.is_lamp_warming   = mod_lamp_is_warming();
	g_sys_stt.lamp_elap_ms_time = mod_lamp_get_state_elapsed_ms(); 				// TODO: Evaluate current state to reduce MCU instructions ?
}

/**
 * @brief Loads the preset factory lamp type @ref M_LAMP_TYPE_E
 * 
 * @return 	void  
 * 
 */
void mod_lamp_load_type_from_flash(void)
{
	g_sys_stt.lamp_type = drv_cfg_get_factory_lamp_type();

	M_LAMP_DBG_PRINT_TXT("Determined type from flash: %d", g_sys_stt.lamp_type);
}

/**
 * @brief Performs a test on the lamp to get its type
 * 
 * @return int8_t 0: In progress, -1: error, 1: succeed
 */
int8_t mod_lamp_perform_type_test(void)
{
    static uint8_t stt_mchn = 0;

	switch (stt_mchn)
	{
		case 0:
			if (g_sys_stt.lamp_type == M_LAMP_TYPE_UNKNOWN_C)
			{
				M_LAMP_DBG_PRINT_TXT("Performing lamp type test");

				g_sys_ctl.lamp_req_pwr_level = M_LAMP_PWR_OFF_C;

				mod_lamp_delay_tmout = make_timeout_time_ms(100);

				// TODO: Is this delay required or was for making sure that M_LAMP_PWR_OFF_C state was applied?

				stt_mchn++;
			}
			else
			{
				return 1;
			}
		break;

		case 1:
			if (get_absolute_time() > mod_lamp_delay_tmout)
			{
				M_LAMP_DBG_PRINT_TXT("Type test (dimming-response)");
				M_LAMP_DBG_PRINT_TXT("Requesting 70%%, striking at 100%%...");

				g_sys_ctl.lamp_req_pwr_level = M_LAMP_PWR_70PCT_C;

				mod_lamp_tmout 		 = make_timeout_time_ms(30 * 1000);
				mod_lamp_delay_tmout = make_timeout_time_ms(10); // TODO: Is this delay required or was for making sure that M_LAMP_PWR_70PCT_C state was applied?

				stt_mchn++;
			}
		break;

		case 2:
			if (get_absolute_time() < mod_lamp_tmout)
			{
				if (get_absolute_time() >= mod_lamp_delay_tmout)
				{
					if ((g_sys_stt.lamp_state == M_LAMP_STATE_FAILED_OFF_C         ) ||
						(g_sys_stt.lamp_state == M_LAMP_STATE_RESTRIKE_COOLDOWN_1_C))
					{
						M_LAMP_DBG_PRINT_ERR("Type test: failed to strike");

						g_sys_stt.lamp_type = M_LAMP_TYPE_UNKNOWN_C;

						stt_mchn = (uint8_t)-1;
					}
					else if (g_sys_stt.lamp_state == M_LAMP_STATE_RUNNING_C)
					{
						stt_mchn++;
					}
					else
					{
						mod_lamp_delay_tmout = make_timeout_time_ms(10);// TODO: Is this delay required ?
					}
				}
			}
			else
			{
				M_LAMP_DBG_PRINT_ERR("Type test. Timeout (Ln %d)", __LINE__);

				stt_mchn = (uint8_t)-1;
			}
		break;

		case 3:
			if (g_sys_stt.lamp_state != M_LAMP_STATE_RUNNING_C)
			{
				M_LAMP_DBG_PRINT_TXT("Type test: strike timeout");

				g_sys_stt.lamp_type = M_LAMP_TYPE_UNKNOWN_C;

				g_sys_ctl.lamp_req_pwr_level = M_LAMP_PWR_100PCT_C;

				stt_mchn = (uint8_t)-1;
			}
			else
			{
				// Wait for warmup — ballast ignores DIMMING during warmup
				M_LAMP_DBG_PRINT_TXT("Type test: lamp running, waiting for warmup...");

				if (mod_lamp_is_warming())
				{
					mod_lamp_delay_tmout = make_timeout_time_ms(10);// TODO: Is this delay required ?

					stt_mchn++;
				}
			}
		break;

		case 4:
			if (get_absolute_time() > mod_lamp_delay_tmout)
			{
				if (!mod_lamp_is_warming())
				{
					// Check dimming response — wait for reported == commanded
					M_LAMP_DBG_PRINT_TXT("Type test: warmup done, checking dimming response...");

					mod_lamp_tmout = make_timeout_time_ms(5 * 1000);

					stt_mchn++;
				}

				mod_lamp_delay_tmout = make_timeout_time_ms(10);// TODO: Is this delay required or was for making sure that M_LAMP_PWR_100PCT_C state was applied?
			}
		break;

		case 5:
			if (get_absolute_time() < mod_lamp_tmout)
			{
				if (get_absolute_time() >= mod_lamp_delay_tmout)
				{
					M_LAMP_DBG_PRINT_TXT("Type test: polling freq=%dHz reported=%s",
										 g_sys_stt.lamp_latched_freq_hz,
										 mod_lamp_get_power_level_str(g_sys_stt.lamp_power_level));

					if (g_sys_stt.lamp_power_level == g_sys_stt.lamp_cmd_power_level)
					{
						stt_mchn++;
					}
					else
					{
						mod_lamp_delay_tmout = make_timeout_time_ms(10);// TODO: Is this delay required ?
					}
				}
			}
			else
			{
				M_LAMP_DBG_PRINT_WRN("Type test. Timeout (Ln %d)", __LINE__);
				stt_mchn++;
				//stt_mchn = (uint8_t)-1;
			}
		break;

		case 6:
			M_LAMP_DBG_PRINT_TXT("Type test: commanded=%s, reported=%s, freq=%dHz",
				   				 mod_lamp_get_power_level_str(g_sys_stt.lamp_cmd_power_level),
				   				 mod_lamp_get_power_level_str(g_sys_stt.lamp_power_level),
								 g_sys_stt.lamp_latched_freq_hz);

			if (g_sys_stt.lamp_power_level == M_LAMP_PWR_70PCT_C)
			{
				M_LAMP_DBG_PRINT_TXT("Determined dimmable (responded to 70%% dimming)");
				g_sys_stt.lamp_type = M_LAMP_TYPE_DIMMABLE_C;
			}
			else if (g_sys_stt.lamp_power_level == M_LAMP_PWR_100PCT_C)
			{
				M_LAMP_DBG_PRINT_TXT("Determined non-dimmable (ignored dimming)");
				g_sys_stt.lamp_type = M_LAMP_TYPE_NON_DIMMABLE_C;
				g_sys_ctl.lamp_req_pwr_level = M_LAMP_PWR_100PCT_C;
			}
			else
			{
				M_LAMP_DBG_PRINT_ERR("Type test inconclusive (reported=%s) — UNKNOWN",
									 mod_lamp_get_power_level_str(g_sys_stt.lamp_power_level));
				g_sys_stt.lamp_type = M_LAMP_TYPE_UNKNOWN_C;
				g_sys_ctl.lamp_req_pwr_level = M_LAMP_PWR_100PCT_C;
			}
			stt_mchn++;
		break;

		default:
			stt_mchn = 0;
			
			if (g_sys_stt.lamp_type != M_LAMP_TYPE_UNKNOWN_C)
			{
				drv_cfg_set_factory_lamp_type(g_sys_stt.lamp_type);
				g_sys_ctl.task.save_cfg = 1;

				M_LAMP_DBG_PRINT_OK("Saving concluded lamp type: %d", 
									mod_lamp_get_lamp_type_str(g_sys_stt.lamp_type));

				return 1;
			}
			else
			{
				M_LAMP_DBG_PRINT_WRN("WARNING: lamp type still UNKNOWN after type test");

				return -1;
			}
		break;
	}

	return 0;
}

/**
 * @brief Resets the lamp type to UNKNOWN and clears persistence.
 *        Allows mod_lamp_perform_type_test() to re-run detection.
 */
void mod_lamp_reset_type(void)
{
	g_sys_stt.lamp_type = M_LAMP_TYPE_UNKNOWN_C;

	drv_cfg_set_factory_lamp_type(M_LAMP_TYPE_UNKNOWN_C);
	g_sys_ctl.task.save_cfg = 1;

	M_LAMP_DBG_PRINT_TXT("mod_lamp_reset_type: Lamp type reset to %s", 
						 mod_lamp_get_lamp_type_str(g_sys_stt.lamp_type));
}


/* Callback functions --------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
 * @brief Sets a new lamp state
 * 
 * @param state The new state to set
 * 
 * @related M_LAMP_STATE_E
 * 
 * @return 	void
 */
static inline void mod_lamp_go_to_state(M_LAMP_STATE_E state)
{
	if (state != g_sys_stt.lamp_state) 
	{
		M_LAMP_DBG_PRINT_TXT("State transition to %s", mod_lamp_get_lamp_state_str(state));
	}
	g_sys_stt.lamp_state = state;
	mod_lamp_state_transition_time = time_us_64();
}

/**
 * @brief Returns the state transition time in milliseconds
 * 
 * @return uint64_t 
 */
static inline uint64_t mod_lamp_get_state_elapsed_ms(void)
{
	return (time_us_64() - mod_lamp_state_transition_time) / 1000;
}

/**
 * @brief Returns whether the lamp is warming or not
 * 
 * @return true 
 * @return false 
 */
static inline bool mod_lamp_is_warming(void)
{
    uint32_t ms = mod_lamp_get_state_elapsed_ms();

    return (g_sys_stt.lamp_state == M_LAMP_STATE_STARTING_C) ||
           ((g_sys_stt.lamp_state == M_LAMP_STATE_RUNNING_C) && 
		    (ms < M_LAMP_START_MS_TIME_C));
}
/**
 * @brief Request lamp to set a power level
 * 
 * If requested power level is already satisfied, the process will return true
 * 
 */
static void mod_lamp_request_power_level(void)
{
	if (!((g_sys_ctl.lamp_req_pwr_level >= M_LAMP_PWR_OFF_C) && 
		  (g_sys_ctl.lamp_req_pwr_level <= M_LAMP_PWR_100PCT_C)))
	{
		return;
	}
	if (mod_lamp_req_pwr_level == g_sys_ctl.lamp_req_pwr_level)
	{
		return;
	}
	if (g_sys_stt.lamp_power_level == g_sys_ctl.lamp_req_pwr_level) 
	{
		return;
	}
	if (g_sys_stt.lamp_state == M_LAMP_STATE_FAILED_OFF_C) 
	{
		return; // dead
	}

	bool requested_on = (g_sys_ctl.lamp_req_pwr_level >= M_LAMP_PWR_20PCT_C) && 
						(g_sys_ctl.lamp_req_pwr_level <= M_LAMP_PWR_100PCT_C);

	if (g_sys_stt.lamp_type == M_LAMP_TYPE_NON_DIMMABLE_C)
	{
		if ((g_sys_ctl.lamp_req_pwr_level != M_LAMP_PWR_OFF_C) && 
			(g_sys_ctl.lamp_req_pwr_level != M_LAMP_PWR_100PCT_C))
		{
			M_LAMP_DBG_PRINT_WRN("Reject dimmed control point for lamp not known to dim");
			g_sys_ctl.lamp_req_pwr_level = M_LAMP_PWR_NONE_C;
			return;
		}
	}

	if (requested_on && (!g_sys_stt.is_12v_rail_on || !g_sys_stt.is_24v_rail_on))
	{
		M_LAMP_DBG_PRINT_WRN("Reject turn on lamp without both rails on");
		g_sys_ctl.lamp_req_pwr_level = M_LAMP_PWR_NONE_C;
		return;
	}

	if ((g_sys_stt.lamp_power_level == M_LAMP_PWR_OFF_C) &&
		(g_sys_ctl.lamp_req_pwr_level != M_LAMP_PWR_OFF_C))
	{
		g_sys_stt.lamp_state = M_LAMP_STATE_STARTING_C;
		
		mod_lamp_state_transition_time = time_us_64();

		M_LAMP_DBG_PRINT_TXT("Setting lamp state to %s", 
							 mod_lamp_get_lamp_state_str(g_sys_stt.lamp_state));
	}

	M_LAMP_DBG_PRINT_TXT("Consumed requested lamp power level: %s", 
						 mod_lamp_get_power_level_str(g_sys_ctl.lamp_req_pwr_level));

	mod_lamp_req_pwr_level = g_sys_ctl.lamp_req_pwr_level;

	//g_sys_ctl.lamp_req_pwr_level = M_LAMP_PWR_NONE_C; // TODO: Clear request ?
}

/**
 * @brief Monitors any power source failure while the lamp is on
 * 
 */
static void mod_lamp_power_source_monitor(void)
{
    static uint8_t stt_mchn = 0;

	switch (stt_mchn)
	{
		case 0:
			if ((g_sys_stt.lamp_state != M_LAMP_STATE_OFF_C) && !g_sys_stt.is_power_ok)
			{
				M_LAMP_DBG_PRINT_ERR("EMERGENCY SHUTDOWN: VBUS=%.2f 12V=%.2f 24V=%.2f",
			   				 		 g_sys_stt.v_vbus, g_sys_stt.v_12v, g_sys_stt.v_24v);

				drv_lamp_enable();  											// Possible intentional discharge

				mod_lamp_pw_mon_dly_tmout = make_timeout_time_ms(100);

				stt_mchn++;
			}
		break;

		case 1:
			if (get_absolute_time() > mod_lamp_pw_mon_dly_tmout) 
			{
				g_sys_ctl.task.rails_off = 1; 									// Request to turn off power rails

				stt_mchn++;
			}
		break;

		case 2:
			if (g_sys_stt.task.rails_off && !g_sys_stt.task.rails_on)			// Are power rails off ?
			{
				mod_lamp_go_to_state(M_LAMP_STATE_OFF_C);

				stt_mchn = 0;
			}
		break;

		default:
			stt_mchn = 0;
		break;
	}
}


/*** END OF FILE ***/