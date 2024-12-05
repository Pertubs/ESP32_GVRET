#pragma once

#include <Arduino.h>
#include "esp32_can.h"
#include "sys_io.h"
void setup();
void loop();

void handleCAN0CB(CAN_FRAME *frame);
void SendFrame(const CAN_FRAME &frame, int MessageFormat);
void Init_CAN();
void Read_CAN();
void Init_WIFI();
void Init_Serial();
void Handshake();
void Wifi_Handshake();
void WiFiEvent(WiFiEvent_t event);
void Update_Watchdog();
