/**
 * @file      mod_lamp_types.h
 * @author    The OSLUV Project
 * @brief     Lamp type definitions for lamp control
 *  
 */

#ifndef _M_LAMP_TYPES_H_
#define _M_LAMP_TYPES_H_


/* Exported includes ---------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>


/* Exported defines ----------------------------------------------------------*/

#define M_LAMP_TYPE_LIST \
    X(M_LAMP_TYPE_UNKNOWN_C,                "UNKOWN")               \
    X(M_LAMP_TYPE_DIMMABLE_C,               "DIMMABLE")             \
    X(M_LAMP_TYPE_NON_DIMMABLE_C,           "NON DIMMABLE")

#define M_LAMP_STATE_LIST \
    X(M_LAMP_STATE_OFF_C,                   "OFF")                  \
    X(M_LAMP_STATE_STARTING_C,              "STARTING")             \
    X(M_LAMP_STATE_RUNNING_C,               "RUNNING")              \
    X(M_LAMP_STATE_FULLPOWER_TEST_C,        "FULLPOWER_TEST")       \
    X(M_LAMP_STATE_RESTRIKE_COOLDOWN_1_C,   "RESTRIKE_COOLDOWN_1")  \
    X(M_LAMP_STATE_RESTRIKE_ATTEMPT_1_C,    "RESTRIKE_ATTEMPT_1")   \
    X(M_LAMP_STATE_RESTRIKE_COOLDOWN_2_C,   "RESTRIKE_COOLDOWN_2")  \
    X(M_LAMP_STATE_RESTRIKE_ATTEMPT_2_C,    "RESTRIKE_ATTEMPT_2")   \
    X(M_LAMP_STATE_RESTRIKE_COOLDOWN_3_C,   "RESTRIKE_COOLDOWN_3")  \
    X(M_LAMP_STATE_RESTRIKE_ATTEMPT_3_C,    "RESTRIKE_ATTEMPT_3")   \
    X(M_LAMP_STATE_FAILED_OFF_C,            "FAILED_OFF")

#define M_LAMP_PWR_LVL_LIST \
    X(M_LAMP_PWR_OFF_C,                   	"OFF")      \
    X(M_LAMP_PWR_20PCT_C,              		"20%")      \
    X(M_LAMP_PWR_40PCT_C,               	"40%")      \
    X(M_LAMP_PWR_70PCT_C,        			"70%")      \
    X(M_LAMP_PWR_100PCT_C,   				"100%") 	\
    X(M_LAMP_PWR_MAX_SETTINGS_C,    		"INVALID")	\
    X(M_LAMP_PWR_NONE_C,                    "NONE")	    \
    X(M_LAMP_PWR_UNKNOWN_C,   				"UNKOWN")


/* Exported typedef ----------------------------------------------------------*/

/**
 * @enum M_LAMP_TYPE_E
 * @brief Available types of lamps
 * 
 * Used in persistance region; bump magic if changed
 * 
 */
#if 0
typedef enum {
	M_LAMP_TYPE_UNKNOWN_C = 0,
	M_LAMP_TYPE_DIMMABLE_C,
	M_LAMP_TYPE_NON_DIMMABLE_C
} M_LAMP_TYPE_E;
#else
typedef enum {
    #define X(type, str) type,
    M_LAMP_TYPE_LIST
    #undef X
} M_LAMP_TYPE_E;
#endif

/**
 * @enum M_LAMP_PWR_LEVEL_E
 * 
 * @brief Available power levels for lamp
 * 
 */
#if 0
typedef enum {
	M_LAMP_PWR_OFF_C = 0,
	M_LAMP_PWR_20PCT_C,
	M_LAMP_PWR_40PCT_C,
	M_LAMP_PWR_70PCT_C,
	M_LAMP_PWR_100PCT_C,
	M_LAMP_PWR_MAX_SETTINGS_C,
	M_LAMP_PWR_UNKNOWN_C
} M_LAMP_PWR_LEVEL_E;
#else
typedef enum {
    #define X(level, str) level,
    M_LAMP_PWR_LVL_LIST
    #undef X
} M_LAMP_PWR_LEVEL_E;
#endif

/**
 * @enum M_LAMP_STATE_E
 * 
 * @brief Lamp states list
 * 
 */
#if 0
typedef enum {
	M_LAMP_STATE_OFF_C = 0,
	M_LAMP_STATE_STARTING_C,
	M_LAMP_STATE_RUNNING_C,
	M_LAMP_STATE_FULLPOWER_TEST_C,
	M_LAMP_STATE_RESTRIKE_COOLDOWN_1_C,
	M_LAMP_STATE_RESTRIKE_ATTEMPT_1_C,
	M_LAMP_STATE_RESTRIKE_COOLDOWN_2_C,
	M_LAMP_STATE_RESTRIKE_ATTEMPT_2_C,
	M_LAMP_STATE_RESTRIKE_COOLDOWN_3_C,
	M_LAMP_STATE_RESTRIKE_ATTEMPT_3_C,
	M_LAMP_STATE_FAILED_OFF_C
} M_LAMP_STATE_E;
#else
typedef enum {
    #define X(name, str) name,
    M_LAMP_STATE_LIST
    #undef X
} M_LAMP_STATE_E;
#endif

/**/

/**
 * @brief Returns the string ID for the lamp type
 * 
 * @param state @ref M_LAMP_TYPE_E
 * @return const char* 
 */
static inline const char* mod_lamp_get_lamp_type_str(M_LAMP_TYPE_E type)
{
    static const char* types[] = {
		#define X(type, str) [type] = str,
        M_LAMP_TYPE_LIST
        #undef X
    };

    if (type < (sizeof(types) / sizeof(types[0])))
    {
        return types[type];
    }

    return "!?!";
}

/**
 * @brief Returns the string ID for a lamp state
 * 
 * @param state @ref M_LAMP_STATE_E
 * @return const char* 
 */
static inline const char* mod_lamp_get_lamp_state_str(M_LAMP_STATE_E state)
{
    static const char* names[] = {
		#define X(name, str) [name] = str,
        M_LAMP_STATE_LIST
        #undef X
    };

    if (state < (sizeof(names) / sizeof(names[0])))
    {
        return names[state];
    }

    return "!?!";
}

/**
 * @brief Returns the string ID for a power level
 * 
 * @param pwr_level @ref M_LAMP_PWR_LEVEL_E
 * @return const char* 
 */
static inline const char* mod_lamp_get_power_level_str(M_LAMP_PWR_LEVEL_E level)
{
    static const char* levels[] = {
		#define X(level, str) [level] = str,
        M_LAMP_PWR_LVL_LIST
        #undef X
    };

    if (level < (sizeof(levels) / sizeof(levels[0])))
    {
        return levels[level];
    }

	return "!?!";
}


#endif /* _M_LAMP_TYPES_H_ */

/*** END OF FILE ***/
