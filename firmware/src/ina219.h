#pragma once

// Initialise the INA219 power sensor over I2C and start a background FreeRTOS
// task that logs [POWER] lines every 200 ms.
//
// Safe to call even when no INA219 is wired — init() returns false and the
// task is simply not started.
void ina219_init();
