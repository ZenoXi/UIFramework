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
#include "Helper/Time.h"
#include "Window/DisplayWindow.h"
#include "Window/Window.h"
#include "App.h"

#pragma comment( lib,"Winmm.lib" )

int WINAPI main(HINSTANCE hInst, HINSTANCE, LPWSTR cmdLine, INT)
{
    // Set working directory
    std::wstring dir;
    dir.resize(MAX_PATH);
    GetModuleFileName(NULL, dir.data(), MAX_PATH);
    auto pos = dir.find_last_of(L"\\/");
    std::wstring runDir = dir.substr(0, pos);
    std::wcout << "Executable path: " << dir << '\n';

    if (!SetCurrentDirectory(runDir.data()))
    {
        std::cout << "Directory set failed\n";
        return -1;
    }

    TCHAR path[MAX_PATH] = { 0 };
    DWORD a = GetCurrentDirectory(MAX_PATH, path);
    std::wcout << "New working directory: " << path << '\n';

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

    // Create window
    //DisplayWindow window(hInst, cmdLine, L"class");

    // Load resources
    //ResourceManagerOld::Init("Resources/Images/resources.resc", window.gfx.GetGraphics().target);

    // Init app
    //App::Init(window);

    // Init appropriate scenes
    //App::Instance()->InitScene(EntryScene::StaticName(), nullptr);

    zwnd::Window wnd1 = zwnd::Window(App::Instance(), zwnd::WindowProperties{ L"wndClass1", 0, 1280, 720 }, hInst, [](zwnd::Window* wnd)
    {
        wnd->resourceManager.SetImageResourceFilePath("Resources/Images/resources.resc");
        wnd->resourceManager.InitAllImages();
        wnd->LoadNonClientAreaScene<DefaultNonClientAreaScene>(nullptr);
        wnd->LoadTitleBarScene<DefaultTitleBarScene>(nullptr);
        wnd->LoadStartingScene<EntryScene>(nullptr);
    });

    //zwnd::Window wnd2 = zwnd::Window(App::Instance(), zwnd::WindowProperties{ L"Window 2", L"wndClass2", 0 }, hInst, [](zwnd::Window* wnd)
    //{
    //    wnd->resourceManager.SetImageResourceFilePath("Resources/Images/resources.resc");
    //    wnd->resourceManager.InitAllImages();
    //    wnd->LoadNonClientAreaScene<DefaultNonClientAreaScene>(nullptr);
    //    wnd->LoadTitleBarScene<DefaultTitleBarScene>(nullptr);
    //    wnd->LoadStartingScene<EntryScene>(nullptr);
    //});

    //zwnd::Window wnd3 = zwnd::Window(App::Instance(), zwnd::WindowProperties{ L"Window 3", L"wndClass3", 0 }, hInst, [](zwnd::Window* wnd)
    //{
    //    wnd->resourceManager.SetImageResourceFilePath("Resources/Images/resources.resc");
    //    wnd->resourceManager.InitAllImages();
    //    wnd->LoadNonClientAreaScene<DefaultNonClientAreaScene>(nullptr);
    //    wnd->LoadTitleBarScene<DefaultTitleBarScene>(nullptr);
    //    wnd->LoadStartingScene<EntryScene>(nullptr);
    //});
    
    Clock msgTimer = Clock(0);

    // Start app thread
    //App::Start();

    // Main window loop
    while (true)
    {
        if (wnd1.Closed())
            break;
        //// Check for app exit
        //if (App::Exited())
        //    break;

        //// Messages
        //bool msgProcessed = window.ProcessMessages();

        //window.HandleFullscreenChange();
        //window.HandleCursorVisibility();

        //// Limit cpu usage
        //if (!msgProcessed)
        //{
        //    // If no messages are received for 50ms or more, sleep to limit cpu usage.
        //    // This way we allow for full* mouse poll rate utilization when necessary.
        //    //
        //    // * the very first mouse move after a break will have a very small delay
        //    // which may be noticeable in certain situations (FPS games)
        //    msgTimer.Update();
        //    if (msgTimer.Now().GetTime(MILLISECONDS) >= 50)
        //        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        //}
        //else
        //{
        //    msgTimer.Reset();
        //}
        //continue;


        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return 0;
}