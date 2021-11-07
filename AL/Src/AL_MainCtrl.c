#include "AL_config.h"
#include "AL_Types.h"
#include "IL_Rte.h"

#include "AL_MainCtrl.h"

#ifdef AL_MAINCTRL


/*
 * ------------------------------------------------------------------------------------------------------
 * Requirements
 * ------------------------------------------------------------------------------------------------------
 * Function shall switch ON motor within 1sec of switch on for configured time at EEPROM address 0
 * -- function shall not switch ON if motor if 'MOTOR_OFF switch' is active during startup
 * -- LED : Green(1sec) --> Red
 * 
 * Motor shall be immediatly OFF if 'MOTOR_OFF switch' is active
 * 
 * Function shall switch OFF module after 5sec of Motor OFF and no activation of 'MOTOR_OFF switch'
 * -- LED : Red --> Green(5sec) --> OFF
 * 
 * (optional)
 * Function shall be enter to calibration mode if long press detected on 'MOTOR_OFF switch'
 * -- every short press on 'MOTOR_OFF switch' in calibration mode start calibration time of 30 sec
 *    -- LED shall be blink of pulse 200ms ON2
 * -- Calibration value shall be start 0 every time enter to calibration mode
 * -- Calibration value shall be save to EEPROM address '0'if no press on 'MOTOR_OFF switch' for 5sec
 *    -- Calibration value shall not be written if calibration count == 0
 * -- Calibration mode shall be exited if no press on 'MOTOR_OFF switch' for 5sec
 * 
 * Function shall indicate ON status by LED
 * -- OFF                : Module OFF
 * -- Stay ON (Green)    : Normal mode
 * -- Stay ON (Red)      : Motor ON
 * -- Stay ON (Orange)   : Calibration mode
 * -- Short Pulse(Orange): Calibration timer reach every 30sec
 * 
 * Function shall not switch ON motor if power failure during motor ON last ECU cycle
 * -- motor ON status shall be stored at EEPROM 1
 *    -- Idle state: motor ready to ON and no power failure in last time
 *    -- On state: there is a power failure in last motor ON (User shall clear this by OFF switch)
 *    -- Off state: Motor ready to OFF (User shall clear this by OFF switch)
 */

#define AL_USE_CALIBRATION
#define EEPROM_CALIB_DATA  0
#define EEPROM_MOTOR_STA   1


typedef enum {
    BUT_TYPE_SHORT,
    BUT_TYPE_LONG,
    BUT_TYPE_SHORT_LOOP,
    BUT_TYPE_LONG_LOOP,
    BUT_TYPE_END
} Button_Type_t;

typedef enum{
    M_STA_IDLE = 1,
    M_STA_ON,
    M_STA_OFF
}MotorSta_t;

#define SIM_OS_TIME_SLICE  SIMOS_TASK_CYCLE_T

#define BUTTON_TYPE_SHORT  0
#define BUTTON_TYPE_LONG   1

#define BUT_PRESS_NONE     0
#define BUT_PRESS_SHORT    1
#define BUT_PRESS_LONG     2
#define BUT_PRESS_ERROR    3

#define BUTTON_MODE_PREV_10DELY (10U * BUTTON_MODE_PREV_DELY)

#define BUTTON_ERROR_TIMEOUT      (5000U/SIM_OS_TIME_SLICE) //5sec
#define CHK_BUTTON_ERROR_TIMEOUT  (2000U /SIM_OS_TIME_SLICE)  //2sec

#define BUTTON_REBOUNCE_100MS     (700U/SIM_OS_TIME_SLICE) // 20 = 200 ms/ 10 ms

#define BUTTON_DELY               (60U/SIM_OS_TIME_SLICE)   //20ms
#define BUTTON_LOOP_DELY          (500U/SIM_OS_TIME_SLICE)  
#define BUTTON_LONG_DELY          (1000U/SIM_OS_TIME_SLICE)   
#define BUTTON_ERROR_DELY         (4000U/SIM_OS_TIME_SLICE)   
#define BUTTON_RELESE_DELY        (1000U/SIM_OS_TIME_SLICE)  
#define BUTTON_MODE_PREV_DELY     (500U/SIM_OS_TIME_SLICE)  
//ButtonFlaggs
#define BUTTON_MANUAL_ON       0
#define MOTOR_OFF_REQ_1B       1
#define CALIB_REQ_1B           2
#define MOTOR_STA_2B           3
#define MOTOR_STA_LOC_1B       5

#define CALIB_TIME_OUT    (5000U/SIM_OS_TIME_SLICE)
#define FACTOR_30SEC      (30000U/SIM_OS_TIME_SLICE)
#define FACTOR_1SEC       (1000U/SIM_OS_TIME_SLICE)
#define MODULE_OFF_TIME   (5000U/SIM_OS_TIME_SLICE)
#define CALIB_PULSE_TIME  (200U/SIM_OS_TIME_SLICE)
#define MOTOR_ON_VALIDATE_TIME (5000U/SIM_OS_TIME_SLICE)
#define MOTOR_OFF_VALIDATE_TIME (2000U/SIM_OS_TIME_SLICE)

//#define AL_MODULE_OFF AL_MOTOR_ON

//static UINT8 Button_RebTiOut0 = 0;
//static UINT8 ButReleseCntr;
static UINT8  ButShortCntr;
static UINT16 ButLongCntr;
static UINT8  ButtonFlaggs;
static UINT8  CalibCounter;
static UINT8  CalibTimeCounter;
static UINT8  CalibValue;

static UINT8 ButtonPress(UINT8 FlagPos,
        Button_Type_t Type,
        UINT8 HwBit,
        UINT8 HwBit_Evaluate,
        UINT8 *ButtonShortCntrPtr,
        UINT16 *ButtonLongCntrPtr) ;

SimOsRet_t AL_MainCtrl_Open(void){
    UINT8 stMotor = 0;
    RteClr(GIO_0, OFF_CONTROL_1B, 1);
    RteSet(GIO_0, LED_GREEN_1B, 1);
    RteClr(GIO_0, LED_RED_1B, 1);
    RteClr(GIO_0, MOTOR_CONTROL_1B, 1);
#ifdef AL_USE_CALIBRATION
    (void)IL_NvmRead(EEPROM_CALIB_DATA, &CalibValue);  
#endif //AL_USE_CALIBRATION
    (void)IL_NvmRead(EEPROM_MOTOR_STA, &stMotor);
    if ((stMotor < M_STA_IDLE) || (stMotor > M_STA_OFF)){
        stMotor = M_STA_IDLE;
    }
    BitSet(ButtonFlaggs, MOTOR_STA_2B, (stMotor & 0x03));

    TimerRunType[AL_CALIB] = TIMER_START;
}

SimOsRet_t AL_MainCtrl_Tsk(void)
{
    UINT8 stOFFbutton;
    UINT8 stMotorForceOff = FALSE;
    
    stOFFbutton = ButtonPress(BUTTON_MANUAL_ON,
            BUT_TYPE_LONG,
            RteGet(GIO_0, OFF_SWITCH_1B, 1),
            FALSE,
            &ButShortCntr,
            &ButLongCntr);
    
    if (BUT_PRESS_ERROR == stOFFbutton) {
        BitSet(ButtonFlaggs, MOTOR_OFF_REQ_1B, 1);
    }
#ifdef AL_USE_CALIBRATION
    else if (BUT_PRESS_LONG == stOFFbutton) {
        //Calibration request by user        
        BitSet(ButtonFlaggs, CALIB_REQ_1B, 1);
        //Calibration value shall be start 0 every time enter to calibration mode
        CalibCounter = 0;
        //(void)IL_Timer(AL_CALIB_TIMEOUT, TIMER_RESTART);
        TimerRunType[AL_CALIB_TIMEOUT] = TIMER_START;
        IL_RteTimer[AL_CALIB_TIMEOUT] = 0;
        BitSet(ButtonFlaggs, MOTOR_OFF_REQ_1B, 1);
    }
#endif //AL_USE_CALIBRATION
    else if (BUT_PRESS_SHORT == stOFFbutton) {
        BitSet(ButtonFlaggs, MOTOR_OFF_REQ_1B, 1);
        stMotorForceOff = TRUE;
#ifdef AL_USE_CALIBRATION
        //every short press on 'MOTOR_OFF switch' in calibration mode start calibration time of 30 sec
        if (BitGet(ButtonFlaggs, CALIB_REQ_1B, 1)){
            CalibCounter++;
            IL_RteTimer[AL_CALIB_TIMEOUT] = 0;
            TimerRunType[AL_CALIB_PRESS] = TIMER_START;
            IL_RteTimer[AL_CALIB_PRESS] = 0;
        }
#endif //AL_USE_CALIBRATION
    }

#ifdef AL_USE_CALIBRATION    
    //Calibration request by user
    if (BitGet(ButtonFlaggs, CALIB_REQ_1B, 1)){
        BitSet(ButtonFlaggs, MOTOR_OFF_REQ_1B, 1);
        if (IL_RteTimer[AL_CALIB_TIMEOUT] > CALIB_TIME_OUT){
            if (CalibCounter > 0){
                (void)IL_NvmWrite(EEPROM_CALIB_DATA, CalibCounter);
                CalibValue = CalibCounter;
            }
            BitClr(ButtonFlaggs, CALIB_REQ_1B, 1);
            TimerRunType[AL_CALIB_TIMEOUT] = TIMER_STOP;
        }        
    }
#endif //AL_USE_CALIBRATION
    
    //Function shall switch ON motor within 1sec of switch on for configured time at EEPROM address 0
    if (IL_RteTimer[AL_CALIB] > FACTOR_30SEC){
        IL_RteTimer[AL_CALIB] = 0;
        CalibTimeCounter++;
    }
    if ((CalibTimeCounter) >= (CalibValue)){
        BitSet(ButtonFlaggs, MOTOR_OFF_REQ_1B, 1);
    }
    
    //motor ON status shall be stored at EEPROM 1
    if ((M_STA_ON == BitGet(ButtonFlaggs, MOTOR_STA_2B, 3))
         && (stMotorForceOff == TRUE)){
        (void)IL_NvmWrite(EEPROM_MOTOR_STA, M_STA_IDLE);
        //set to unknown state
        BitClr(ButtonFlaggs, MOTOR_STA_2B, 3);
    } else if ((M_STA_OFF == BitGet(ButtonFlaggs, MOTOR_STA_2B, 3))
         && (stMotorForceOff == TRUE)){
        (void)IL_NvmWrite(EEPROM_MOTOR_STA, M_STA_IDLE);
        //set to unknown state
        BitClr(ButtonFlaggs, MOTOR_STA_2B, 3);
    }
    
    //Motor ON conditions
    // -- No - immediate motor OFF requested by User
    // -- Function shall not switch ON motor if power failure during motor ON last ECU cycle
    if ((FALSE == BitGet(ButtonFlaggs, MOTOR_OFF_REQ_1B, 1))
        && (M_STA_IDLE == BitGet(ButtonFlaggs, MOTOR_STA_2B, 3)))
    {
        RteSet(GIO_0, MOTOR_CONTROL_1B, 1);
        //Stay ON (Red)      : Motor ON
        RteSet(GIO_0, LED_RED_1B, 1);
        RteClr(GIO_0, LED_GREEN_1B, 1);
        TimerRunType[AL_MODULE_OFF] = TIMER_STOP;
        IL_RteTimer[AL_MODULE_OFF] = 0;
        
        if (FALSE == BitGet(ButtonFlaggs, MOTOR_STA_LOC_1B, 1)){
            TimerRunType[AL_MOTOR_ON] = TIMER_START;
            if (IL_RteTimer[AL_MOTOR_ON] > MOTOR_ON_VALIDATE_TIME){
                (void)IL_NvmWrite(EEPROM_MOTOR_STA, M_STA_ON);           
                BitSet(ButtonFlaggs, MOTOR_STA_LOC_1B, 1);
                TimerRunType[AL_MOTOR_ON] = TIMER_STOP;
                IL_RteTimer[AL_MOTOR_ON] = 0;
            }
        }else if (((CalibTimeCounter + 1) == (CalibValue)) 
                && ((IL_RteTimer[AL_CALIB] + FACTOR_1SEC) == (FACTOR_30SEC))){
            (void)IL_NvmWrite(EEPROM_MOTOR_STA, M_STA_OFF); 
            IL_RteTimer[AL_MOTOR_ON] = 0;
        }
    }else{
        RteClr(GIO_0, MOTOR_CONTROL_1B, 1);
        
        if (TRUE == BitGet(ButtonFlaggs, MOTOR_STA_LOC_1B, 1)){
            TimerRunType[AL_MOTOR_ON] = TIMER_START;
            if (IL_RteTimer[AL_MOTOR_ON] > MOTOR_OFF_VALIDATE_TIME){
                (void)IL_NvmWrite(EEPROM_MOTOR_STA, M_STA_IDLE); 
                TimerRunType[AL_MOTOR_ON] = TIMER_STOP;
                BitClr(ButtonFlaggs, MOTOR_STA_LOC_1B, 1);
            }
        }
        
        if (FALSE == BitGet(ButtonFlaggs, CALIB_REQ_1B, 1)){
            //Stay ON (Green)    : Normal mode
            RteClr(GIO_0, LED_RED_1B, 1);
            RteSet(GIO_0, LED_GREEN_1B, 1);
            TimerRunType[AL_MODULE_OFF] = TIMER_START;
            //Function shall switch OFF module after 5sec of Motor OFF and no activation of 'MOTOR_OFF switch'
            if (IL_RteTimer[AL_MODULE_OFF] > MODULE_OFF_TIME){
                RteSet(GIO_0, OFF_CONTROL_1B, 1);
            }
        }
#ifdef AL_USE_CALIBRATION
        else{            
            IL_RteTimer[AL_MODULE_OFF] = 0;
            TimerRunType[AL_MODULE_OFF] = TIMER_STOP;
            //Stay ON (Orange)   : Calibration mode
            RteSet(GIO_0, LED_GREEN_1B, 1);
            //Short Pulse(Orange): Calibration timer reach every 30sec
            if (IL_RteTimer[AL_CALIB_PRESS] < CALIB_PULSE_TIME){
                RteClr(GIO_0, LED_RED_1B, 1); 
            }else{
                RteSet(GIO_0, LED_RED_1B, 1);   
                TimerRunType[AL_CALIB_PRESS] = TIMER_STOP;     
            }
        }        
#endif //AL_USE_CALIBRATION
        CalibTimeCounter = 0;
    }  
}


/* Description: This function will do button press operation for long and short press
 * Arguments: 
 *    ButtonShortCntrPtr -[inout]: Counter pointer for short press timeout
 *    ButtonLongCntrPtr -[inout] : Counter pointer for long press timeout
 *    ChangeCnt     -[out]       : Data pointer for change when button pressed
 *    FlagPos       -[in]        : POsition of button flag in variable ButtonFlaggs for
 *                                 identify button 
 *    HwBit         -[in]        : Hardware pin status , it might be toggle or de-bounce
 *    HwBit_Evaluate-[in]        : Hardware pin evaluation state [TRUE, FALSE]
 *    Type          -[in]        : It's for identify nature of button activity 
 */

////////////////////////////////////////////////////////////////////////////////////////////////////////
//                           Key debounce                                           100 ms            //
//                         |<------------->|                                       |<---->|           //
//                 		                                                                              //
//                         |---|   |---|   |---------------------------------------|                  //
//                         |   |   |   |   |                                       |                  //
// Button H/W      --------|   |---|   |---|                                       |-------------     //
//                                                                                                    //
//                         |--------------------------------------------------------------|           //
//                         |                                                              |           //
// Manual Motor ON --------|                         2 sec                                |------     //
//                                         |<--------------------->|                                  //
//                                                                 |----------------------|           //
//                                                                 |                      |           //
// Emergancy ON    ------------------------------------------------|                      |------     //
//                                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////

static UINT8 ButtonPress(UINT8 FlagPos,
        Button_Type_t Type,
        UINT8 HwBit,
        UINT8 HwBit_Evaluate,
        UINT8 *ButtonShortCntrPtr,
        UINT16 *ButtonLongCntrPtr) 
{
    UINT8 RetVal = BUT_PRESS_NONE;

    if ((HwBit_Evaluate == HwBit) &&
        (
            (BUT_TYPE_SHORT_LOOP != Type) ||
            (FALSE == BitGet(ButtonFlaggs, FlagPos, 1))
            )
            ) 
    {
        if (BUT_TYPE_SHORT_LOOP == Type) {
            *ButtonShortCntrPtr = BUTTON_LOOP_DELY;
        } else {
            *ButtonShortCntrPtr = BUTTON_DELY;
        }
        BitSet(ButtonFlaggs, FlagPos, 1);
    } else if ((*ButtonShortCntrPtr) > 0) {
        (*ButtonShortCntrPtr)--;
    } else {
        (*ButtonLongCntrPtr) = 0;
        (*ButtonShortCntrPtr) = 0;
        BitClr(ButtonFlaggs, FlagPos, 1);
    }

    if (TRUE == BitGet(ButtonFlaggs, FlagPos, 1)) 
    {
        if ((*ButtonShortCntrPtr) > 0) 
        {
            (*ButtonShortCntrPtr)--;
            (*ButtonLongCntrPtr)++;

            if ((*ButtonLongCntrPtr) > BUTTON_ERROR_DELY) {
                RetVal = BUT_PRESS_ERROR;
            } else if ((*ButtonLongCntrPtr) == BUTTON_LONG_DELY) {
                RetVal = BUT_PRESS_LONG;
            }
        } else {
            BitClr(ButtonFlaggs, FlagPos, 1);
            (*ButtonLongCntrPtr) = 0;

            RetVal = BUT_PRESS_SHORT;
        }
    }

    return RetVal;

}//ButtonPress

#endif //AL_MAINCTRL