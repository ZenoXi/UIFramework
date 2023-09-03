#pragma once

#include "WindowsEx.h"
#include <string>

namespace zwnd
{
    // Struct containing window creation properties
    struct WindowProperties
    {
        // A unique window class name. For details refer to the Microsoft documentation
        std::wstring windowClassName;
        // An icon that will be used in the taskbar and executable file
        HICON windowIcon;
        // Initial width of the window
        int initialWidth;
        // Initial height of the window
        int initialHeight;
    };

}