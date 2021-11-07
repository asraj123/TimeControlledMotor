/* 
 * File:   AL_Config.h
 * Author: Sandeep Raj
 *
 * Created on January 3, 2018, 11:32 PM
 */

#include "../../../../../Lib/Code/MyCode/OS/Inc/simOs.h"

#ifndef AL_CONFIG_H
#define	AL_CONFIG_H

#ifdef	__cplusplus
extern "C" {
#endif

#define FLOAT_SEN_STATE      0 //Flaot switch

#define ON_TIME_BEFORE_SLEEP (2000u/SIMOS_5MS_TASK_CYCLE_T)
#define SLEEP_TIME_MINUTE    (20)
#define VALVE_ACTIVE         0
    
#ifdef	__cplusplus
}
#endif

#endif	/* AL_CONFIG_H */

