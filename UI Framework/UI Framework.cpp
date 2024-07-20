#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include "Window/WindowsEx.h"
// Necessary for clsid's to work when creating custom effects
#include <Mmsystem.h>

#include "Scenes/DefaultNonClientAreaScene.h"
#include "Scenes/DefaultTitleBarScene.h"
#include "Scenes/EntryScene.h"
#include "Helper/ResourceManager.h"
#include "Window/Window.h"
#include "App.h"

#pragma comment( lib,"Winmm.lib" )

int WINAPI main(HINSTANCE hInst, HINSTANCE, LPWSTR cmdLine, INT)
{
    // Read arguments
    std::vector<std::wstring> args;
    int argCount;
    LPWSTR* pArgs = CommandLineToArgvW(cmdLine, &argCount);
    for (int i = 0; i < argCount; i++)
    {
        args.push_back(std::wstring(pArgs[i]));
        std::wcout << args[i] << '\n';
    }
    LocalFree(pArgs);

    App app(hInst);

    std::optional<zwnd::WindowId> id2 = app.CreateTopWindow(
        zwnd::WindowProperties()
            .WindowClassName(L"wndClass")
            .InitialSize(720, 720)
            .MainWindow(),
        [](zwnd::Window* wnd)
        {
            wnd->resourceManager.SetImageResourceFilePath("Resources/Images/resources.resc");
            wnd->resourceManager.InitAllImages();
            wnd->LoadNonClientAreaScene<zcom::DefaultNonClientAreaScene>(nullptr);
            zcom::DefaultTitleBarSceneOptions opt;
            wnd->LoadTitleBarScene<zcom::DefaultTitleBarScene>(&opt);
            wnd->LoadStartingScene<zcom::EntryScene>(nullptr);
            //wnd->LoadStartingScene<zcom::TestScene>(nullptr);
        }
    );

    while (true)
    {
        if (app.WindowsClosed())
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}