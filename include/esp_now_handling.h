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

// Event types
#define ESP_NOW_EVENT_AWAKE "awake"
#define ESP_NOW_EVENT_CMD "cmd"
#define ESP_NOW_EVENT_SLEEP "sleep"

// MAC address format in strings: xx-xx-xx-xx-xx-xx (6 bytes, lowercase hex, dash-separated)
#define MAC_STR_LEN 17 // 6*2 + 5 dashes + null

// Max message string length: MAC(17) + ':' + event(16) + 8*( ':' + key(10) + ':' + value(6) ) + null
// = 17 + 1 + 16 + 8*23 + 1 = 17 + 1 + 16 + 184 + 1 = 219
#define MSG_STRING_MAX_LEN 219

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

// Register a peer MAC address for ESP-NOW communication
void add_esp_now_peer(uint8_t *peer_mac);

// Remove a peer MAC address from ESP-NOW
void remove_esp_now_peer(uint8_t *peer_mac);

// Convert esp_now_event_t to string (for serial communication)
// Format: mac_str:event_str:key1:value1:key2:value2:...
// Returns pointer to static string (caller must copy before calling again)
const char *msg_to_string(const uint8_t *mac, const esp_now_event_t *msg);

// Convert string to esp_now_event_t (for parsing serial commands)
// Format: mac_str:event_str:key1:value1:key2:value2:...
// Returns true if successful, false otherwise
bool string_to_msg(const char *input, uint8_t *mac_out, esp_now_event_t *output);

#endif
