#include <Arduino.h>
#include <EEPROM.h>
#include "communicate.hpp"
#include "screen.hpp"
#include "shared_val.hpp"
#include "draw.hpp"
#include "bat.hpp"
#include "button.hpp"
#include "cal.h"
#include "probe/heimann_driver.hpp"

void setup1(){
  while (config_loaded == false){delay(100);}
  sensor_power_on();
  sensor_init();
  prob_status = PROB_READY;
  flag_sensor_ok = true;
}

void loop1(){
  sensor_loop();
}

// 定义任务句柄
TaskHandle_t Task1;
void vTaskCore0(void * pvParameters){
  setup1();
  for(;;){
    loop1();
  }
}

void setup() {
  serial_start();
  EEPROM.begin(512);  // 初始化 EEPROM
  color_reverse = EEPROM.read(0) != 0; // 非零值转换为 true，零值转换为 false
  
  // 初始化双线性插值查找表
  bilinear_init();

  // 在 Core 0 上创建任务
  xTaskCreatePinnedToCore(
                    vTaskCore0,   /* 任务函数 */
                    "vTaskCore0",     /* 任务名称 */
                    20000,       /* 堆栈大小 - 增加到 20KB 防止栈溢出 */
                    NULL,        /* 参数 */
                    1,           /* 优先级 */
                    &Task1,      /* 任务句柄 */
                    0);          /* 指定核心: 0 */
  bat_init();
  button_init();
  screen_init();
  fsInit();
  readCalFile();
  delay(100);
  config_loaded = true;
  while (flag_sensor_ok==false) {preparing_loop();delay(5);}
  // refresh_status();
  smooth_off();
  tft.fillScreen(TFT_BLACK);
  screen_loop();
  smooth_on();
}

void loop() {
  serial_loop();
  bat_loop();
  screen_loop();      // 渲染屏幕
  button_loop();
  delay(5);
}