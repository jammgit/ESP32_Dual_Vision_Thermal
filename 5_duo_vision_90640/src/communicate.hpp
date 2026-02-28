#pragma once
#include <Arduino.h>
#include "screen.hpp"
#include "fusion.hpp"
#include "file_system.hpp"
#include "camera.hpp"

void serial_start() {
  Serial.begin(115200);
  while (!Serial) {
    ; // 等待串口连接
  }
  Serial.println("Serial communication initialized.");
  // 初始化文件系统
  fs_init();
  // 加载对齐参数
  load_align_params();
}

// 应用摄像头翻转
void camera_apply_flip() {
    if (!camera_ok) return;
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_vflip(s, camera_vflip);
        s->set_hmirror(s, camera_hmirror);
    }
}

// 设置摄像头翻转
void camera_set_flip(bool vflip, bool hmirror) {
    camera_vflip = vflip;
    camera_hmirror = hmirror;
    camera_apply_flip();
    save_align_params();
}

// Supports 'h' for help menu
// Supports 'echo' for echoing input
// Supports 'screen' commands for display control
// Supports 'mode' commands for fusion mode switching
// Supports 'set_align', 'get_align', 'set_alpha' commands for affine transform
// Supports 'ls', 'rm', 'df' commands for file system operations
void serial_loop(){
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim(); // Remove leading and trailing whitespaces

        if (input == "h") {
            // ==== English Help Menu ====
            Serial.println("\r\n=========================================");
            Serial.println("          ESP32 Serial Console           ");
            Serial.println("=========================================");
            Serial.println("[ General Commands ]");
            Serial.println("  h                     - Show this help message");
            Serial.println("  echo <message>        - Echo the input message back to you");
            Serial.println("");
            Serial.println("[ Screen Control ]");
            Serial.println("  screen on             - Turn on the screen smoothly");
            Serial.println("  screen off            - Turn off the screen smoothly");
            Serial.println("  screen brightness <X> - Set brightness level (X: 5~255)");
            Serial.println("");
            Serial.println("[ Display Mode (Fusion) - Auto-saved to EEPROM ]");
            Serial.println("  mode                  - Show current display mode");
            Serial.println("  mode next             - Switch to next display mode");
            Serial.println("  mode <0-4>            - Set specific mode:");
            Serial.println("                          0=Thermal Only");
            Serial.println("                          1=Camera Only");
            Serial.println("                          2=PiP (Camera main + Thermal sub) [DEFAULT]");
            Serial.println("                          3=PiP (Thermal main + Camera sub)");
            Serial.println("                          4=Thermal Overlay on Camera");
            Serial.println("");
            Serial.println("[ File System Commands ]");
            Serial.println("  ls                    - List files in root directory");
            Serial.println("  rm <filename>         - Remove a file");
            Serial.println("  df                    - Show disk usage information");
            Serial.println("");
            Serial.println("[ Fusion Configuration ]");
            Serial.println("  set_align tx ty sx sy ang - Set alignment params");
            Serial.println("  get_align             - Get current alignment params");
            Serial.println("  set_alpha val         - Set fusion transparency (0-255)");
            Serial.println("  toggle_vflip          - Toggle vertical flip");
            Serial.println("  toggle_hflip          - Toggle horizontal flip");
            Serial.println("Note: Mode and align params are auto-saved and restored on boot.");
            Serial.println("=========================================\r\n");

        } else if (input.startsWith("echo ")) {
            String message = input.substring(5); // Extract message after "echo "
            Serial.println("Echo: " + message);

        } else if (input.startsWith("screen ") || input.startsWith("color_reverse ")) {  // Intercept screen control commands
            screen_cli(input);

        } else if (input == "mode") {
            Serial.printf("[Mode] Current: %s (%d)\n", get_display_mode_name(), current_display_mode);

        } else if (input == "mode next") {
            next_display_mode();
            Serial.printf("[Mode] Switched to: %s (%d)\n", get_display_mode_name(), current_display_mode);

        } else if (input.startsWith("mode ")) {
            String mode_str = input.substring(5);
            int mode = mode_str.toInt();
            if (mode >= 0 && mode <= 4) {
                set_display_mode((DisplayMode)mode);
                Serial.printf("[Mode] Set to: %s (%d)\n", get_display_mode_name(), current_display_mode);
            } else {
                Serial.println("[Error] Invalid mode. Use 0-4 or 'next'.");
            }

        } else if (input.startsWith("ls") || input.startsWith("rm") || input.startsWith("df")) {
            fs_cli(input);

        } else if (input.startsWith("set_align")) {
            handle_set_align(input);

        } else if (input.startsWith("get_align")) {
            Serial.printf("ALIGN %.2f %.2f %.3f %.3f %.2f %d\n",
                align_tx, align_ty, align_sx, align_sy, align_ang, fusion_alpha);

        } else if (input.startsWith("set_alpha")) {
            handle_set_alpha(input);

        } else if (input.startsWith("toggle_vflip")) {
            camera_vflip = !camera_vflip;
            camera_apply_flip();
            save_align_params();
            Serial.printf("VFLIP:%d\n", camera_vflip);

        } else if (input.startsWith("toggle_hflip")) {
            camera_hmirror = !camera_hmirror;
            camera_apply_flip();
            save_align_params();
            Serial.printf("HFLIP:%d\n", camera_hmirror);

        } else if (input.startsWith("set_flip")) {
            int v = 0, h = 0;
            sscanf(input.c_str(), "set_flip %d %d", &v, &h);
            camera_set_flip(v == 1, h == 1);
            Serial.printf("OK: Flip set to V:%d H:%d\n", v, h);

        }else if (input.startsWith("top")) {
            print_heap_usage();

        } else if (input.length() > 0) { // Prevent blank enter keys from triggering unknown command
            Serial.println("Unknown command: '" + input + "'. Type 'h' for help.");
        }
    }
}