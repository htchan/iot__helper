#include "esp_now_handling.h"

void init_esp_now()
{
    WiFi.mode(WIFI_STA);
#if defined(ESP32)

#if defined(CUSTOM_MAC_ADDR)
    esp_wifi_set_mac(WIFI_IF_STA, (uint8_t *)CUSTOM_MAC_ADDR);
#endif

    esp_now_init();

#if defined(ESP_NOW_PMK)
    esp_now_set_pmk((uint8_t *)ESP_NOW_PMK);
#endif

#elif defined(ESP8266)

#if defined(CUSTOM_MAC_ADDR)
    wifi_set_macaddr(STATION_IF, (uint8_t *)CUSTOM_MAC_ADDR);
#endif

    esp_now_init();

#if defined(ESP_NOW_PMK)
    esp_now_set_kok((uint8_t *)ESP_NOW_PMK, 16);
#endif

#endif
}

void send_esp_now_event(const char *peer_mac, const esp_now_event_t &event)
{
    esp_now_send((uint8_t *)peer_mac, (uint8_t *)&event, sizeof(event));
}