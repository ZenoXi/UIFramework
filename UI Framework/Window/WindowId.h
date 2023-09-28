#pragma once

#include <cstdint>
#include <atomic>

namespace zwnd
{
    struct WindowId
    {
        static WindowId Generate()
        {
            static std::atomic<uint64_t> _ID_COUNTER{ 0 };
            return WindowId(_ID_COUNTER.fetch_add(1));
        }

        bool operator==(const WindowId& other)
        {
            return _id == other._id;
        }
        bool operator!=(const WindowId& other)
        {
            return _id != other._id;
        }

        WindowId(const WindowId& other) : _id(other._id) {}
        WindowId& operator=(const WindowId& other) { _id = other._id; return *this; }
        WindowId(WindowId&& other) noexcept : _id(other._id) {}
        WindowId& operator=(WindowId&& other) noexcept { _id = other._id; return *this; }

    private:
        uint64_t _id;

        WindowId(uint64_t id) : _id(id) {}
    };
}