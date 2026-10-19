#include "esp_now_handling.h"

void init_esp_now()
{
    WiFi.mode(WIFI_STA);
#if defined(ESP32)

#if defined(CUSTOM_MAC_ADDR)
    esp_wifi_set_mac(WIFI_IF_STA, (uint8_t *)CUSTOM_MAC_ADDR);
#endif

    esp_now_init();

    esp_now_register_send_cb(nullptr);
    esp_now_register_recv_cb(nullptr);

#elif defined(ESP8266)

#if defined(CUSTOM_MAC_ADDR)
    wifi_set_macaddr(STATION_IF, (uint8_t *)CUSTOM_MAC_ADDR);
#endif

    esp_now_init();

    esp_now_register_send_cb(nullptr);
    esp_now_register_recv_cb(nullptr);
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);

#endif
}

// Register a peer MAC address for ESP-NOW communication
void add_esp_now_peer(uint8_t *peer_mac)
{
#if defined(ESP32)
    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memcpy(peer.peer_addr, peer_mac, 6);
    peer.channel = 0; // Use current channel
    peer.encrypt = 0; // Disnable encryption
    esp_now_add_peer(&peer);
#elif defined(ESP8266)
    esp_now_add_peer(peer_mac, ESP_NOW_ROLE_COMBO, 0, NULL, 0);
#endif
}

// Remove a peer MAC address from ESP-NOW
void remove_esp_now_peer(uint8_t *peer_mac)
{
#if defined(ESP32)
    esp_now_del_peer(peer_mac);
#elif defined(ESP8266)
    esp_now_del_peer(peer_mac);
#endif
}

void send_esp_now_event(const char *peer_mac, const esp_now_event_t &event)
{
    encrypted_packet_t encrypted_content = encrypt_event(event);
    esp_now_send((uint8_t *)peer_mac, (uint8_t *)&encrypted_content, encrypted_content.length);
    // esp_now_send((uint8_t *)peer_mac, (uint8_t *)&event, sizeof(event));
}

// --- ENCRYPT FUNCTION ---
encrypted_packet_t encrypt_event(const esp_now_event_t &event)
{
    encrypted_packet_t packet;
    memset(&packet, 0, sizeof(packet));

// 1. Generate 16-byte random IV
#if defined(ESP8266)
    for (int i = 0; i < 16; i++)
        packet.iv[i] = RANDOM_REG32;
#elif defined(ESP32)
    esp_fill_random(packet.iv, sizeof(packet.iv));
#endif

    // 2. Copy struct to raw buffer (144 bytes)
    uint8_t raw_buffer[144];
    memcpy(raw_buffer, &event, sizeof(esp_now_event_t));

// 3. Encrypt
#if defined(ESP8266)
    uint16_t encLen = aesLib.encrypt(raw_buffer, sizeof(raw_buffer), packet.payload, (byte *)AES_KEY, 128, packet.iv);
    packet.length = 16 + encLen; // 16 bytes IV + Ciphertext length

#elif defined(ESP32)
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, AES_KEY, 128);

    uint8_t iv_copy[16];
    memcpy(iv_copy, packet.iv, 16);

    // PKCS#7 Padding: 144 bytes is a multiple of 16, so add a 16-byte padding block of value 0x10
    uint8_t padded_buffer[160];
    memcpy(padded_buffer, raw_buffer, 144);
    for (int i = 144; i < 160; i++)
    {
        padded_buffer[i] = 16;
    }

    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, 160, iv_copy, padded_buffer, packet.payload);
    mbedtls_aes_free(&aes);

    packet.length = 16 + 160;
#endif

    return packet;
}

// --- DECRYPT FUNCTION ---
bool decrypt_event(const encrypted_packet_t &packet, esp_now_event_t &out_event)
{
    uint8_t decrypted_buffer[160];
    memset(decrypted_buffer, 0, sizeof(decrypted_buffer));
    uint16_t ciphertext_len = packet.length - 16;

#if defined(ESP8266)
    memcpy(decrypted_buffer, packet.payload, ciphertext_len);
    aesLib.decrypt(decrypted_buffer, ciphertext_len, decrypted_buffer, (byte *)AES_KEY, 128, (byte *)packet.iv);
    memcpy(&out_event, decrypted_buffer, sizeof(esp_now_event_t));
    return true;

#elif defined(ESP32)
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, AES_KEY, 128);

    uint8_t iv_copy[16];
    memcpy(iv_copy, packet.iv, 16);

    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, ciphertext_len, iv_copy, packet.payload, decrypted_buffer);
    mbedtls_aes_free(&aes);

    memcpy(&out_event, decrypted_buffer, sizeof(esp_now_event_t));
    return true;
#endif
}

// Convert esp_now_event_t to string (for serial communication)
// Format: mac_str:event_str:key1:value1:key2:value2:...
// Returns pointer to static string (caller must copy before calling again)
const char *msg_to_string(const uint8_t *mac, const esp_now_event_t *msg)
{
    static char s_msg_buffer[MSG_STRING_MAX_LEN];
    size_t pos = 0;

    // Add MAC address (xx-xx-xx-xx-xx-xx format)
    for (int i = 0; i < 6; i++)
    {
        if (i > 0)
        {
            s_msg_buffer[pos++] = '-';
        }
        snprintf(s_msg_buffer + pos, MSG_STRING_MAX_LEN - pos, "%02x", mac[i]);
        pos += 2;
    }

    // Add event name
    s_msg_buffer[pos++] = ':';
    strncpy(s_msg_buffer + pos, msg->event, MSG_STRING_MAX_LEN - pos - 1);
    pos += strlen(msg->event);

    // Add key:value pairs
    for (int i = 0; i < 8; i++)
    {
        if (strlen(msg->data[i].key) == 0)
            continue;

        s_msg_buffer[pos++] = ':';
        strncpy(s_msg_buffer + pos, msg->data[i].key, MSG_STRING_MAX_LEN - pos - 1);
        pos += strlen(msg->data[i].key);

        s_msg_buffer[pos++] = ':';
        strncpy(s_msg_buffer + pos, msg->data[i].value, MSG_STRING_MAX_LEN - pos - 1);
        pos += strlen(msg->data[i].value);
    }

    s_msg_buffer[pos] = '\0';
    return s_msg_buffer;
}

// Convert string to esp_now_event_t (for parsing serial commands)
// Format: mac_str:event_str:key1:value1:key2:value2:...
bool string_to_msg(const char *input, uint8_t *mac_out, esp_now_event_t *output)
{
    if (!input || !output)
        return false;

    memset(output, 0, sizeof(esp_now_event_t));

    String str = String(input);
    int first_colon = str.indexOf(':');

    if (first_colon != 17)
        return false; // MAC should be exactly 17 chars

    // Parse MAC address if provided
    if (mac_out)
    {
        String mac_part = str.substring(0, 17);
        int idx = 0;
        int start = 0;
        while (idx < 6)
        {
            int dash = mac_part.indexOf('-', start);
            String hex = (dash < 0) ? mac_part.substring(start) : mac_part.substring(start, dash);
            mac_out[idx++] = (uint8_t)strtol(hex.c_str(), nullptr, 16);
            if (dash < 0)
                break;
            start = dash + 1;
        }
    }

    // Get rest after MAC
    String rest = str.substring(18); // Skip MAC + colon
    first_colon = rest.indexOf(':');

    if (first_colon <= 0)
        return false;

    // Extract event name
    strncpy(output->event, rest.substring(0, first_colon).c_str(), sizeof(output->event) - 1);

    // Parse key:value pairs
    int msg_idx = 0;
    int pos = first_colon + 1;

    while (msg_idx < 8 && pos < rest.length())
    {
        // Find key
        int next_colon = rest.indexOf(':', pos);
        if (next_colon < 0)
            break;

        String key = rest.substring(pos, next_colon);
        pos = next_colon + 1;

        // Find value
        next_colon = rest.indexOf(':', pos);
        String value = (next_colon < 0) ? rest.substring(pos) : rest.substring(pos, next_colon);

        if (key.length() < sizeof(output->data[msg_idx].key) &&
            value.length() < sizeof(output->data[msg_idx].value))
        {
            strncpy(output->data[msg_idx].key, key.c_str(), sizeof(output->data[msg_idx].key) - 1);
            strncpy(output->data[msg_idx].value, value.c_str(), sizeof(output->data[msg_idx].value) - 1);
            msg_idx++;
        }

        if (next_colon < 0)
            break;
        pos = next_colon + 1;
    }

    return msg_idx >= 0;
}
