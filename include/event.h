#pragma once
#include <cstdint>

// Compile-time string hashing (works identically on ESP32 & ESP8266)
constexpr uint32_t hashString(const char *str, uint32_t value = 2166136261U)
{
    return (*str == 0) ? value : hashString(str + 1, (value ^ static_cast<uint32_t>(*str)) * 16777619U);
}

struct Event
{
    uint32_t typeHash;
    void *payload;

    // Convenience constructor with default parameters
    constexpr Event(uint32_t hash = 0, void *ptr = nullptr)
        : typeHash(hash), payload(ptr) {}
};