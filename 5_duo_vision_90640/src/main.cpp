#include <Arduino.h>
#include <EEPROM.h>
#include "communicate.hpp"
#include "screen.hpp"
#include "shared_val.hpp"
#include "draw.hpp"
#include "bat.hpp"
#include "camera.hpp"
#include "mlx_drivers/mlx_probe.hpp"
#include "fusion.hpp"
#include "button.hpp"
void setup() {
  button_init();
  serial_start();
  bat_init();
  sensor_power_on();
  EEPROM.begin(512);  // 初始化 EEPROM
  screen_init();
  camera_init();
  
  

  // 设置当前传感器类型
  current_sensor = EEPROM.read(EEPROM_ADDR_SENSOR_TYPE); // 读取传感器类型
  if (current_sensor > SENSOR_MLX90641){ current_sensor = SENSOR_MLX90640; }
  if (current_sensor == SENSOR_MLX90640) {is_90640 = true;} else {is_90640 = false;}

  // 更新传感器参数
  update_sensor_params();

  // 从 EEPROM 读取显示模式，如果没有则使用默认融合模式
  DisplayMode saved_mode = load_display_mode_from_eeprom();
  current_display_mode = saved_mode;
  Serial.printf("[Setup] Loaded display mode from EEPROM: %s (%d)\n",
                get_display_mode_name(), current_display_mode);

  // 尝试初始化 MLX 传感器
  sensor_power_on();
  bool mlx_ok = blocking_mlx_init_and_check(5);

  if (mlx_ok) {
    flag_sensor_ok = true;
    prob_status = PROB_READY;
    probe_loop_mlx();
    screen_loop();
    Serial.println("[Setup] MLX Sensor initialized successfully");
  } else {
    Serial.println("[Setup] MLX Sensor initialization failed!");
  }

  // 根据传感器和摄像头状态，智能选择显示模式
  bool cam_ok = camera_ok;

  if (!mlx_ok && !cam_ok) {
    // 两者都失败，只能显示准备状态
    current_display_mode = MODE_THERMAL_ONLY;  // 使用热成像模式显示准备状态
    Serial.println("[Setup] CRITICAL: Both MLX and Camera failed!");
  } else if (!mlx_ok) {
    // 只有热成像失败，切换到仅摄像头模式
    current_display_mode = MODE_CAMERA_ONLY;
    Serial.println("[Setup] MLX failed, fallback to Camera Only mode");
  } else if (!cam_ok) {
    // 只有摄像头失败，切换到纯热成像模式
    current_display_mode = MODE_THERMAL_ONLY;
    Serial.println("[Setup] Camera failed, fallback to Thermal Only mode");
  }
  // 如果两者都正常，保持 EEPROM 中的融合模式

  // 如果是需要摄像头缓冲区的模式，初始化缓冲区
  if (current_display_mode == MODE_PIP_CAMERA) {
    init_fusion_buffers();
  }

  // Alpha 融合模式需要全屏 Sprite
  if (current_display_mode == MODE_THERMAL_OVERLAY) {
    init_fusion_sprite();
  }

  smooth_on();
  Serial.printf("[Setup] Final display mode: %s (%d)\n",
                get_display_mode_name(), current_display_mode);
}

void loop() {
  serial_loop();
  button_loop();
  bat_loop();
  // 根据当前显示模式决定如何渲染
  if (current_display_mode == MODE_CAMERA_ONLY) {
    // 仅摄像头模式：不需要 MLX 传感器
    screen_loop();  // 渲染屏幕（摄像头画面）
  }
  else if (flag_sensor_ok) {
    // 需要 MLX 的模式：读取传感器数据并渲染
    probe_loop_mlx();
    screen_loop();
  }
  else {
    // MLX 未准备好，显示准备状态
    preparing_loop();
    delay(100);
  }
}