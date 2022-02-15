#include "mks_esp_test.h"

#include "../inc/MarlinConfigPre.h"
#include "../gcode/queue.h"
#include "../pins/esp32/pins_MKS_TINYBEE.h"
#include "../gcode/gcode.h"
#include "../module/motion.h"
#include "../module/planner.h"
#include "../module/temperature.h"
#if BOTH(ESP32, MKS_TEST)


#define MKS_TEST_TASK_RUNNING_PRIORITY          2
#define MKS_TEST_RUNNING_CORE                   1   // must run same with marlin

bool step_dir_ctrl = false;

void my_delay(volatile uint32_t tick) {
    while(tick) {
        tick--;
    }
}


void tinybee_io_test_init(void) {

    pinMode(HEATER_0_PIN, OUTPUT);
    pinMode(HEATER_1_PIN_T, OUTPUT);
    pinMode(FAN_PIN_T, OUTPUT);
    pinMode(FAN1_PIN_T, OUTPUT);
    pinMode(HEATER_BED_PIN_T, OUTPUT);

    SET_INPUT_PULLUP(X_STOP_PIN);
    SET_INPUT_PULLUP(Y_STOP_PIN);
    SET_INPUT_PULLUP(Z_STOP_PIN);
    SET_INPUT_PULLUP(35);
}

uint32_t tick_start = 0;
uint32_t tick_end = 0;

bool is_limit_is_err = false;
bool step_dir = false;

void test_step_run() {

    tick_end = millis();
    
    if(tick_end - tick_start >= 100) {

        if(queue.has_commands_queued())  return;

        if(is_limit_is_err == false) return;

        if(step_dir) {
                queue.enqueue_now_P(PSTR("G1X50Y50E10F8000"));
                queue.enqueue_now_P(PSTR("G1Z50 F8000"));
                step_dir = false;
        }else{
                queue.enqueue_now_P(PSTR("G1X0Y0E0F8000"));
                queue.enqueue_now_P(PSTR("G1Z0 F8000"));
                step_dir = true;
        }

        tick_start = tick_end;
    }
}

bool tintbee_limit_read(void) {

    if ( 
        (READ(X_STOP_PIN) == false) &&
        (READ(Y_STOP_PIN) == false) &&
        (READ(Z_STOP_PIN) == false) &&
        (READ(35) == false)) {
        
        is_limit_is_err = true;
    }else {
        is_limit_is_err = false;
    }

    return true;
}


void tinbee_test_task(void *parg) {

    disableCore0WDT();
    disableCore1WDT();

    SERIAL_ECHO_MSG("enter test task\n");
    
    tinybee_io_test_init();

    delay(1000);        // 让出1s的时间等待初始化

    while(1) {
        tintbee_limit_read();

        if(step_dir_ctrl) {
            // WRITE(HEATER_0_PIN, HIGH);
            WRITE(HEATER_1_PIN_T, HIGH);
            WRITE(FAN_PIN_T, HIGH);
            WRITE(FAN1_PIN_T, HIGH);
            thermalManager.temp_bed.target = 100;
            thermalManager.temp_hotend[0].target = 280;
            step_dir_ctrl = false;
        }else {
            // WRITE(HEATER_0_PIN, LOW);
            WRITE(HEATER_1_PIN_T, LOW);
            WRITE(FAN_PIN_T, LOW);
            WRITE(FAN1_PIN_T, LOW);
            // WRITE(HEATER_BED_PIN_T, LOW);
            thermalManager.temp_bed.target = 0;
            thermalManager.temp_hotend[0].target = 0;
            step_dir_ctrl = true;
        }
        delay(5000);
    }
}

void tinybee_test(void) {

    xTaskCreatePinnedToCore(
        tinbee_test_task, /* Task function. */
        "MKS TEST TASK", /* name of task. */
        8192*2, /* Stack size of task */
        NULL, /* parameter of the task */
        MKS_TEST_TASK_RUNNING_PRIORITY, /* priority of the task */
        NULL, /* Task handle to keep track of created task */
        MKS_TEST_RUNNING_CORE    /* Core to run the task */
    );
}

#endif
