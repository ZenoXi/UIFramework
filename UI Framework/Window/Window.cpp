#include "App.h"
#include "Window.h"

zwnd::Window::Window(App* app, WindowProperties props, HINSTANCE hinst, std::function<void(zwnd::Window* window)> initFunction)
{
    _app = app;
    _props = props;
    _hinst = hinst;

    // Start message thread
    _messageThread = std::thread(&Window::_MessageThread, this);

    // Wait for window to be created
    while (!_windowCreated);

    // Init resources and scenes
    initFunction(this);


    _scenesInited = true;
}

void zwnd::Window::Fullscreen(bool fullscreen)
{
    if (fullscreen == _fullscreen)
        return;

    _fullscreen = fullscreen;
    if (_fullscreen)
    {
        mouseManager.SetTopMenuVisibility(false);
        _window->SetFullscreen(true);
        _sceneChanged = true;
    }
    else
    {
        mouseManager.SetTopMenuVisibility(true);
        _window->SetFullscreen(false);
        _sceneChanged = true;
    }

}

void zwnd::Window::_UninitScene(std::string name)
{
    int index = _GetSceneIndex(name);
    if (index == -1)
        return;

    Scene* scene = _activeScenes[index].get();
    scene->Uninit();
    _activeScenes.erase(_activeScenes.begin() + index);
    _sceneChanged = true;
}

std::vector<Scene*> zwnd::Window::Scenes()
{
    std::vector<Scene*> scenes(_activeScenes.size());
    for (int i = 0; i < _activeScenes.size(); i++)
        scenes[i] = _activeScenes[i].get();
    return scenes;
}

DefaultTitleBarScene* zwnd::Window::GetTitleBarScene()
{
    return _titleBarScene.get();
}

DefaultNonClientAreaScene* zwnd::Window::GetNonClientAreaScene()
{
    return _nonClientAreaScene.get();
}

Scene* zwnd::Window::_GetScene(std::string name)
{
    for (int i = 0; i < _activeScenes.size(); i++)
        if (_activeScenes[i]->GetName() == name)
            return _activeScenes[i].get();
    return nullptr;
}

int zwnd::Window::_GetSceneIndex(std::string name)
{
    for (int i = 0; i < _activeScenes.size(); i++)
        if (_activeScenes[i]->GetName() == name)
            return i;
    return -1;
}

void zwnd::Window::_HandleMessage(WindowMessage msg)
{
    if (msg.id == MouseMoveMessage::ID())
    {
        MouseMoveMessage message;
        message.Decode(msg);
        int x = message.x;
        int y = message.y;

        RECT margins = _nonClientAreaScene->GetClientAreaMargins();

        struct SceneInfo
        {
            int left;
            int right;
            int top;
            int bottom;
            Scene* scene;
        };

        // Group all scenes into one list, with top scenes at the start
        std::vector<SceneInfo> scenes;
        scenes.reserve(2 + _activeScenes.size());

        SceneInfo titleBarSceneInfo;
        titleBarSceneInfo.scene = _titleBarScene.get();
        titleBarSceneInfo.left = margins.left;
        titleBarSceneInfo.top = margins.top;
        titleBarSceneInfo.right = _window->GetWidth() - margins.right;
        titleBarSceneInfo.bottom = margins.top + _titleBarScene->TitleBarHeight();
        scenes.push_back(titleBarSceneInfo);

        // Scenes are stored in the array in the order: bottom-most -> top-most
        for (auto reverseit = _activeScenes.crbegin(); reverseit != _activeScenes.crend(); reverseit++)
        {
            SceneInfo sceneInfo;
            sceneInfo.scene = reverseit->get();
            sceneInfo.left = margins.left;
            sceneInfo.top = margins.top;
            sceneInfo.right = _window->GetWidth() - margins.right;
            sceneInfo.bottom = _window->GetHeight() - margins.bottom;
            scenes.push_back(sceneInfo);
        }

        SceneInfo nonClientAreaSceneInfo;
        nonClientAreaSceneInfo.scene = _nonClientAreaScene.get();
        nonClientAreaSceneInfo.left = 0;
        nonClientAreaSceneInfo.top = 0;
        nonClientAreaSceneInfo.right = _window->GetWidth();
        nonClientAreaSceneInfo.bottom = _window->GetHeight();
        scenes.push_back(nonClientAreaSceneInfo);

        // Iterate all scenes until the mouse move event is handled
        for (auto& sceneInfo : scenes)
        {
            if (x >= sceneInfo.left && x < sceneInfo.right &&
                y >= sceneInfo.top && y < sceneInfo.bottom)
            {
                int sceneSpaceX = x - sceneInfo.left;
                int sceneSpaceY = y - sceneInfo.top;
                zcom::Component* target = sceneInfo.scene->GetCanvas()->OnMouseMove(sceneSpaceX, sceneSpaceY);
                if (target != nullptr)
                    break;
            }
        }
    }
    else if (msg.id == MouseEnterMessage::ID())
    {
        // At the moment scene mouse move handler deals with mouse entry
    }
    else if (msg.id == MouseLeaveMessage::ID())
    {

    }

    switch (msg.id)
    {
    case WM_MOUSEMOVE:
    {
        int x = (short)LOWORD(msg.lParam);
        int y = (short)HIWORD(msg.lParam);

        if (!_mouseInside)
        {
            _mouseInside = true;
            for (auto h : _mouseHandlers)
                h->OnMouseEnter();
        }
        for (auto h : _mouseHandlers)
            h->OnMouseMove(x, y);

        //if (x >= 0 && x < GetWidth() && y >= 0 && y < GetHeight())
        //{
        //    if (!_mouseInWindow)
        //    {
        //        _mouseInWindow = true;
        //        for (auto h : _mouseHandlers) h->OnMouseEnter();
        //    }
        //    for (auto h : _mouseHandlers) h->OnMouseMove(x, y);
        //}
        //else
        //{
        //    if (msg.wParam & (MK_LBUTTON | MK_RBUTTON))
        //    {
        //        for (auto h : _mouseHandlers) h->OnMouseMove(x, y);
        //    }
        //    else
        //    {
        //        _mouseInWindow = false;
        //        for (auto h : _mouseHandlers)
        //        {
        //            h->OnLeftReleased(x, y);
        //            h->OnRightReleased(x, y);
        //            h->OnMouseLeave();
        //        }
        //    }
        //}
        break;
    }
    case WM_MOUSELEAVE:
    {
        _mouseInside = false;
        for (auto h : _mouseHandlers)
            h->OnMouseLeave();
        break;
    }
    case WM_LBUTTONDOWN:
    {
        int x = LOWORD(msg.lParam);
        int y = HIWORD(msg.lParam);
        for (auto h : _mouseHandlers)
            h->OnLeftPressed(x, y);
        break;
    }
    case WM_RBUTTONDOWN:
    {
        int x = LOWORD(msg.lParam);
        int y = HIWORD(msg.lParam);
        for (auto h : _mouseHandlers)
            h->OnRightPressed(x, y);
        break;
    }
    case WM_LBUTTONUP:
    {
        int x = LOWORD(msg.lParam);
        int y = HIWORD(msg.lParam);
        for (auto h : _mouseHandlers)
            h->OnLeftReleased(x, y);
        break;
    }
    case WM_RBUTTONUP:
    {
        int x = LOWORD(msg.lParam);
        int y = HIWORD(msg.lParam);
        for (auto h : _mouseHandlers)
            h->OnRightReleased(x, y);
        break;
    }
    case WM_MOUSEWHEEL:
    {
        int x = LOWORD(msg.lParam);
        int y = HIWORD(msg.lParam);
        if (GET_WHEEL_DELTA_WPARAM(msg.wParam) > 0)
        {
            for (auto h : _mouseHandlers)
                h->OnWheelUp(x, y);
        }
        else if (GET_WHEEL_DELTA_WPARAM(msg.wParam) < 0)
        {
            for (auto h : _mouseHandlers)
                h->OnWheelDown(x, y);
        }
        break;
    }
    case WM_KEYDOWN:
    {
        for (auto& h : _keyboardHandlers)
            h->OnKeyDown(msg.wParam);
        break;
    }
    case WM_KEYUP:
    {
        for (auto& h : _keyboardHandlers)
            h->OnKeyUp(msg.wParam);
        break;
    }
    case WM_CHAR:
    {
        for (auto& h : _keyboardHandlers)
            h->OnChar(msg.wParam);
        break;
    }
    case WM_MOVE:
    {
        _moveResult.handled = false;
        _moveResult.msg = WM_MOVE;
        _moveResult.lParam = msg.lParam;
        _moveResult.wParam = msg.wParam;
        break;
    }
    case WM_SIZE:
    {
        _sizeResult.handled = false;
        _sizeResult.msg = WM_SIZE;
        _sizeResult.lParam = msg.lParam;
        _sizeResult.wParam = msg.wParam;
        break;
    }
    }
}

void zwnd::Window::_MessageThread()
{
    // Create window
    _window = std::make_unique<WindowBackend>(_hinst, _props);

    // Pass the device context to the resource manager
    resourceManager.CoInit();
    resourceManager.SetDeviceContext(_window->gfx.GetGraphics().target);

    // Wait for scene and resource init
    _windowCreated = true;
    while (!_scenesInited);

    // Add handlers
    _window->AddMouseHandler(&mouseManager);
    _window->AddKeyboardHandler(&keyboardManager);

    // Start UI thread
    _uiThread = std::thread(&Window::_UIThread, this);

    // Main window loop
    Clock msgTimer = Clock(0);
    while (true)
    {
        // Check for window close
        if (_closed)
            break;

        // Messages
        bool msgProcessed = _window->ProcessMessages();

        _window->HandleFullscreenChange();
        _window->HandleCursorVisibility();

        // Limit cpu usage
        if (!msgProcessed)
        {
            // If no messages are received for 50ms or more, sleep to limit cpu usage.
            // This way we allow for full* mouse poll rate utilization when necessary.
            //
            // * the very first mouse move after a break will have a very small delay
            // which may be noticeable in certain situations (FPS games)
            msgTimer.Update();
            if (msgTimer.Now().GetTime(MILLISECONDS) >= 50)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        else
        {
            msgTimer.Reset();
        }
        continue;
    }
}

void zwnd::Window::_UIThread()
{
    int framecounter = 0;
    Clock frameTimer = Clock(0);

    // Create frame number debug text rendering resources
    IDWriteFactory* dwriteFactory = nullptr;
    IDWriteTextFormat* dwriteTextFormat = nullptr;
    IDWriteTextLayout* dwriteTextLayout = nullptr;
    //DWriteCreateFactory(
    //    DWRITE_FACTORY_TYPE_SHARED,
    //    __uuidof(IDWriteFactory),
    //    reinterpret_cast<IUnknown**>(&dwriteFactory)
    //);
    //dwriteFactory->CreateTextFormat(
    //    L"Calibri",
    //    NULL,
    //    DWRITE_FONT_WEIGHT_BOLD,
    //    DWRITE_FONT_STYLE_NORMAL,
    //    DWRITE_FONT_STRETCH_NORMAL,
    //    20.0f,
    //    L"en-us",
    //    &dwriteTextFormat
    //);

    while (true)
    {
        _window->ProcessQueueMessages([&](WindowMessage msg) { Window::_HandleMessage(msg); });
        WindowMessage wmMove = _window->GetMoveResult();
        WindowMessage wmSize = _window->GetSizeResult();
        WindowMessage wmExit = _window->GetExitResult();

        // Check for exit message
        //if (!wmExit.handled)
        //{
        //    // Uninit all scenes
        //    for (auto& scene : _activeScenes)
        //        scene->Uninit();

        //    _closed = true;
        //    break;
        //}

        ztime::clock[CLOCK_GAME].Update();
        ztime::clock[CLOCK_MAIN].Update();

        _window->LockSize();

        // Check for resize
        zwnd::MessageWindowSize windowSize = _window->GetMessageWindowSize();
        if (windowSize.changed)
        {
            // Resize render target
            //if (_window->gfx.Initialized())
            //    _window->gfx.ResizeBuffers(windowSize.width, windowSize.height, false);

            RECT clientAreaMargins = _nonClientAreaScene->GetClientAreaMargins();

            // Resize regular scenes
            for (auto& scene : _activeScenes)
                scene->Resize(
                    windowSize.width - clientAreaMargins.left - clientAreaMargins.right,
                    windowSize.height - clientAreaMargins.top - clientAreaMargins.bottom - _titleBarScene->TitleBarHeight()
                );
            // Resize title bar scene
            ((Scene*)_titleBarScene.get())->Resize(
                windowSize.width - clientAreaMargins.left - clientAreaMargins.right,
                _titleBarScene->TitleBarHeight()
            );
            // Resize non-client area scene
            ((Scene*)_nonClientAreaScene.get())->Resize(windowSize.width, windowSize.height);
        }

        // Render frame
        //_window->gfx.Lock();
        _window->gfx.BeginFrame();

        // Pass title bar item and non-client area scene properties to underlying window thread
        _PassParamsToHitTest();

        bool redraw = false;
        if (_sceneChanged)
        {
            _sceneChanged = false;
            redraw = true;

            // TODO: Resend mouse move message (prefferably have a function in 'WindowBackend')
        }

        // Use a copy because the scene update functions can change the '_activeScenes' ordering
        auto activeScenes = Scenes();

        { // Updating scenes
            // Update content scenes
            for (auto& scene : activeScenes)
                scene->Update();
            // Update the title bar scene
            ((Scene*)_titleBarScene.get())->Update();
            // Update the non-client area scene
            ((Scene*)_nonClientAreaScene.get())->Update();
        }

        { // Redraw checking
            // Check for content scene redraw
            for (auto& scene : activeScenes)
            {
                if (scene->Redraw())
                {
                    redraw = true;
                    break;
                }
            }
            // Check for title scene redraw
            if (((Scene*)_titleBarScene.get())->Redraw())
                redraw = true;
            // Check for non-client area scene redraw
            if (((Scene*)_nonClientAreaScene.get())->Redraw())
                redraw = true;
        }

        //_window->SetSize(_window->GetWidth() + 1, _window->GetHeight() + 1);

        //std::cout << "Updated " << ++framecounter << '\n';
        //redraw = true;
        if (redraw)
        {
            //std::cout << "Redrawn (" << framecounter++ << ")\n";
            Graphics g = _window->gfx.GetGraphics();
            g.target->BeginDraw();
            g.target->Clear();
            //g.target->Clear(D2D1::ColorF(0.0f, 0.5f, 0.8f));

            RECT clientAreaMargins = _nonClientAreaScene->GetClientAreaMargins();
            D2D1_SIZE_F clientSize = {
                (float)_window->GetWidth() - (clientAreaMargins.left + clientAreaMargins.right),
                (float)_window->GetHeight() - (clientAreaMargins.top + clientAreaMargins.bottom)
            };

            // Create bitmap for client area rendering
            ID2D1Bitmap1* clientAreaBitmap = nullptr;
            g.target->CreateBitmap(
                D2D1::SizeU(clientSize.width, clientSize.height),
                nullptr,
                0,
                D2D1::BitmapProperties1(
                    D2D1_BITMAP_OPTIONS_TARGET,
                    { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }
                ),
                &clientAreaBitmap
            );

            // Set content bitmap as target
            ID2D1Image* stash = nullptr;
            g.target->GetTarget(&stash);
            g.target->SetTarget(clientAreaBitmap);
            g.target->Clear();

            // Draw the title bar scene
            if (((Scene*)_titleBarScene.get())->Redraw())
                ((Scene*)_titleBarScene.get())->Draw(g);
            g.target->DrawBitmap(((Scene*)_titleBarScene.get())->Image());

            // Draw other scenes
            for (auto& scene : activeScenes)
            {
                if (scene->Redraw())
                    scene->Draw(g);
                g.target->DrawBitmap(
                    scene->Image(),
                    D2D1::RectF(
                        0.0f,
                        _titleBarScene->TitleBarHeight(),
                        g.target->GetSize().width,
                        g.target->GetSize().height
                    )
                );
            }

            // Unstash original target
            g.target->SetTarget(stash);
            stash->Release();

            // Draw non-client area scene
            _nonClientAreaScene->SetClientAreaBitmap(clientAreaBitmap);
            ((Scene*)_nonClientAreaScene.get())->Draw(g);
            //g.target->Clear(D2D1::ColorF(0.0f, 0.5f, 0.9f));

            g.target->DrawBitmap(((Scene*)_nonClientAreaScene.get())->Image());
            clientAreaBitmap->Release();

            if (GetKeyState(VK_SPACE) & 0x8000)
                g.target->Clear(D2D1::ColorF(0.2f, 0.2f, 0.2f, 0.8f));

            // Display frame number while 'Ctrl + S + F' is held
            if ((GetKeyState(VK_CONTROL) & 0x8000) &&
                (GetKeyState('S') & 0x8000) &&
                (GetKeyState('F') & 0x8000))
            {
                std::wstringstream ss;
                ss << framecounter++;
                dwriteFactory->CreateTextLayout(
                    ss.str().c_str(),
                    ss.str().length(),
                    dwriteTextFormat,
                    100,
                    30,
                    &dwriteTextLayout
                );

                ID2D1SolidColorBrush* brush;
                g.target->CreateSolidColorBrush(D2D1::ColorF(0.4f, 0.8f, 0.0f, 0.9f), &brush);
                g.target->DrawTextLayout(D2D1::Point2F(5.0f, 5.0f), dwriteTextLayout, brush);

                brush->Release();
                dwriteTextLayout->Release();
            }

            // Update layered window
            _window->UpdateLayeredWindow();

            HRESULT hr = g.target->EndDraw();
            int thing = 5;
        }

        _window->UnlockSize();

        // Uninit scenes
        while (!_scenesToUninitialize.empty())
        {
            _UninitScene(_scenesToUninitialize.front());
            _scenesToUninitialize.pop_front();
        }

        //Clock c(0);
        _window->gfx.EndFrame(redraw);

        //_window->SetWidth(_window->GetWidth() + 1);
        //c.Update();
        //std::cout << c.Now().GetTime() << std::endl;
        //_window->gfx.Unlock();

        // Prevent deadlock from extremely short unlock-lock cycle
        // (it doesn't make sense to me either but for some reason
        // the mutexes aren't locked in order of 'Lock()' calls)
        //Clock sleepTimer = Clock(0);
        //do sleepTimer.Update();
        //while (sleepTimer.Now().GetTime(MICROSECONDS) < 10);
    }

    // Release text rendering resources
    //dwriteFactory->Release();
    //dwriteTextFormat->Release();
}

void zwnd::Window::_PassParamsToHitTest()
{

    _window->SetResizingBorderMargins(_nonClientAreaScene->GetResizingBorderWidths());
    RECT clientAreaMargins = _nonClientAreaScene->GetClientAreaMargins();
    _window->SetClientAreaMargins(clientAreaMargins);
    _window->SetTitleBarHeight(_titleBarScene->TitleBarHeight());

    RECT windowMenuButtonRect = _titleBarScene->WindowMenuButtonRect();
    // Transform rect to window coordinates
    windowMenuButtonRect.left += clientAreaMargins.left;
    windowMenuButtonRect.right += clientAreaMargins.left;
    windowMenuButtonRect.top += clientAreaMargins.top;
    windowMenuButtonRect.bottom += clientAreaMargins.top;
    _window->SetWinMenuButtonRect(windowMenuButtonRect);

    std::vector<RECT> excludedRects = _titleBarScene->ExcludedCaptionRects();
    // Transform rects to window coordinates
    for (auto& rect : excludedRects)
    {
        rect.left += clientAreaMargins.left;
        rect.right += clientAreaMargins.left;
        rect.top += clientAreaMargins.top;
        rect.bottom += clientAreaMargins.top;
    }
    _window->SetExcludedCaptionRects(excludedRects);
}