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

zwnd::Window::~Window()
{
    _closed = true;
    _messageThread.join();
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

    zcom::Scene* scene = _activeScenes[index].get();
    scene->Uninit();
    _activeScenes.erase(_activeScenes.begin() + index);
    _sceneChanged = true;
}

std::vector<zcom::Scene*> zwnd::Window::Scenes()
{
    std::vector<zcom::Scene*> scenes(_activeScenes.size());
    for (int i = 0; i < _activeScenes.size(); i++)
        scenes[i] = _activeScenes[i].get();
    return scenes;
}

zcom::DefaultTitleBarScene* zwnd::Window::GetTitleBarScene()
{
    return _titleBarScene.get();
}

zcom::DefaultNonClientAreaScene* zwnd::Window::GetNonClientAreaScene()
{
    return _nonClientAreaScene.get();
}

zcom::Scene* zwnd::Window::_GetScene(std::string name)
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
    if (msg.id == WindowSizeMessage::ID())
    {
        WindowSizeMessage message;
        message.Decode(msg);
        _windowSizeMessage = message;
    }
    else if (msg.id == WindowMoveMessage::ID())
    {

    }
    else if (msg.id == WindowCloseMessage::ID())
    {
        _closed = true;
    }
    else if (msg.id == MouseMoveMessage::ID())
    {
        MouseMoveMessage message;
        message.Decode(msg);
        int x = message.x;
        int y = message.y;

        _BuildMasterPanel()->OnMouseMove(x, y);
    }
    else if (msg.id == MouseEnterMessage::ID())
    {
        // At the moment scene mouse move handler deals with mouse entry
    }
    else if (msg.id == MouseLeaveMessage::ID())
    {
        _titleBarScene->GetCanvas()->OnMouseLeave();
        _nonClientAreaScene->GetCanvas()->OnMouseLeave();
        for (auto& scene : _activeScenes)
            scene->GetCanvas()->OnMouseLeave();
    }
    else if (msg.id == MouseLeftPressedMessage::ID())
    {
        MouseLeftPressedMessage message;
        message.Decode(msg);
        int x = message.x;
        int y = message.y;

        _BuildMasterPanel()->OnLeftPressed(x, y);
    }
    else if (msg.id == MouseRightPressedMessage::ID())
    {
        MouseRightPressedMessage message;
        message.Decode(msg);
        int x = message.x;
        int y = message.y;

        _BuildMasterPanel()->OnRightPressed(x, y);
    }
    else if (msg.id == MouseLeftReleasedMessage::ID())
    {
        MouseLeftReleasedMessage message;
        message.Decode(msg);
        int x = message.x;
        int y = message.y;

        _BuildMasterPanel()->OnLeftReleased(x, y);
    }
    else if (msg.id == MouseRightReleasedMessage::ID())
    {
        MouseRightReleasedMessage message;
        message.Decode(msg);
        int x = message.x;
        int y = message.y;

        _BuildMasterPanel()->OnRightReleased(x, y);
    }
    else if (msg.id == MouseWheelUpMessage::ID())
    {
        MouseWheelUpMessage message;
        message.Decode(msg);
        int x = message.x;
        int y = message.y;

        _BuildMasterPanel()->OnWheelUp(x, y);
    }
    else if (msg.id == MouseWheelDownMessage::ID())
    {
        MouseWheelDownMessage message;
        message.Decode(msg);
        int x = message.x;
        int y = message.y;

        _BuildMasterPanel()->OnWheelDown(x, y);
    }
    else if (msg.id == KeyDownMessage::ID())
    {
        KeyDownMessage message;
        message.Decode(msg);
        keyboardManager.OnKeyDown(message.keyCode);
    }
    else if (msg.id == KeyUpMessage::ID())
    {
        KeyUpMessage message;
        message.Decode(msg);
        keyboardManager.OnKeyUp(message.keyCode);
    }
    else if (msg.id == CharMessage::ID())
    {
        CharMessage message;
        message.Decode(msg);
        keyboardManager.OnChar(message.character);
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
    _uiThread.join();

    resourceManager.ReleaseResources();
    resourceManager.CoUninit();

    _window.reset();
}

void zwnd::Window::_UIThread()
{
    int framecounter = 0;
    Clock frameTimer = Clock(0);

    // Create frame number debug text rendering resources
    IDWriteFactory* dwriteFactory = nullptr;
    IDWriteTextFormat* dwriteTextFormat = nullptr;
    DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&dwriteFactory)
    );
    dwriteFactory->CreateTextFormat(
        L"Calibri",
        NULL,
        DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        20.0f,
        L"en-us",
        &dwriteTextFormat
    );

    while (true)
    {
        _windowSizeMessage = std::nullopt;
        _window->ProcessQueueMessages([&](WindowMessage msg) { Window::_HandleMessage(msg); });

        ztime::clock[CLOCK_GAME].Update();
        ztime::clock[CLOCK_MAIN].Update();

        _window->LockSize();

        // Check for resize
        if (_windowSizeMessage.has_value())
        {
            int newWidth = _windowSizeMessage->width;
            int newHeight = _windowSizeMessage->height;
            zcom::ResizeInfo resizeInfo;
            resizeInfo.windowMaximized = _windowSizeMessage->maximized;
            resizeInfo.windowMinimized = _windowSizeMessage->minimized;
            resizeInfo.windowRestored = _windowSizeMessage->restored;
            RECT clientAreaMargins = _nonClientAreaScene->GetClientAreaMargins();

            // Resize regular scenes
            for (auto& scene : _activeScenes)
                scene->Resize(
                    newWidth - clientAreaMargins.left - clientAreaMargins.right,
                    newHeight - clientAreaMargins.top - clientAreaMargins.bottom - _titleBarScene->TitleBarHeight(),
                    resizeInfo
                );
            // Resize title bar scene
            ((zcom::Scene*)_titleBarScene.get())->Resize(
                newWidth - clientAreaMargins.left - clientAreaMargins.right,
                _titleBarScene->TitleBarHeight(),
                resizeInfo
            );
            // Resize non-client area scene
            ((zcom::Scene*)_nonClientAreaScene.get())->Resize(newWidth, newHeight, resizeInfo);
        }

        // Render frame
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
            ((zcom::Scene*)_titleBarScene.get())->Update();
            // Update the non-client area scene
            ((zcom::Scene*)_nonClientAreaScene.get())->Update();
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
            if (((zcom::Scene*)_titleBarScene.get())->Redraw())
                redraw = true;
            // Check for non-client area scene redraw
            if (((zcom::Scene*)_nonClientAreaScene.get())->Redraw())
                redraw = true;
        }

        //std::cout << "Updated " << ++framecounter << '\n';
        //redraw = true;
        if (redraw)
        {
            std::cout << "Redrawn (" << framecounter++ << ")\n";
            Graphics g = _window->gfx.GetGraphics();
            g.target->BeginDraw();
            g.target->Clear();

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
            if (((zcom::Scene*)_titleBarScene.get())->Redraw())
                ((zcom::Scene*)_titleBarScene.get())->Draw(g);
            g.target->DrawBitmap(((zcom::Scene*)_titleBarScene.get())->ContentImage());

            // Draw other scenes
            for (auto& scene : activeScenes)
            {
                if (scene->Redraw())
                    scene->Draw(g);
                g.target->DrawBitmap(
                    scene->ContentImage(),
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
            ((zcom::Scene*)_nonClientAreaScene.get())->Draw(g);

            g.target->DrawBitmap(((zcom::Scene*)_nonClientAreaScene.get())->ContentImage());
            clientAreaBitmap->Release();

            if (GetKeyState(VK_SPACE) & 0x8000)
                g.target->Clear(D2D1::ColorF(0.2f, 0.2f, 0.2f, 0.8f));

            // Display frame number while 'Ctrl + S + F' is held
            if ((GetKeyState(VK_CONTROL) & 0x8000) &&
                (GetKeyState('S') & 0x8000) &&
                (GetKeyState('F') & 0x8000))
            {
                IDWriteTextLayout* dwriteTextLayout = nullptr;

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

        _window->gfx.EndFrame(redraw);

        if (_closed)
            break;
    }

    for (auto& scene : _activeScenes)
        scene->Uninit();
    _activeScenes.clear();
    ((zcom::Scene*)_titleBarScene.get())->Uninit();
    _titleBarScene.reset();
    ((zcom::Scene*)_nonClientAreaScene.get())->Uninit();
    _nonClientAreaScene.reset();

    // Release text rendering resources
    dwriteTextFormat->Release();
    dwriteFactory->Release();
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

std::unique_ptr<zcom::Panel> zwnd::Window::_BuildMasterPanel()
{
    RECT margins = _nonClientAreaScene->GetClientAreaMargins();

    std::unique_ptr<zcom::Panel> masterPanel = _nonClientAreaScene->CreatePanel();
    masterPanel->Resize(Width(), Height());
    masterPanel->DeferLayoutUpdates();
    {
        zcom::Panel* panel = _nonClientAreaScene->GetCanvas()->BasePanel();
        panel->SetX(0);
        panel->SetY(0);
        masterPanel->AddItem(panel);
    }
    for (auto& scene : _activeScenes)
    {
        zcom::Panel* panel = scene->GetCanvas()->BasePanel();
        panel->SetX(margins.left);
        panel->SetY(margins.top + _titleBarScene->TitleBarHeight());
        masterPanel->AddItem(panel);
    }
    {
        zcom::Panel* panel = _titleBarScene->GetCanvas()->BasePanel();
        panel->SetX(margins.left);
        panel->SetY(margins.top);
        masterPanel->AddItem(panel);
    }
    for (int i = 0; i < masterPanel->ItemCount(); i++)
        masterPanel->GetItem(i)->SetZIndex(i);
    masterPanel->ResumeLayoutUpdates(false);

    return masterPanel;
}