/* 
 * File:   AL_ModeMgt.h
 * Author: Sandeep Raj
 *
 * Created on January 29, 2018, 2:00 AM
 */

#ifndef AL_MODEMGT_H
#define	AL_MODEMGT_H

#include "simOs_types.h"
#include "simOs_Signals.h"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef AL_MAINCTRL    

SimOsRet_t AL_MainCtrl_Open(void);

SimOsRet_t AL_MainCtrl_Tsk(void);

#endif //AL_MODEMGT

#ifdef	__cplusplus
}
#endif

#endif	/* AL_MAINCTRL */

