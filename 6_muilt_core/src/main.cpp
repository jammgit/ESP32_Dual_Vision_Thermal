#include <Arduino.h>
#include <EEPROM.h>
#include "communicate.hpp"
#include "screen.hpp"
#include "shared_val.hpp"
#include "draw.hpp"
#include "bat.hpp"
#include "button.hpp"
#include "mlx_drivers/mlx_probe.hpp"

void setup1(){
  sensor_power_on();
  EEPROM.begin(512);  // 初始化 EEPROM
  // 设置当前传感器类型
  current_sensor = EEPROM.read(20); // 读取传感器类型
  if (current_sensor > SENSOR_MLX90641){ current_sensor = SENSOR_MLX90640;}
  if (current_sensor == SENSOR_MLX90640) {is_90640 = true;} else {is_90640 = false;}
  // 尝试初始化 MLX 传感器
  delay(MXL_STARTUP_DELAY);
  sensor_power_on();
  blocking_mlx_init_and_check();
  flag_sensor_ok = true;
  prob_status = PROB_PREPARING;
  for(int i = 0; i < 10; i++){
    probe_loop_mlx();
    delay(5);
  }
  prob_status = PROB_READY;
}

void loop1(){
  probe_loop_mlx();   // 读取传感器数据
  delay(1);
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
  delay(100);
  while (prob_status != PROB_READY) {
    preparing_loop();
    delay(10);
  }
  smooth_off();
  screen_loop();
  smooth_on();
}

void loop() {
  serial_loop();
  bat_loop();
  screen_loop();      // 渲染屏幕
  button_loop();
}