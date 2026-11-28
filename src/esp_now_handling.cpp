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
    esp_now_send((uint8_t *)peer_mac, (uint8_t *)&encrypted_content, sizeof(encrypted_content));
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

// 2. Generate random boot_id (4 bytes)
#if defined(ESP8266)
    for (int i = 0; i < 4; i++)
        packet.boot_id[i] = RANDOM_REG32;
#elif defined(ESP32)
    esp_fill_random(packet.boot_id, sizeof(packet.boot_id));
#endif

// 3. Increment sequence number for this transmission
static uint32_t s_sequence_num = 0;
++s_sequence_num;
packet.sequence_num[0] = (uint8_t)(s_sequence_num & 0xFF);
packet.sequence_num[1] = (uint8_t)((s_sequence_num >> 8) & 0xFF);
packet.sequence_num[2] = (uint8_t)((s_sequence_num >> 16) & 0xFF);
packet.sequence_num[3] = (uint8_t)((s_sequence_num >> 24) & 0xFF);

// 4. Copy struct to raw buffer (144 bytes)
uint8_t raw_buffer[144];
memcpy(raw_buffer, &event, sizeof(esp_now_event_t));

// 5. Encrypt
#if defined(ESP8266)
    uint16_t encLen = aesLib.encrypt(raw_buffer, sizeof(raw_buffer), packet.ciphertext, (byte *)AES_KEY, 128, packet.iv);
    packet.actual_length = (uint8_t)(sizeof(esp_now_event_t)); // 144

#elif defined(ESP32)
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, AES_KEY, 128);

    uint8_t iv_copy[16];
    memcpy(iv_copy, packet.iv, 16);

    // Plaintext is 144 bytes (already a multiple of 16), encrypt directly
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, sizeof(raw_buffer), iv_copy, raw_buffer, packet.ciphertext);
    mbedtls_aes_free(&aes);

    packet.actual_length = (uint8_t)(sizeof(esp_now_event_t)); // 144
#endif

    return packet;
}

// --- DECRYPT FUNCTION ---
bool decrypt_event(const encrypted_packet_t &packet, esp_now_event_t &out_event)
{
    uint8_t decrypted_buffer[144];
    uint8_t iv_copy[16];
    uint8_t actual_len = packet.actual_length;

    // boot_id and sequence_num are informational headers; only the IV and ciphertext are used for decryption
    // boot_id: packet.boot_id, sequence_num: packet.sequence_num

    memcpy(iv_copy, packet.iv, 16);

#if defined(ESP8266)
    memcpy(decrypted_buffer, packet.ciphertext, actual_len);
    aesLib.decrypt(decrypted_buffer, actual_len, decrypted_buffer, (byte *)AES_KEY, 128, (byte *)packet.iv);
    memcpy(&out_event, decrypted_buffer, sizeof(esp_now_event_t));
    return true;

#elif defined(ESP32)
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, AES_KEY, 128);

    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, actual_len, iv_copy, packet.ciphertext, decrypted_buffer);
    mbedtls_aes_free(&aes);

    memcpy(&out_event, decrypted_buffer, sizeof(esp_now_event_t));
    return true;
#endif
}

// Convert esp_now_event_t to string (for serial communication)
// Format: [<mac>]"<key>:<value>","<key>:<value>",...
// A JSON string array payload without the surrounding brackets, prefixed by [<mac>],
// e.g. [AA_BB_CC_DD_EE_00]"temp/ste:25.5","humi/ste:60"
// Returns pointer to static string (caller must copy before calling again)
const char *msg_to_string(const uint8_t *mac, const esp_now_event_t *msg)
{
    static char s_msg_buffer[MSG_STRING_MAX_LEN];
    size_t pos = 0;

    // Add '['
    s_msg_buffer[pos++] = '[';

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

    // Add ']' and the opening quote of the JSON string array element
    s_msg_buffer[pos++] = ']';
    s_msg_buffer[pos++] = '"';

    bool first = true;
    // Add key:value pairs
    for (int i = 0; i < 8; i++)
    {
        if (strlen(msg->data[i].key) == 0)
            continue;

        if (!first)
        {
            s_msg_buffer[pos++] = ',';
        }
        first = false;

        strncpy(s_msg_buffer + pos, msg->data[i].key, MSG_STRING_MAX_LEN - pos - 1);
        pos += strlen(msg->data[i].key);

        s_msg_buffer[pos++] = ':';
        strncpy(s_msg_buffer + pos, msg->data[i].value, MSG_STRING_MAX_LEN - pos - 1);
        pos += strlen(msg->data[i].value);
    }

    // Add closing quote of the JSON string array element
    s_msg_buffer[pos++] = '"';
    s_msg_buffer[pos] = '\0';
    return s_msg_buffer;
}

// Convert string to esp_now_event_t (for parsing serial commands)
// Format: [<mac>]"<key>:<value>","<key>:<value>",...
// A JSON string array payload without the surrounding brackets, prefixed by [<mac>],
// e.g. [AA_BB_CC_DD_EE_00]"temp/ste:25.5","humi/ste:60"
bool string_to_msg(const char *input, uint8_t *mac_out, esp_now_event_t *output)
{
    if (!input || !output)
        return false;

    memset(output, 0, sizeof(esp_now_event_t));

    String str = String(input);
    if (str.length() == 0)
        return false;

    int close = str.indexOf(']');
    if (close < 0)
        return false;

    String first = str.substring(0, 1);
    if (first != "[")
        return false;

    // Parse MAC address (xx-xx-xx-xx-xx-xx) if provided
    if (mac_out)
    {
        String mac_part = str.substring(1, close);
        int idx = 0;
        int start = 0;
        while (idx < 6)
        {
            int dash = mac_part.indexOf('-', start);
            String hex = (dash < 0) ? mac_part.substring(start) : mac_part.substring(start, dash);
            if (hex.length() == 0)
                break;
            mac_out[idx] = (uint8_t)strtol(hex.c_str(), nullptr, 16);
            idx++;
            if (dash < 0)
                break;
            start = dash + 1;
        }
    }

    // Payload must start with the opening quote of a JSON string array element
    String payload = str.substring(close + 1);
    if (payload.length() < 2)
        return false;
    String firstChar = payload.substring(0, 1);
    if (firstChar != "\"")
        return false;

    // Strip the surrounding quotes and split the comma separated key:value pairs
    String body = payload.substring(1, payload.length() - 1);

    int msg_idx = 0;
    int pos = 0;
    int len = body.length();
    while (pos < len)
    {
        int comma = body.indexOf(',', pos);
        String pair = (comma < 0) ? body.substring(pos) : body.substring(pos, comma);
        int colon = pair.indexOf(':');
        if (colon > 0 && msg_idx < 8)
        {
            String key = pair.substring(0, colon);
            String value = pair.substring(colon + 1);
            if (key.length() < sizeof(output->data[msg_idx].key) &&
                value.length() < sizeof(output->data[msg_idx].value))
            {
                strncpy(output->data[msg_idx].key, key.c_str(), sizeof(output->data[msg_idx].key) - 1);
                strncpy(output->data[msg_idx].value, value.c_str(), sizeof(output->data[msg_idx].value) - 1);
                msg_idx++;
            }
        }
        if (comma < 0)
            break;
        pos = comma + 1;
    }

    // Serial command lines coming from the host are command (pair) lines
    strncpy(output->event, ESP_NOW_EVENT_CMD, sizeof(output->event) - 1);

    return msg_idx > 0;
}
