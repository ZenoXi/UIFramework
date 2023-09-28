#pragma once

#include "WindowsEx.h"
#include <string>

namespace zwnd
{
    // Struct containing window creation properties
    struct WindowProperties
    {
        //
        // General properties

        // A unique window class name. For details refer to the Microsoft documentation
        std::wstring windowClassName;
        // An icon that will be used in the taskbar and executable file
        HICON windowIcon = NULL;
        // Initial width of the window
        int initialWidth = 1280;
        // Initial height of the window
        int initialHeight = 720;

        //
        // Top level window properties

        // Indicates whether the window is a "main" window
        bool mainWindow = false;

        //
        // Child window properties

        // Indicates whether the window blocks interaction with the parent window
        bool blockParent = false;

        
        WindowProperties& WindowClassName(std::wstring name) { windowClassName = name; return *this; }
        WindowProperties& WindowIcon(HICON icon) { windowIcon = icon; return *this; }
        WindowProperties& InitialWidth(int width) { initialWidth = width; return *this; }
        WindowProperties& InitialHeight(int height) { initialHeight = height; return *this; }
        WindowProperties& InitialSize(int width, int height) { initialWidth = width; initialHeight = height; return *this; }
        WindowProperties& MainWindow() { mainWindow = true; return *this; }
        WindowProperties& BlockParent() { blockParent = true; return *this; }
    };

}