#ifndef DWAR_H
#define DWAR_H

#include <Arduino.h>
#include "screen.hpp"
#include "shared_val.hpp"
#include "probe/heimann_driver.hpp"
#include "BilinearInterpolation.h"
#include "color_map.hpp"
#include "kalman_filter.h"
#include "camera.hpp"
#include "fusion.hpp"


#define PROB_SCALE 8
// Note: draw_pixel is defined in shared_val.hpp
// extern unsigned short draw_pixel[PIXEL_PER_COLUMN][PIXEL_PER_ROW];
int R_colour, G_colour, B_colour; 
float ft_point; // 屏幕光标点的温度值
const int biox = 30;
const int bioy = 28;
const int lines = 3;  // 一次渲染多少行的像素
uint16_t  lineBuffer[biox * PROB_SCALE * lines]; // Toggle buffer for lines
uint16_t  dmaBuffer1[biox * PROB_SCALE * lines]; // Toggle buffer for lines
uint16_t  dmaBuffer2[biox * PROB_SCALE * lines]; // Toggle buffer for lines
uint16_t* dmaBufferPtr = dmaBuffer1;
bool dmaBufferSel = 0;
unsigned short max_posx = 0;
unsigned short max_posy = 0;
KalmanDK kalman_matrix[32][32];  // 1024 个滤波器
KalmanDK kfp_diff;              // 用于过滤温度差的卡尔曼滤波器
bool kalman_matrix_inited = false;
inline float temp_cal(float input){
   if (input > calibrate0_start && input < calibrate0_end){
      return calibrate0_weight * input + calibrate0_bias;
   }else if (input > calibrate1_start && input < calibrate1_end){
      return calibrate1_weight * input + calibrate1_bias;
   }else{
      return input;
   }
}

// 绘制十字
inline void draw_cross(int x, int y, int len, bool widther = false){
   
   tft.drawLine(x - len/2, y, x + len/2, y, tft.color565(255, 255, 255));
   tft.drawLine(x, y-len/2, x, y+len/2,  tft.color565(255, 255, 255));
   if (widther){
      tft.drawLine(x - len/2, y+1, x + len/2, y+1, tft.color565(255, 255, 255));
      tft.drawLine(x+1, y-len/2, x+1, y+len/2,  tft.color565(255, 255, 255));
      tft.drawLine(x - len/2, y-1, x + len/2, y-1, tft.color565(255, 255, 255));
      tft.drawLine(x-1, y-len/2, x-1, y+len/2,  tft.color565(255, 255, 255));

      tft.drawLine(x - len/4, y+1, x + len/4, y-1, tft.color565(255, 0, 0));
      tft.drawLine(x+1, y-len/4, x+1, y+len/4,  tft.color565(255, 0, 0));
      tft.drawLine(x - len/4, y-1, x + len/4, y-1, tft.color565(255, 0, 0));
      tft.drawLine(x-1, y-len/4, x-1, y+len/4,  tft.color565(255, 0, 0));
   }

   tft.drawLine(x - len/4, y, x + len/4, y, tft.color565(0, 0, 0));
   tft.drawLine(x, y-len/4, x, y+len/4,  tft.color565(0, 0, 0));
}


// 点测温功能
inline void show_local_temp(float num, int x, int y, int cursor_size){
   tft.setRotation(1);
   draw_cross(x, y, 8);
   static short temp_xy;
   static int shift_x, shift_y;
   if (x<140){shift_x=10;} else {shift_x=-60;}
   if (y<120){shift_y=10;} else {shift_y=-20;}
   tft.setTextSize(cursor_size);
   tft.setCursor(x+shift_x, y+shift_y);
   tft.printf("%.2f", num);
}  

// 点测温功能
inline void show_local_temp(float num, int x, int y){
   show_local_temp(num, x, y, 2);
}  

// 判断应该在什么时候渲染光标（光标位置在当前渲染行数-40行的时候）
// 这么做是为了让光标别闪
inline void insert_temp_cursor(int y){
   static int trig_line;
   tft.setRotation(1);
   trig_line = test_point[0] + 80;
   if (trig_line>215){trig_line = 215;}
   if (y==trig_line){
      if (flag_show_cursor==true) {show_local_temp(ft_point, test_point[0], test_point[1]);}
   }
   tft.setRotation(2);
}


// 判断应该在什么时候渲染光标（光标位置在当前渲染行数-40行的时候）
// 这么做是为了让光标别闪
inline void insert_max_cursor(int y){
   static short trig_line;
   tft.setRotation(1);
   trig_line = (256 - max_posx) + 30;
   if (trig_line>255){trig_line = 255;}
   else if (trig_line<0){trig_line = 0;}
   if (y==trig_line){
      if (flag_show_cursor==true) {
         draw_cross(max_posx, max_posy, 10, true);
      }
   }
   tft.setRotation(2);
}

inline void draw_float_num(int x, int y, float num){
      // 定义两个字符串来存储小数点前后的部分
  char integerPart[5];
  char fractionalPart[5];
    // 使用 sprintf 格式化字符串
  sprintf(integerPart, "%d", (int)num); // 获取整数部分
  sprintf(fractionalPart, "%02d", (int)((num - (int)num) * 100)); // 获取小数部分
    tft.setCursor(x, y);
    tft.printf("%s.", integerPart);
    tft.setCursor(x+10, y+15);
    tft.printf("%s", fractionalPart);
}

inline void draw_percent_num(int x, int y, uint8_t num){
      // 定义两个字符串来存储小数点前后的部分
    tft.setCursor(x, y);
    tft.printf("%d%% ", num);

}


void draw(){
      static int value;
      static int now_y = 0;

      if(use_upsample){
      tft.setRotation(2);
      // 只渲染 240x224 区域 (30*8 x 28*8)
      for(int y=0; y<28 * PROB_SCALE; y++){ 
         for(int x=0; x<30 * PROB_SCALE; x++){
            value = bio_linear_interpolation(x, y, draw_pixel);
            lineBuffer[x] = colormap[value];
         }
         // 使用普通的 pushImage 代替 DMA
         tft.pushImage(0, y, biox*PROB_SCALE, 1, lineBuffer);
         insert_temp_cursor(y);
         insert_max_cursor(y);
      }
      tft.setRotation(0);
   }else{
    static uint16_t c565;
    tft.setRotation(2);
    // 只渲染 30x28 区域
    for (int i = 0 ; i < 30 ; i++){
    for (int j = 0; j < 28; j++){
         c565 = colormap[(int)draw_pixel[i][j]];
         tft.fillRect(i*PROB_SCALE, j*PROB_SCALE, PROB_SCALE, PROB_SCALE, c565);  
    }
    }
   }
   tft.setRotation(0);
}

// 用来处理画面被暂停时的热成像图层的渲染工作
void freeze_handeler(){
   // 仅拍照模式下，位于第一屏时会启用这个功能
   if (flag_clear_cursor) {draw(); flag_clear_cursor=false;} // 通过重新渲染一张画面来清除光标
   if (flag_show_cursor) {
      show_local_temp(ft_point, test_point[0], test_point[1]);
   } // 每次点击都渲染光标位置
}


// 探头准备期间的渲染管线
void preparing_loop(){
   tft.setRotation(1);
   static uint8_t last_status = 0xFF;
   const int left_x = 5;
   const int bottom_y = 230;  // 最底部一行
   
   // 只清除底部一行（高度13像素）
   if (prob_status != last_status) {
      tft.fillRect(left_x, bottom_y - 2, 200, 13, TFT_BLACK);
      last_status = prob_status;
   }
   
   tft.setTextColor(TFT_WHITE, TFT_BLACK, true);
   tft.setTextSize(1);
   tft.setCursor(left_x, bottom_y);
   
   static uint8_t spin = 0;
   const char* spinner = "|/-\\";
   
   if (prob_status == PROB_CONNECTING) {
      // 阶段1: 连接中
      tft.printf("[%c] Connecting...  [ ] [ ]", spinner[spin++ & 3]);
   } 
   else if (prob_status == PROB_INITIALIZING) {
      // 阶段2: 已连接
      tft.printf("[O] Connected(0x%02X) [%c] [ ]", SENSOR_ADDRESS, spinner[spin++ & 3]);
   }
   else if (prob_status == PROB_PREPARING) {
      // 阶段3: 就绪
      tft.printf("[O] Connected(0x%02X) [O] [%c]", SENSOR_ADDRESS, spinner[spin++ & 3]);
   }
   else if (prob_status == PROB_READY) {
      // 阶段4: 全部就绪
      tft.printf("[O] Connected(0x%02X) [O] [O] Ready!", SENSOR_ADDRESS);
      delay(500);
   }
   delay(10);
}

// 探头准备期间的渲染管线
void refresh_status(){
   tft.setCursor(20, 200);
   tft.setTextColor(TFT_WHITE, TFT_BLACK, true);
   tft.setTextSize(1);
   tft.fillRect(20, 200, 240, 30, TFT_BLACK); 
   tft.printf("prepering touch panel...");
   delay(10);
}

// 处理tft_espi渲染管线
void screen_loop(){
   static uint8_t count_max_flash = 0;
   static unsigned short diff = 1;  // 初始值避免除零，实际值在锁保护下获取
   if (! kalman_matrix_inited){  // 卡尔曼矩阵初始化
      pix_cp_lock = true;
      KalmanMatrix_Init(kalman_matrix, data_pixel);
      Kalman_Init(&kfp_diff, T_max - T_min + 1, 20);
      pix_cp_lock = false; 
      kalman_matrix_inited = true;
   }
   if(!flag_in_photo_mode){
      unsigned short value;
      // 等待传感器释放锁，然后获取 pix_cp_lock 保护数据访问
      while (prob_lock == true) {delay(5);}
      pix_cp_lock = true;
      // 在锁保护下读取 T_max/T_min
      float ft_max = float(T_max) / 10 - 273.15;
      float ft_min = float(T_min) / 10 - 273.15;
      static unsigned short diff;
      diff = T_max - T_min + 1;
      ft_point = (float)(data_pixel[30-(test_point[1] / PROB_SCALE)][30-(test_point[0] / PROB_SCALE)] / 10.) - 273.15;
      ft_point = temp_cal(ft_point);
      for (int i = 0; i < 32; i++) {
      for (int j = 0; j < 32; j++) {
         if( flag_use_kalman ){
            value = Kalman_Update(&kalman_matrix[i][j], data_pixel[i][j]);
            diff = Kalman_Update(&kfp_diff, T_max - T_min);
         }else{
            value = data_pixel[i][j];
            diff = T_max - T_min + 1;
         }
         if (value < T_min){
            value = T_min;
         }
         value = (180 * (value - T_min) / (diff));
         if (value >= 180 ) {
            value=179;
         }
         draw_pixel[i][j] = value;
      }
      }
      pix_cp_lock = false;
      while (cmap_loading_lock == true) {delay(1);} // 拷贝温度信息, 并提前映射到色彩空间中
      
      // 如果启用了融合且有摄像头数据，使用融合渲染
      static bool last_fusion_state = false;
      bool fusion_ready = enable_fusion && fusion_initialized && is_buf_init && 
                          rgb_buffer != nullptr && camera_ok && camera_calibrated;
      
      if (fusion_ready != last_fusion_state) {
         Serial.printf("[Fusion] State: enable=%d, init=%d, buf_init=%d, rgb_buf=%p, camera_ok=%d, calibrated=%d\n", 
                       enable_fusion, fusion_initialized, is_buf_init, rgb_buffer, camera_ok, camera_calibrated);
         last_fusion_state = fusion_ready;
      }
      
      if (fusion_ready) {
         // 先获取摄像头帧并解码到 rgb_buffer
         camera_loop();
         // 执行融合并推送
         composite_and_push_fusion((uint16_t*)rgb_buffer);
      } else {
         // 普通渲染（无融合）
         draw();
      }
      
      // 绘制右侧信息栏（不在融合区域内）
      tft.setRotation(1);
      tft.setTextSize(2);
      tft.setCursor(258, 32);
      tft.setTextColor(TFT_RED);
      tft.printf("max");

      tft.setTextColor(TFT_BLUE);
      tft.setCursor(258, 110);
      tft.printf("min");
      
      tft.setTextColor(TFT_GREEN);
      tft.setCursor(258, 195);
      tft.printf("Bat");
      tft.setTextColor(TFT_WHITE, TFT_BLACK, true);
      draw_float_num(258, 52, ft_max);
      draw_float_num(258, 130, ft_min);
      draw_percent_num(258, 215, vbat_percent);
      
      if (flag_show_cursor==true) {
         show_local_temp(ft_point, test_point[0], test_point[1]);
         if (count_max_flash > 4){
            // draw_cross(x_max * PROB_SCALE, 240-(y_max* PROB_SCALE), 8);
            max_posx = 256 - ((x_max + 2) * PROB_SCALE);
            if (max_posx > 230) {max_posx = 230;} else if (max_posx < 10) {max_posx = 10;}
            max_posy = (y_max* PROB_SCALE);
            if (max_posx > 250) {max_posx = 250;} else if (max_posx < 10) {max_posx = 10;}
            count_max_flash = 0;
         }else{
            count_max_flash++;
         }
      }
   }else{
      freeze_handeler();
   }

}



#endif