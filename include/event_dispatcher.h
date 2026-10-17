#pragma once
#include "event.h"
#include <functional>
#include <unordered_map>

class EventDispatcher
{
public:
    using EventCallback = std::function<void(const Event &)>;
    static constexpr size_t Capacity = 32;

    static EventDispatcher &getInstance();

    bool publish(const Event &event);
    void subscribe(uint32_t typeHash, EventCallback callback);
    void dispatch();

private:
    EventDispatcher();
    ~EventDispatcher() = default;
    EventDispatcher(const EventDispatcher &) = delete;
    EventDispatcher &operator=(const EventDispatcher &) = delete;

    bool poll(Event &event);

    Event _queue[Capacity];
    size_t _head;
    size_t _tail;
    std::unordered_map<uint32_t, EventCallback> _callbacks;
};