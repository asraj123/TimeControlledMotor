# Time Controlled Motor
A simple, cheap and power efficient solution for automatic water pump based on time.

Module will switch ON motor for configured time in EEPROM and switch OFF power itself in idle time. Module will switch OFF module after 5sec of Motor OFF and no activation of 'MOTOR_OFF switch'.

Module will indicate status by LED as following:
 * -- OFF                : Module OFF
 * -- Stay ON (Green)    : Normal mode
 * -- Stay ON (Red)      : Motor ON
 * -- Stay ON (Orange)   : Calibration mode
 * -- Short Pulse(Orange): Calibration timer reach every 30sec
  
Module will not switch ON motor if power failure during motor ON last ECU cycle, this is managed by state machine with following state at motor and status will be stored at EEPROM
 * -- Idle state: motor ready to ON and no power failure in last time
 * -- On state: there is a power failure in last motor ON (User shall clear this by OFF switch)
 * -- Off state: Motor ready to OFF (User shall clear this by OFF switch)
 
#**Controller**: PIC12F675

#**Software**: C with Hitech C8 compiler with 'Mplab X' IDE

#**OS**: lightweight round robin OS with 5ms task schedule.
