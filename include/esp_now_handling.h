#ifndef ESP_NOW_HANDLING_H
#define ESP_NOW_HANDLING_H
#include <Arduino.h>

#if defined(ESP32)
#include <esp_now.h>
#include <WiFi.h>
#elif defined(ESP8266)
#include <espnow.h>
#include <ESP8266WiFi.h>
#endif

struct esp_now_data_t
{
    char key[10];
    char value[6];
};

struct esp_now_event_t
{
    char event[16];
    esp_now_data_t data[8];
};

void init_esp_now();

void send_esp_now_event(const char *peer_mac, const esp_now_event_t &event);

#endif