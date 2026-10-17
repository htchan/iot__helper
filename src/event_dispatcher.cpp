#include "event_dispatcher.h"

EventDispatcher &EventDispatcher::getInstance()
{
    static EventDispatcher instance;
    return instance;
}

EventDispatcher::EventDispatcher() : _head(0), _tail(0) {}

bool EventDispatcher::publish(const Event &event)
{
    size_t nextHead = (_head + 1) % Capacity;
    if (nextHead == _tail)
    {
        return false; // Queue overflow
    }
    _queue[_head] = event;
    _head = nextHead;
    return true;
}

void EventDispatcher::subscribe(uint32_t typeHash, EventCallback callback)
{
    _callbacks[typeHash] = callback;
}

bool EventDispatcher::poll(Event &event)
{
    if (_head == _tail)
    {
        return false; // Queue empty
    }
    event = _queue[_tail];
    _tail = (_tail + 1) % Capacity;
    return true;
}

void EventDispatcher::dispatch()
{
    Event event;
    while (poll(event))
    {
        auto it = _callbacks.find(event.typeHash);
        if (it != _callbacks.end() && it->second)
        {
            it->second(event);
        }
    }
}