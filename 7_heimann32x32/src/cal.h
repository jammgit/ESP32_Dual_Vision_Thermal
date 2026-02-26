#ifndef CAL_H
#define CAL_H

#include <Arduino.h>
#include "shared_val.hpp"
#include <LittleFS.h>
#include <algorithm> 

void fsInit(){
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
        return;
    }Serial.println("LittleFS mount successful");
}


// 线性手动偏移补偿的读取
void readCalFile() {
    File file = LittleFS.open("/cal.txt", "r");
    if (!file) {
        Serial.println("Failed to open file /cal.txt");
        return;
    }

    file.read((uint8_t*)&calibrate0_start, sizeof(calibrate0_start));
    file.read((uint8_t*)&calibrate0_end, sizeof(calibrate0_end));
    file.read((uint8_t*)&calibrate0_weight, sizeof(calibrate0_weight));
    file.read((uint8_t*)&calibrate0_bias, sizeof(calibrate0_bias));

    file.read((uint8_t*)&calibrate1_start, sizeof(calibrate1_start));
    file.read((uint8_t*)&calibrate1_end, sizeof(calibrate1_end));
    file.read((uint8_t*)&calibrate1_weight, sizeof(calibrate1_weight));
    file.read((uint8_t*)&calibrate1_bias, sizeof(calibrate1_bias));

    file.close();
}

void updateCalFile() {
    File file = LittleFS.open("/cal.txt", "w");
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    file.write((uint8_t*)&calibrate0_start, sizeof(calibrate0_start));
    file.write((uint8_t*)&calibrate0_end, sizeof(calibrate0_end));
    file.write((uint8_t*)&calibrate0_weight, sizeof(calibrate0_weight));
    file.write((uint8_t*)&calibrate0_bias, sizeof(calibrate0_bias));

    file.write((uint8_t*)&calibrate1_start, sizeof(calibrate1_start));
    file.write((uint8_t*)&calibrate1_end, sizeof(calibrate1_end));
    file.write((uint8_t*)&calibrate1_weight, sizeof(calibrate1_weight));
    file.write((uint8_t*)&calibrate1_bias, sizeof(calibrate1_bias));

    file.close();
    Serial.println("Calibration values updated in file");
}

void cal_cli(String command){
    command.trim();
    if (command.startsWith("cali -s0 ")) {
        calibrate0_start = command.substring(9).toFloat();
        Serial.println("calibrate0_start set to " + String(calibrate0_start));
    } else if (command.startsWith("cali -e0 ")) {
        calibrate0_end = command.substring(9).toFloat();
        Serial.println("calibrate0_end set to " + String(calibrate0_end));
    } else if (command.startsWith("cali -w0 ")) {
        calibrate0_weight = command.substring(9).toFloat();
        Serial.println("calibrate0_weight set to " + String(calibrate0_weight));
    } else if (command.startsWith("cali -b0 ")) {
        calibrate0_bias = command.substring(9).toFloat();
        Serial.println("calibrate0_bias set to " + String(calibrate0_bias));
    } else if (command.startsWith("cali -s1 ")) {
        calibrate1_start = command.substring(9).toFloat();
        Serial.println("calibrate1_start set to " + String(calibrate1_start));
    } else if (command.startsWith("cali -e1 ")) {
        calibrate1_end = command.substring(9).toFloat();
        Serial.println("calibrate1_end set to " + String(calibrate1_end));
    } else if (command.startsWith("cali -w1 ")) {
        calibrate1_weight = command.substring(9).toFloat();
        Serial.println("calibrate1_weight set to " + String(calibrate1_weight));
    } else if (command.startsWith("cali -b1 ")) {
        calibrate1_bias = command.substring(9).toFloat();
        Serial.println("calibrate1_bias set to " + String(calibrate1_bias));
    } else if (command.startsWith("cali -show")){
        Serial.printf("s0 %.2f\ne0 %.2f\nw0 %.2f\nb0 %.2f\ns1 %.2f\ne1 %.2f\nw1 %.2f\nb1 %.2f\n", 
            calibrate0_start,calibrate0_end,calibrate0_weight,calibrate0_bias,
            calibrate1_start,calibrate1_end,calibrate1_weight,calibrate1_bias);
    } else if (command.startsWith("save")){
        updateCalFile();
        Serial.println("Calibration saved.");
    }
}

#endif