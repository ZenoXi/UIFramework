#include "WindowBackend.h"

#include "D2DEffects/TintEffect.h"

#include <dwmapi.h>
#pragma comment( lib,"Dwmapi.lib" )
#include <hidusage.h>

//BOOL CALLBACK enum_windows_callback(HWND handle, LPARAM lParam)
//{
//    unsigned long process_id = 0;
//    GetWindowThreadProcessId(handle, &process_id);
//    if (process_id == 27068)
//    {
//        int result = GetWindowLong(handle, GWL_EXSTYLE);
//        RECT rect;
//        GetWindowRect(handle, &rect);
//        std::cout << "RECT: " << rect.left << ':' << rect.top << ':' << rect.right << ':' << rect.bottom << ' ';
//        if (result & WS_EX_LAYERED)
//            std::cout << "(Layered)\n";
//        else
//            std::cout << "(Not layered)\n";
//    }
//
//    return TRUE;
//}

zwnd::WindowBackend::WindowBackend(HINSTANCE hInst, WindowProperties props) : _hInst(hInst), _wndClassName(props.windowClassName.c_str())
{
    _linfo.SetWidth(props.initialWidth);
    _linfo.SetHeight(props.initialHeight);
    _messageWidth = props.initialWidth;
    _messageHeight = props.initialHeight;

    OleInitialize(NULL);

    _cursor = LoadCursor(NULL, IDC_ARROW);
    WNDCLASSEX wc = {
        sizeof(WNDCLASSEX),
        0/*CS_CLASSDC*/,
        _HandleMsgSetup,
        0,
        0,
        hInst,
        NULL,
        NULL,//_cursor,
        nullptr,
        nullptr,
        _wndClassName,
        NULL
    };

    RegisterClassEx(&wc);

    // Calculate initial window size
    RECT workRect;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workRect, 0);
    int x = (workRect.right - props.initialWidth) / 2;
    int y = (workRect.bottom - props.initialHeight) / 2;
    int w = props.initialWidth;
    int h = props.initialHeight;

    // Set windowed rect size
    _windowedRect.left = x;
    _windowedRect.top = y;
    _windowedRect.right = x + w;
    _windowedRect.bottom = y + h;
    _last2Moves[0] = _windowedRect;
    _last2Moves[1] = _windowedRect;

    // Create and show window
    _hwnd = CreateWindowEx(
        WS_EX_LAYERED,
        //WS_EX_APPWINDOW,
        _wndClassName,
        nullptr,
        // WS_THICKFRAME: adds the automatic sizing border
        // WS_SYSMENU: required to show the window in the taskbar
        // WS_MAXIMIZEBOX: enables Aero snapping and the maximize option in the window menu
        // WS_MINIMIZEBOX: enables the minimize option in the window menu
        // WS_CAPTION: automatic window region updating
        WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_CAPTION,
        //WS_POPUP,
        x, y, w, h,
        NULL,
        NULL,
        hInst,
        this
    );

    _fileDropHandler = std::make_unique<FileDropHandler>(_hwnd);
    HRESULT hr = RegisterDragDrop(_hwnd, _fileDropHandler.get());

    gfx.Initialize(&_hwnd);

    int showFlag = SW_SHOWNORMAL;
    ShowWindow(_hwnd, showFlag);

    // Send resize message to UI
    WindowSizeMessage message;
    message.width = _messageWidth;
    message.height = _messageHeight;
    if (showFlag == SW_SHOWMAXIMIZED)
        message.maximized = true;
    else if (showFlag == SW_SHOWMINIMIZED)
        message.minimized = true;
    else if (showFlag == SW_SHOWNORMAL)
        message.restored = true;
    _m_msg.lock();
    _msgQueue.push(message.Encode());
    _m_msg.unlock();
}

zwnd::WindowBackend::~WindowBackend()
{
    gfx.Close();

    RevokeDragDrop(_hwnd);
    DestroyWindow(_hwnd);

    UnregisterClass(_wndClassName, _hInst);
    OleUninitialize();
}

void zwnd::WindowBackend::LockSize()
{
    _m_windowSize.lock();
}

void zwnd::WindowBackend::UnlockSize()
{
    _m_windowSize.unlock();
}

void zwnd::WindowBackend::UpdateLayeredWindow()
{
    HRESULT hr;
    HDC hdc;
    ID2D1GdiInteropRenderTarget* GDIRT;

    hr = gfx.GetGraphics().target->QueryInterface(&GDIRT);
    hr = GDIRT->GetDC(D2D1_DC_INITIALIZE_MODE_COPY, &hdc);

    _linfo.SetWidth((UINT)GetWidth());
    _linfo.SetHeight((UINT)GetHeight());
    _linfo.Update(_hwnd, hdc);

    GDIRT->ReleaseDC(nullptr);
    GDIRT->Release();
}

bool zwnd::WindowBackend::ProcessMessages()
{
    MSG msg;
    bool msgProcessed = false;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        msgProcessed = true;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (msg.message == WM_QUIT)
        {
            exit(0);
        }
    }
    return msgProcessed;
}

void zwnd::WindowBackend::ProcessQueueMessages(std::function<void(WindowMessage)> callback)
{
    std::lock_guard<std::mutex> lock(_m_msg);
    while (!_msgQueue.empty())
    {
        callback(_msgQueue.front());
        //HandleMsgFromQueue(_msgQueue.front());
        _msgQueue.pop();
    }
}

LRESULT WINAPI zwnd::WindowBackend::_HandleMsgSetup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // use create parameter passed in from CreateWindow() to store window class pointer at WinAPI side
    if (msg == WM_NCCREATE)
    {
        // extract ptr to window class from creation data
        const CREATESTRUCTW* const pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        WindowBackend* const pWnd = reinterpret_cast<WindowBackend*>(pCreate->lpCreateParams);
        // set WinAPI-managed user data to store ptr to window class
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWnd));
        // set message proc to normal (non-setup) handler now that setup is finished
        SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowBackend::_HandleMsgThunk));
        // forward message to window class handler
        return pWnd->HandleMsg(hWnd, msg, wParam, lParam);
    }
    // if we get a message before the WM_NCCREATE message, handle with default handler
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT WINAPI zwnd::WindowBackend::_HandleMsgThunk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // retrieve ptr to window class
    WindowBackend* const pWnd = reinterpret_cast<WindowBackend*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    // forward message to window class handler
    return pWnd->HandleMsg(hWnd, msg, wParam, lParam);
}

LRESULT zwnd::WindowBackend::HandleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CLOSE:
    {
        _m_msg.lock();
        _msgQueue.push(WindowCloseMessage().Encode());
        _m_msg.unlock();
        break;
    }
    case WM_CREATE:
    {
        // TODO: investigate why app crashes on a mutex lock when WM_CREATE isn't handled

        RAWINPUTDEVICE Rid[1];
        Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
        Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
        Rid[0].dwFlags = RIDEV_INPUTSINK;
        Rid[0].hwndTarget = hWnd;
        RegisterRawInputDevices(Rid, 1, sizeof(Rid[0]));

        //MARGINS margins = { 0 };
        //DwmExtendFrameIntoClientArea(hWnd, &margins);
        //SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
        break;
    }
    case WM_NCCALCSIZE:
    {
        // Capture all window area as client area
        return 0;
    }
    case WM_NCHITTEST:
    {
        std::lock_guard<std::mutex> lock(_m_hittest);

        // Translate client coordinates to window coordinates
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hWnd, &pt);

        //std::cout << pt.x << ':' << pt.y << '\n';

        // Check for close button hit
        if (pt.x >= _closeButtonRect.left &&
            pt.y >= _closeButtonRect.top &&
            pt.x < _closeButtonRect.right &&
            pt.y < _closeButtonRect.bottom)
        {
            return HTCLOSE;
        }
        // Check for minimize button hit
        if (pt.x >= _minimizeButtonRect.left &&
            pt.y >= _minimizeButtonRect.top &&
            pt.x < _minimizeButtonRect.right &&
            pt.y < _minimizeButtonRect.bottom)
        {
            return HTMINBUTTON;
        }
        // Check for maximize button hit
        if (pt.x >= _maximizeButtonRect.left &&
            pt.y >= _maximizeButtonRect.top &&
            pt.x < _maximizeButtonRect.right &&
            pt.y < _maximizeButtonRect.bottom)
        {
            return HTMAXBUTTON;
        }

        // Extend the corner sizing border hits to make them easier to hit
        constexpr int cornerHitPadding = 10;

        // Check for sizing border hit
        if (pt.x < _resizingBorderMargins.left)
        {
            if (pt.y < _resizingBorderMargins.top + cornerHitPadding)
                return HTTOPLEFT;
            else if (pt.y >= GetHeight() - _resizingBorderMargins.bottom - cornerHitPadding)
                return HTBOTTOMLEFT;
            else
                return HTLEFT;
        }
        else if (pt.x >= GetWidth() - _resizingBorderMargins.right)
        {
            if (pt.y < _resizingBorderMargins.top + cornerHitPadding)
                return HTTOPRIGHT;
            else if (pt.y >= GetHeight() - _resizingBorderMargins.bottom - cornerHitPadding)
                return HTBOTTOMRIGHT;
            else
                return HTRIGHT;
        }
        else
        {
            if (pt.y < _resizingBorderMargins.top)
            {
                if (pt.x < _resizingBorderMargins.left + cornerHitPadding)
                    return HTTOPLEFT;
                else if (pt.x >= GetWidth() - _resizingBorderMargins.right - cornerHitPadding)
                    return HTTOPRIGHT;
                else
                    return HTTOP;
            }
            else if (pt.y >= GetHeight() - _resizingBorderMargins.bottom)
            {
                if (pt.x < _resizingBorderMargins.left + cornerHitPadding)
                    return HTBOTTOMLEFT;
                else if (pt.x >= GetWidth() - _resizingBorderMargins.right - cornerHitPadding)
                    return HTBOTTOMRIGHT;
                else
                    return HTBOTTOM;
            }
            else; // Continue checking other hit parts
        }

        // Check for menu button hit
        if (pt.x >= _winMenuButtonRect.left &&
            pt.y >= _winMenuButtonRect.top &&
            pt.x < _winMenuButtonRect.right &&
            pt.y < _winMenuButtonRect.bottom)
        {
            return HTSYSMENU;
        }

        // Check for client area
        if (pt.x >= _clientAreaMargins.left &&
            pt.y >= _clientAreaMargins.top &&
            pt.x < GetWidth() - _clientAreaMargins.right &&
            pt.y < GetHeight() - _clientAreaMargins.bottom)
        {
            // Check for content area
            if (pt.y >= _clientAreaMargins.top + _titleBarHeight)
                return HTCLIENT;

            // Check for caption area in the title bar
            bool inExcludedRect = false;
            for (int i = 0; i < _excludedCaptionRects.size(); i++)
            {
                if (pt.x >= _excludedCaptionRects[i].left &&
                    pt.y >= _excludedCaptionRects[i].top &&
                    pt.x < _excludedCaptionRects[i].right &&
                    pt.y < _excludedCaptionRects[i].bottom)
                {
                    inExcludedRect = true;
                    break;
                }
            }

            if (!inExcludedRect)
                return HTCAPTION;
            else
                return HTCLIENT;
        }

        return HTNOWHERE;
    }
    case WM_INPUT:
    {
        UINT dwSize = sizeof(RAWINPUT);
        static BYTE lpb[sizeof(RAWINPUT)];

        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));

        RAWINPUT* raw = (RAWINPUT*)lpb;

        if (raw->header.dwType == RIM_TYPEMOUSE)
        {
            int x = raw->data.mouse.lLastX;
            int y = raw->data.mouse.lLastY;
            std::cout << x << ":" << y << '\n';
        }
        break;
    }
    case WM_MOUSEMOVE:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        //std::cout << x << ":" << y << '\n';

        if (x != _lastMouseMove.x || y != _lastMouseMove.y)
        {
            _lastMouseMove.x = x;
            _lastMouseMove.y = y;
        }
        else
        {
            break;
        }

        _m_msg.lock();
        if (x >= 0 && x < GetWidth() && y >= 0 && y < GetHeight())
        {
            if (!_mouseInWindow)
            {
                TRACKMOUSEEVENT ev;
                ev.cbSize = sizeof(TRACKMOUSEEVENT);
                ev.dwFlags = TME_LEAVE;
                ev.hwndTrack = hWnd;
                TrackMouseEvent(&ev);
                _prevCursor = GetCursor();

                _msgQueue.push(MouseEnterMessage().Encode());
            }
        }
        else
        {
            if (wParam & (MK_LBUTTON | MK_RBUTTON))
            {

            }
            else
            {
                SetCursor(_prevCursor);
            }
        }

        MouseMoveMessage moveMsg;
        moveMsg.x = x;
        moveMsg.y = y;
        _msgQueue.push(moveMsg.Encode());
        _m_msg.unlock();
        break;
    }
    case WM_MOUSELEAVE:
    {
        _mouseInWindow = false;
        _m_msg.lock();
        _msgQueue.push(MouseLeaveMessage().Encode());
        _m_msg.unlock();
        break;
    }
    case WM_SETCURSOR:
    {
        if (LOWORD(lParam) != HTCLIENT)
            return DefWindowProc(hWnd, msg, wParam, lParam);

        _m_msg.lock();
        SetCursor(_cursor);
        _m_msg.unlock();
        break;
    }
    case WM_LBUTTONDOWN:
    {
        SetCapture(_hwnd);
        SetWindowPos(_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);

        MouseLeftPressedMessage message;
        message.x = GET_X_LPARAM(lParam);
        message.y = GET_Y_LPARAM(lParam);
        _m_msg.lock();
        _msgQueue.push(message.Encode());
        _m_msg.unlock();
        break;
    }
    case WM_LBUTTONUP:
    {
        ReleaseCapture();

        MouseLeftReleasedMessage message;
        message.x = GET_X_LPARAM(lParam);
        message.y = GET_Y_LPARAM(lParam);
        _m_msg.lock();
        _msgQueue.push(message.Encode());
        _m_msg.unlock();
        break;
    }
    case WM_RBUTTONDOWN:
    {
        MouseRightPressedMessage message;
        message.x = GET_X_LPARAM(lParam);
        message.y = GET_Y_LPARAM(lParam);
        _m_msg.lock();
        _msgQueue.push(message.Encode());
        _m_msg.unlock();
        break;
    }
    case WM_RBUTTONUP:
    {
        MouseRightReleasedMessage message;
        message.x = GET_X_LPARAM(lParam);
        message.y = GET_Y_LPARAM(lParam);
        _m_msg.lock();
        _msgQueue.push(message.Encode());
        _m_msg.unlock();
        break;
    }
    case WM_MOUSEWHEEL:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        WindowMessage message;
        if (GET_WHEEL_DELTA_WPARAM(wParam) > 0)
        {
            MouseWheelUpMessage upMessage;
            upMessage.x = x;
            upMessage.y = y;
            message = upMessage.Encode();
        }
        else if (GET_WHEEL_DELTA_WPARAM(wParam) < 0)
        {
            MouseWheelDownMessage downMessage;
            downMessage.x = x;
            downMessage.y = y;
            message = downMessage.Encode();
        }

        _m_msg.lock();
        _msgQueue.push(message);
        _m_msg.unlock();
        break;
    }
    case WM_MOVE:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        if (!_fullscreen)
        {
            int w = _windowedRect.right - _windowedRect.left;
            int h = _windowedRect.bottom - _windowedRect.top;
            _last2Moves[0] = _last2Moves[1];
            _last2Moves[1].left = x;
            _last2Moves[1].top = y;
            _last2Moves[1].right = x + w;
            _last2Moves[1].bottom = y + h;
        }

        WindowMoveMessage message;
        message.x = x;
        message.y = y;
        _m_msg.lock();
        _msgQueue.push(message.Encode());
        _m_msg.unlock();
        break;
    }
    //case WM_GETMINMAXINFO:
    //{
    //    MINMAXINFO* info = (MINMAXINFO*)lParam;
    //    info->ptMinTrackSize.x = 640;
    //    info->ptMinTrackSize.y = 480;
    //    break;
    //}
    case WM_ENTERSIZEMOVE:
    {
        _sizingStarted = true;
        break;
    }
    case WM_EXITSIZEMOVE:
    {
        _sizingStarted = false;
        break;
    }
    case WM_WINDOWPOSCHANGING:
    {
        WINDOWPOS* pos = (WINDOWPOS*)lParam;

        // Check if sizing occurs
        if (!(pos->flags & SWP_NOSIZE) && (_messageWidth != pos->cx || _messageHeight != pos->cy))
        {
            // Wait for window sizing to become available
            _m_windowSize.lock();
        }
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    case WM_SIZE:
    {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        //std::cout << w << ":" << h << '\n';

        if (_messageWidth == w && _messageHeight == h)
            break;

        _messageWidth = w;
        _messageHeight = h;

        WindowSizeMessage message;
        message.width = w;
        message.height = h;

        if (wParam == SIZE_RESTORED && !_fullscreen)
        {
            _last2Moves[0] = _last2Moves[1];
            GetWindowRect(_hwnd, &_last2Moves[1]);
            GetWindowRect(_hwnd, &_windowedRect);
            if (_maximized || _minimized)
                message.restored = true;
            _maximized = false;
            _minimized = false;
        }
        if (wParam == SIZE_MAXIMIZED)
        {
            _last2Moves[1] = _last2Moves[0];
            _maximized = true;
            _minimized = false;
            message.maximized = true;
        }
        if (wParam == SIZE_MINIMIZED)
        {
            _last2Moves[1] = _last2Moves[0];
            _minimized = true;
            _maximized = false;
            message.minimized = true;
        }

        gfx.ResizeBuffers(w, h, false);

        _m_msg.lock();
        _msgQueue.push(message.Encode());
        _m_msg.unlock();

        _m_windowSize.unlock();

        return DefWindowProc(hWnd, msg, wParam, lParam);
        break;
    }
    case WM_KEYDOWN:
    {
        KeyDownMessage message;
        message.keyCode = wParam;
        message.repeatCount = (uint16_t)(lParam & 0xFFFF);
        message.scanCode = (uint8_t)((lParam >> 16) & 0xFF);
        message.isExtended = (lParam & KF_EXTENDED) == KF_ALTDOWN;
        message.repeated = (lParam & KF_REPEAT) == KF_ALTDOWN;

        _m_msg.lock();
        _msgQueue.push(message.Encode());
        _m_msg.unlock();
        break;
    }
    case WM_KEYUP:
    {
        KeyUpMessage message;
        message.keyCode = wParam;
        message.scanCode = (uint8_t)((lParam >> 16) & 0xFF);
        message.isExtended = (lParam & KF_EXTENDED) == KF_ALTDOWN;

        _m_msg.lock();
        _msgQueue.push(message.Encode());
        _m_msg.unlock();
        break;
    }
    case WM_SYSKEYDOWN:
    {
        KeyDownMessage message;
        message.keyCode = wParam;
        message.repeatCount = (uint16_t)(lParam & 0xFFFF);
        message.scanCode = (uint8_t)((lParam >> 16) & 0xFF);
        message.isExtended = (lParam & KF_EXTENDED) == KF_ALTDOWN;
        message.repeated = (lParam & KF_REPEAT) == KF_ALTDOWN;

        // Translate to a regular key down message
        bool contextCode = (lParam & KF_ALTDOWN) == KF_ALTDOWN;
        if (!contextCode)
        {
            _m_msg.lock();
            _msgQueue.push(message.Encode());
            _m_msg.unlock();
        }
        else
        {
            // null context code
            lParam &= ~KF_ALTDOWN;

            _m_msg.lock();
            message.keyCode = VK_MENU;
            _msgQueue.push(message.Encode());
            message.keyCode = wParam;
            _msgQueue.push(message.Encode());
            _m_msg.unlock();
        }

        break;
    }
    case WM_SYSKEYUP:
    {
        KeyUpMessage message;
        message.keyCode = wParam;
        message.scanCode = (uint8_t)((lParam >> 16) & 0xFF);
        message.isExtended = (lParam & KF_EXTENDED) == KF_ALTDOWN;

        // Translate to a regular key up message
        bool contextCode = (lParam & KF_ALTDOWN) == KF_ALTDOWN;
        if (!contextCode)
        {
            _m_msg.lock();
            _msgQueue.push(message.Encode());
            _m_msg.unlock();
        }
        else
        {
            // null context code
            lParam &= ~KF_ALTDOWN;

            _m_msg.lock();
            message.keyCode = VK_MENU;
            _msgQueue.push(message.Encode());
            message.keyCode = wParam;
            _msgQueue.push(message.Encode());
            _m_msg.unlock();
        }

        break;
    }
    case WM_CHAR:
    {
        CharMessage message;
        message.character = wParam;
        message.repeatCount = (uint16_t)(lParam & 0xFFFF);
        message.scanCode = (uint8_t)((lParam >> 16) & 0xFF);

        _m_msg.lock();
        _msgQueue.push(message.Encode());
        _m_msg.unlock();
        break;
    }
    default:
    {
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    }

    return 0;
}

void zwnd::WindowBackend::HandleFullscreenChange()
{
    std::lock_guard<std::mutex> lock(_m_fullscreen);
    if (_fullscreenChanged)
    {
        _fullscreenChanged = false;

        if (_fullscreen)
        {
            WINDOWPLACEMENT placement;
            placement.length = sizeof(WINDOWPLACEMENT);
            GetWindowPlacement(_hwnd, &placement);
            _windowedMaximized = (placement.showCmd == SW_SHOWMAXIMIZED);

            HMONITOR hMonitor = MonitorFromWindow(_hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO info;
            info.cbSize = sizeof(MONITORINFO);
            GetMonitorInfo(hMonitor, &info);
            RECT monitor = info.rcMonitor;
            int w = monitor.right - monitor.left;
            int h = monitor.bottom - monitor.top;

            SetWindowLong(_hwnd, GWL_STYLE, WS_POPUP);
            SetWindowPos(_hwnd, HWND_TOP, monitor.left, monitor.top, w, h, SWP_FRAMECHANGED);
            // Window region gets stuck on the non-fullscreen window size and prevents the window from rendering fullscreen
            // This is somehow related to the window being a layered window
            SetWindowRgn(_hwnd, NULL, FALSE);

            ShowWindow(_hwnd, SW_SHOW);
        }
        else
        {
            int x = _last2Moves[1].left;
            int y = _last2Moves[1].top;
            int w = _last2Moves[1].right - _last2Moves[1].left;
            int h = _last2Moves[1].bottom - _last2Moves[1].top;

            // NOTE:
            // SETTING WS_CAPTION IS MANDATORY!!!!!!
            // Not setting the WS_CAPTION style leaves window region updating disabled for some reason
            // and the window behavior remains similar to a popup window, with no animations when
            // minimizing/maximizing and a dysfunctional taskbar icon
            SetWindowLongPtr(_hwnd, GWL_STYLE, WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_CAPTION);
            SetWindowPos(_hwnd, NULL, x, y, w, h, SWP_FRAMECHANGED);
            
            // NOTE:
            // WS_DLGFRAME style is applied automatically to non-child windows

            if (_windowedMaximized)
                ShowWindow(_hwnd, SW_SHOWMAXIMIZED);
            else
                ShowWindow(_hwnd, SW_SHOW);
        }
    }
}

void zwnd::WindowBackend::SetCursorIcon(CursorIcon cursor)
{
    switch (cursor)
    {
    case CursorIcon::APP_STARTING:
    {
        _cursor = LoadCursor(NULL, IDC_APPSTARTING);
        //SetCursor(_cursor);
        break;
    }
    case CursorIcon::ARROW:
    {
        _cursor = LoadCursor(NULL, IDC_ARROW);
        //SetCursor(_cursor);
        break;
    }
    case CursorIcon::CROSS:
    {
        _cursor = LoadCursor(NULL, IDC_CROSS);
        //SetCursor(_cursor);
        break;
    }
    case CursorIcon::HAND:
    {
        _cursor = LoadCursor(NULL, IDC_HAND);
        //SetCursor(_cursor);
        break;
    }
    case CursorIcon::HELP:
    {
        _cursor = LoadCursor(NULL, IDC_HELP);
        //SetCursor(_cursor);
        break;
    }
    case CursorIcon::IBEAM:
    {
        _cursor = LoadCursor(NULL, IDC_IBEAM);
        //SetCursor(_cursor);
        break;
    }
    case CursorIcon::NO:
    {
        _cursor = LoadCursor(NULL, IDC_NO);
        //SetCursor(_cursor);
        break;
    }
    case CursorIcon::SIZE_ALL:
    {
        _cursor = LoadCursor(NULL, IDC_SIZEALL);
        //SetCursor(_cursor);
        break;
    }
    case CursorIcon::SIZE_NESW:
    {
        _cursor = LoadCursor(NULL, IDC_SIZENESW);
        //SetCursor(_cursor);
        break;
    }
    case CursorIcon::SIZE_NS:
    {
        _cursor = LoadCursor(NULL, IDC_SIZENS);
        //SetCursor(_cursor);
        break;
    }
    case CursorIcon::SIZE_NWSE:
    {
        _cursor = LoadCursor(NULL, IDC_SIZENWSE);
        //SetCursor(_cursor);
        break;
    }
    case CursorIcon::SIZE_WE:
    {
        _cursor = LoadCursor(NULL, IDC_SIZEWE);
        //SetCursor(_cursor);
        break;
    }
    case CursorIcon::UP_ARROW:
    {
        _cursor = LoadCursor(NULL, IDC_UPARROW);
        //SetCursor(_cursor);
        break;
    }
    case CursorIcon::WAIT:
    {
        _cursor = LoadCursor(NULL, IDC_WAIT);
        //SetCursor(_cursor);
        break;
    }
    default:
        break;
    }
}

void zwnd::WindowBackend::SetCursorVisibility(bool visible)
{
    if (visible != _cursorVisible)
    {
        _cursorVisible = visible;
        _cursorVisibilityChanged = true;
    }
}

void zwnd::WindowBackend::HandleCursorVisibility()
{
    if (_cursorVisibilityChanged)
    {
        _cursorVisibilityChanged = false;

        if (!_cursorVisible)
        {
            int value;
            while ((value = ShowCursor(false)) > -1);
            while (value < -1) value = ShowCursor(true);
        }
        else
        {
            int value;
            while ((value = ShowCursor(true)) < 1);
            while (value > 1) value = ShowCursor(false);
        }
    }
}

void zwnd::WindowBackend::ResetScreenTimer()
{
    SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
}

void zwnd::WindowBackend::AddKeyboardHandler(KeyboardEventHandler* handler)
{
    _keyboardHandlers.push_back(handler);
}

bool zwnd::WindowBackend::RemoveKeyboardHandler(KeyboardEventHandler* handler)
{
    for (auto it = _keyboardHandlers.begin(); it != _keyboardHandlers.end(); it++)
    {
        if (*it == handler)
        {
            _keyboardHandlers.erase(it);
            return true;
        }
    }
    return false;
}

void zwnd::WindowBackend::AddDragDropHandler(IDragDropEventHandler* handler)
{
    _fileDropHandler->AddDragDropEventHandler(handler);
}

bool zwnd::WindowBackend::RemoveDragDropHandler(IDragDropEventHandler* handler)
{
    return _fileDropHandler->RemoveDragDropEventHandler(handler);
}

RECT zwnd::WindowBackend::GetWindowRectangle()
{
    RECT rect;
    GetWindowRect(_hwnd, &rect);
    return rect;
}

void zwnd::WindowBackend::SetWindowRectangle(RECT rect)
{
    SetWindowPos(_hwnd, 0, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_ASYNCWINDOWPOS);
}

int zwnd::WindowBackend::GetWidth()
{
    RECT windowRect = GetWindowRectangle();
    return windowRect.right - windowRect.left;
}

int zwnd::WindowBackend::GetHeight()
{
    RECT windowRect = GetWindowRectangle();
    return windowRect.bottom - windowRect.top;
}

void zwnd::WindowBackend::SetWidth(int newWidth)
{
    SetSize(newWidth, GetHeight());
}

void zwnd::WindowBackend::SetHeight(int newHeight)
{
    SetSize(GetWidth(), newHeight);
}

void zwnd::WindowBackend::SetSize(int newWidth, int newHeight)
{
    SetWindowPos(_hwnd, 0, 0, 0, newWidth, newHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_ASYNCWINDOWPOS);
}

int zwnd::WindowBackend::GetXPos()
{
    return GetWindowRectangle().left;
}

int zwnd::WindowBackend::GetYPos()
{
    return GetWindowRectangle().top;
}

void zwnd::WindowBackend::SetXPos(int newX)
{
    SetPosition(newX, GetYPos());
}

void zwnd::WindowBackend::SetYPos(int newY)
{
    SetPosition(GetXPos(), newY);
}

void zwnd::WindowBackend::SetPosition(int newX, int newY)
{
    SetWindowPos(_hwnd, 0, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_ASYNCWINDOWPOS);
}

bool zwnd::WindowBackend::Maximized()
{
    WINDOWPLACEMENT placement;
    placement.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(_hwnd, &placement);
    return placement.showCmd == SW_SHOWMAXIMIZED;
}

void zwnd::WindowBackend::Maximize()
{
    PostMessage(_hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
}

bool zwnd::WindowBackend::Minimized()
{
    WINDOWPLACEMENT placement;
    placement.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(_hwnd, &placement);
    return placement.showCmd == SW_SHOWMINIMIZED;
}

void zwnd::WindowBackend::Minimize()
{
    PostMessage(_hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
}

void zwnd::WindowBackend::Restore()
{
    PostMessage(_hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
}

void zwnd::WindowBackend::SetFullscreen(bool fullscreen)
{
    std::lock_guard<std::mutex> lock(_m_fullscreen);
    if (_fullscreen == fullscreen)
        return;
    _fullscreenChanged = true;
    _fullscreen = fullscreen;
}

bool zwnd::WindowBackend::IsFullscreen()
{
    return _fullscreen;
}

void zwnd::WindowBackend::SetResizingBorderMargins(RECT margins)
{
    std::lock_guard<std::mutex> lock(_m_hittest);
    _resizingBorderMargins = margins;
}

void zwnd::WindowBackend::SetClientAreaMargins(RECT margins)
{
    std::lock_guard<std::mutex> lock(_m_hittest);
    _clientAreaMargins = margins;
}

void zwnd::WindowBackend::SetTitleBarHeight(int height)
{
    std::lock_guard<std::mutex> lock(_m_hittest);
    _titleBarHeight = height;
}

void zwnd::WindowBackend::SetCloseButtonRect(RECT rect)
{
    std::lock_guard<std::mutex> lock(_m_hittest);
    _closeButtonRect = rect;
}

void zwnd::WindowBackend::SetMinimizeButtonRect(RECT rect)
{
    std::lock_guard<std::mutex> lock(_m_hittest);
    _minimizeButtonRect = rect;
}

void zwnd::WindowBackend::SetMaximizeButtonRect(RECT rect)
{
    std::lock_guard<std::mutex> lock(_m_hittest);
    _maximizeButtonRect = rect;
}

void zwnd::WindowBackend::SetWinMenuButtonRect(RECT rect)
{
    std::lock_guard<std::mutex> lock(_m_hittest);
    _winMenuButtonRect = rect;
}

void zwnd::WindowBackend::SetExcludedCaptionRects(const std::vector<RECT>& rects)
{
    std::lock_guard<std::mutex> lock(_m_hittest);
    _excludedCaptionRects = rects;
}