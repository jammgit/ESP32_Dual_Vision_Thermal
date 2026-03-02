#pragma once
#include <Arduino.h>
#include "screen.hpp"
#include "file_system.hpp"
#include "camera.hpp"
#include "fusion.hpp"

void serial_start() {
  Serial.begin(115200);
  while (!Serial) {
    ; // 等待串口连接
  }
  Serial.println("Serial communication initialized.");
}

// 处理设置对齐参数命令
void handle_set_align_command(String input);
// 处理设置透明度命令
void handle_set_alpha_command(String input);
// 处理获取对齐参数命令
void handle_get_align_command();

// Supports 'h' for help menu
// Supports 'echo' for echoing input
// Supports 'screen' commands for display control
void serial_loop(){
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim(); // Remove leading and trailing whitespaces
        
        if (input == "h" || input == "help") {
            // ==== English Help Menu ====
            Serial.println("\r=========================================");
            Serial.println("          ESP32 Serial Console           ");
            Serial.println("=========================================");
            Serial.println("[ General Commands ]");
            Serial.println("  h                     - Show this help message");
            Serial.println("  echo <message>        - Echo the input message back to you");
            Serial.println("  top                   - Show heap usage");
            Serial.println("");
            Serial.println("[ Screen Control ]");
            Serial.println("  screen on             - Turn on the screen smoothly");
            Serial.println("  screen off            - Turn off the screen smoothly");
            Serial.println("  screen brightness <X> - Set brightness level (X: 5~255)");
            Serial.println("  color_reverse <opt>   - Color inversion control");
            Serial.println("                         1: enable, 0: disable, -q: query, -r: toggle");
            Serial.println("");
            Serial.println("[ File System Commands ]");
            Serial.println("  ls [path]             - List directory contents");
            Serial.println("  rm <filename>         - Remove a file");
            Serial.println("  cat <filename>        - Display file contents");
            Serial.println("  df                    - Show disk usage");
            Serial.println("");
            Serial.println("[ Camera Commands ]");
            Serial.println("  check_camera          - Check camera I2C and init status");
            Serial.println("  test_camera           - Test camera frame capture");
            Serial.println("  toggle_vflip          - Toggle vertical flip");
            Serial.println("  toggle_hflip          - Toggle horizontal mirror");
            Serial.println("  set_flip <v> <h>      - Set flip (v: 0/1, h: 0/1)");
            Serial.println("");
            Serial.println("[ Fusion Commands ]");
            Serial.println("  set_align <tx> <ty> <sx> <sy> <ang>");
            Serial.println("                        - Set alignment parameters");
            Serial.println("  get_align             - Get current alignment params");
            Serial.println("  set_alpha <val>       - Set fusion alpha (0~255)");
            Serial.println("  toggle_fusion         - Toggle fusion on/off");
            Serial.println("  enable_fusion         - Enable fusion");
            Serial.println("  disable_fusion        - Disable fusion");
            Serial.println("=========================================\r");
            
        } else if (input.startsWith("echo ")) {
            String message = input.substring(5); // Extract message after "echo "
            Serial.println("Echo: " + message);
            
        } else if (input.startsWith("screen ") || input.startsWith("color_reverse ")) {  // Intercept screen control commands
            screen_cli(input);
        } else if (input.startsWith("top")) {  // Intercept screen control commands
            print_heap_usage();
        } else if (input.startsWith("ls") || input.startsWith("rm") || 
                   input.startsWith("cat") || input.startsWith("df")) {
            // File system commands
            fs_cli(input);
        } else if (input.startsWith("toggle_vflip") || input.startsWith("toggle_hflip") || 
                   input.startsWith("set_flip") || input.startsWith("check_camera") ||
                   input.startsWith("test_camera")) {
            // Camera commands
            camera_cli(input);
        } else if (input.startsWith("set_align ")) {
            // Set alignment parameters
            handle_set_align_command(input);
        } else if (input.startsWith("get_align")) {
            // Get alignment parameters
            handle_get_align_command();
        } else if (input.startsWith("set_alpha ")) {
            // Set fusion alpha
            handle_set_alpha_command(input);
        } else if (input.startsWith("toggle_fusion")) {
            // Toggle fusion on/off
            fusion_toggle();
            save_align_params();
        } else if (input.startsWith("enable_fusion")) {
            // Enable fusion
            fusion_set_enabled(true);
            save_align_params();
        } else if (input.startsWith("disable_fusion")) {
            // Disable fusion
            fusion_set_enabled(false);
            save_align_params();
        } else if (input.length() > 0) { // Prevent blank enter keys from triggering unknown command
            Serial.println("Unknown command: '" + input + "'. Type 'h' for help.");
        }
    }
}

// 处理设置对齐参数命令
void handle_set_align_command(String cmd) {
    float params[5];
    int start = 0;
    int index = 0;
    
    cmd = cmd.substring(10); // 移除 "set_align "
    cmd.trim();
    
    while (index < 5) {
        int spaceIndex = cmd.indexOf(' ', start);
        if (spaceIndex == -1) spaceIndex = cmd.length();
        String p = cmd.substring(start, spaceIndex);
        params[index++] = p.toFloat();
        start = spaceIndex + 1;
        if (start >= cmd.length()) break;
    }

    if (index >= 5) {
        align_tx = params[0];
        align_ty = params[1];
        align_sx = params[2];
        align_sy = params[3];
        align_ang = params[4];
        save_align_params(); // 保存
        Serial.println("OK: Align params updated");
    } else {
        Serial.println("Err: Invalid align params. Usage: set_align <tx> <ty> <sx> <sy> <ang>");
    }
}

// 处理设置透明度命令
void handle_set_alpha_command(String cmd) {
    cmd = cmd.substring(10); // 移除 "set_alpha "
    cmd.trim();
    int val = cmd.toInt();
    
    if (val < 0) val = 0;
    if (val > 255) val = 255;
    
    fusion_alpha = (uint8_t)val;
    save_align_params(); // 保存
    Serial.printf("OK: Alpha set to %d\n", fusion_alpha);
}

// 处理获取对齐参数命令
void handle_get_align_command() {
    // 回传当前参数，方便上位机同步
    Serial.printf("ALIGN %.2f %.2f %.3f %.3f %.2f %d %d\n", 
        align_tx, align_ty, align_sx, align_sy, align_ang, fusion_alpha, enable_fusion ? 1 : 0);
}
