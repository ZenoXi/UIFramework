#include "WindowBackend.h"

#include "D2DEffects/TintEffect.h"

#include <dwmapi.h>
#pragma comment( lib,"Dwmapi.lib" )
#include <hidusage.h>

#include "Helper/Time.h"

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

zwnd::WindowBackend::WindowBackend(HINSTANCE hInst, WindowProperties props, HWND parentWindow)
  : _hInst(hInst),
    _parentHwnd(parentWindow),
    _wndClassName(props.windowClassName.c_str())
{
    _linfo.SetWidth(props.initialWidth);
    _linfo.SetHeight(props.initialHeight);
    _messageWidth = props.initialWidth;
    _messageHeight = props.initialHeight;
    _activationDisabled = props.disableWindowActivation;

    HRESULT hr = OleInitialize(NULL);

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
    RECT workRect = {};
    if (props.ignoreTaskbarForPlacement)
    {
        workRect.left = 0;
        workRect.top = 0;
        workRect.right = GetSystemMetrics(SM_CXSCREEN);
        workRect.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    else
    {
        SystemParametersInfo(SPI_GETWORKAREA, 0, &workRect, 0);
    }
    int x = (workRect.right - props.initialWidth) / 2;
    int y = (workRect.bottom - props.initialHeight) / 2;
    int w = props.initialWidth;
    int h = props.initialHeight;
    if (props.initialXOffset)
        x = props.initialXOffset.value();
    if (props.initialYOffset)
        y = props.initialYOffset.value();

    _minWidth = props.minWidth;
    _minHeight = props.minHeight;
    _maxWidth = props.maxWidth;
    _maxHeight = props.maxHeight;
    if (props.fixedWidth)
    {
        _minWidth = w;
        _maxWidth = w;
    }
    if (props.fixedHeight)
    {
        _minHeight = h;
        _maxHeight = h;
    }

    // Set windowed rect size
    _windowedRect.left = x;
    _windowedRect.top = y;
    _windowedRect.right = x + w;
    _windowedRect.bottom = y + h;
    _last2Moves[0] = _windowedRect;
    _last2Moves[1] = _windowedRect;

    // WS_THICKFRAME: adds the automatic sizing border
    // WS_SYSMENU: required to show the window in the taskbar
    // WS_MAXIMIZEBOX: enables Aero snapping and the maximize option in the window menu
    // WS_MINIMIZEBOX: enables the minimize option in the window menu
    // WS_CAPTION: automatic window region updating
    DWORD windowStyle = WS_THICKFRAME | WS_SYSMENU | WS_CAPTION;
    if (!props.disableMaximizing)
        windowStyle |= WS_MAXIMIZEBOX;
    if (!props.disableMinimizing)
        windowStyle |= WS_MINIMIZEBOX;
    
    // Create and show window
    _hwnd = CreateWindowEx(
        WS_EX_LAYERED | (_activationDisabled ? WS_EX_NOACTIVATE : NULL) | (props.disableMouseInteraction ? WS_EX_TRANSPARENT : NULL) | (props.topMost ? WS_EX_TOPMOST : NULL),
        _wndClassName,
        nullptr,
        windowStyle,
        x, y, w, h,
        parentWindow,
        NULL,
        hInst,
        this
    );

    _fileDropHandler = std::make_unique<FileDropHandler>(_hwnd);
    hr = RegisterDragDrop(_hwnd, _fileDropHandler.get());

    if (props.disableWindowAnimations)
    {
        BOOL attrib = TRUE;
        DwmSetWindowAttribute(_hwnd, DWMWA_TRANSITIONS_FORCEDISABLED, &attrib, sizeof(attrib));
    }

    gfx.Initialize(&_hwnd);

    int showFlag = SW_SHOWNORMAL;
    switch (props.initialDisplay)
    {
    case zwnd::WindowDisplayType::NORMAL: { showFlag = SW_SHOWNORMAL; break; }
    case zwnd::WindowDisplayType::NORMAL_NOACTIVATE: { showFlag = SW_SHOWNA; break; }
    case zwnd::WindowDisplayType::MINIMIZED: { showFlag = SW_SHOWMINIMIZED; break; }
    case zwnd::WindowDisplayType::MAXIMIZED: { showFlag = SW_SHOWMAXIMIZED; break; }
    case zwnd::WindowDisplayType::HIDDEN: { showFlag = SW_HIDE; break; }
    }
    if (_activationDisabled && showFlag == SW_SHOWNORMAL)
        showFlag = SW_SHOWNOACTIVATE;

    if (showFlag != SW_HIDE)
    {
        _insideInitialShowWindowCall = true;
        ShowWindow(_hwnd, showFlag);
        _insideInitialShowWindowCall = false;
        _initialShowWindowCallDone = true;

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
}

zwnd::WindowBackend::WindowBackend(HINSTANCE hInst)
    : _hInst(hInst),
    _wndClassName(L"wndClassMessage")
{
    _messageOnly = true;

    WNDCLASSEX wc = {
        sizeof(WNDCLASSEX),
        0,
        _HandleMsgSetup,
        0,
        0,
        hInst,
        NULL,
        NULL,
        nullptr,
        nullptr,
        _wndClassName,
        NULL
    };
    RegisterClassEx(&wc);

    _hwnd = CreateWindowEx(
        NULL,
        _wndClassName,
        nullptr,
        NULL,
        0, 0, 0, 0,
        HWND_MESSAGE,
        NULL,
        hInst,
        this
    );
}

zwnd::WindowBackend::~WindowBackend()
{
    if (_messageOnly)
    {
        DestroyWindow(_hwnd);
        UnregisterClass(_wndClassName, _hInst);
    }
    else
    {
        gfx.Close();

        RevokeDragDrop(_hwnd);
        DestroyWindow(_hwnd);

        UnregisterClass(_wndClassName, _hInst);
        OleUninitialize();
    }
}

void zwnd::WindowBackend::KillMessageLoop()
{
    PostMessage(_hwnd, WM_APP_KILL_MESSAGE_LOOP, NULL, NULL);
}

void zwnd::WindowBackend::LockSize()
{
    _m_windowSize.lock();
}

void zwnd::WindowBackend::UnlockSize()
{
    // Disable not-locked mutex warning
#pragma warning( push )
#pragma warning( disable : 26110 )
    _m_windowSize.unlock();
#pragma warning( pop )
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

void zwnd::WindowBackend::ProcessMessages()
{
    MSG msg;
    BOOL ret;
    while ((ret = GetMessage(&msg, nullptr, 0, 0)) != 0)
    {
        if (ret == -1)
            break;
        if (msg.message == WM_APP_KILL_MESSAGE_LOOP)
            break;
        if (msg.message == WM_QUIT)
            exit(0);

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // NOTE:
    // If the message loop uses PeekMessage, sleeping between PeekMessage calls causes
    // unexpected behavior in certain cases. The main one is the following:
    // 
    // If you have 2 windows, one of which owns the other (when creating the second
    // window, the hwnd of the first is used as hWndParent in CreateWindow(Ex) function),
    // switching focus between the two will cause the window that is being focused to
    // freeze for a short time. This is likely caused by the fact that the PeekMessage
    // call is delayed on the other window, but the weird part is that the freeze is way
    // longer than the sleep duration - the sleep is usually <20ms, while the freeze
    // lasts for up to a second. Perhaps some internal WinApi behaviour depends on the
    // second window processing the message immediatelly and gets stuck if a delay is
    // introduced, but this is only speculation.
    //
}

bool zwnd::WindowBackend::ProcessSingleMessage()
{
    MSG msg;
    BOOL ret;
    if ((ret = GetMessage(&msg, nullptr, 0, 0)) != 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (ret == -1)
        return false;
    if (msg.message == WM_APP_KILL_MESSAGE_LOOP)
        return false;
    if (msg.message == WM_QUIT)
        exit(0);

    return true;
}

void zwnd::WindowBackend::ProcessQueueMessages(std::function<void(WindowMessage)> callback)
{
    // Create a copy of the queue and process the messages without blocking the message thread
    // Not doing this leads to a deadlock when trying to create a child window from the UI thread
    std::unique_lock<std::mutex> lock(_m_msg);
    std::queue<WindowMessage> msgQueueCopy = _msgQueue;
    while (!_msgQueue.empty())
        _msgQueue.pop();
    lock.unlock();

    while (!msgQueueCopy.empty())
    {
        callback(msgQueueCopy.front());
        msgQueueCopy.pop();
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
    static int counter = 0;
    //if (_parentHwnd != NULL)
    //    std::cout << msg << '\n';

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

        if (_messageOnly)
        {
            RAWINPUTDEVICE Rid[1] = {};
            Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
            Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
            Rid[0].dwFlags = RIDEV_INPUTSINK;
            Rid[0].hwndTarget = hWnd;
            RegisterRawInputDevices(Rid, 1, sizeof(Rid[0]));
        }

        //MARGINS margins = { 0 };
        //DwmExtendFrameIntoClientArea(hWnd, &margins);
        //SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
        break;
    }
    case WM_NCACTIVATE:
    {
        WindowActivateMessage message = {};
        message.activationType = wParam == TRUE ? WindowActivateMessage::ACTIVATED : WindowActivateMessage::DEACTIVATED;
        _m_msg.lock();
        _msgQueue.push(message.Encode());
        _m_msg.unlock();

        // DefWindowProc for this message causes a short but very noticeable freeze in the message pipeline
        // when a window has owned windows or is an owned window.
        // Microsoft documentation states that the function "draws the title bar or icon title in its active
        // colors when the wParam parameter is TRUE and in its inactive colors when wParam is FALSE".
        // I can't say why the freeze occurs, but since the title bar is custom drawn, the default processing
        // for this message is unnecessary.

        // UPDATE: returning false completely breaks parent/owned window activation behaviour. The behaviour
        // explained above was a result of using sleep in a PeekMessage call loop. In-depth explanation in a
        // comment in ProcessMessages() function

        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    case WM_NCCALCSIZE:
    {
        // Capture all window area as client area
        return 0;
    }
    case WM_ACTIVATE:
    {
        WindowActivateMessage message = {};
        message.activationType = (int)wParam;
        _m_msg.lock();
        _msgQueue.push(message.Encode());
        _m_msg.unlock();
        break;
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
            int deltaX = raw->data.mouse.lLastX;
            int deltaY = raw->data.mouse.lLastY;
            //std::cout << _hwnd << " - " << deltaX << ":" << deltaY << '\n';

            MouseInputMessage message = {};
            message.deltaX = deltaX;
            message.deltaY = deltaY;
            _m_msg.lock();
            _msgQueue.push(message.Encode());
            _m_msg.unlock();
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
                TRACKMOUSEEVENT ev = {};
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

        MouseMoveMessage moveMsg = {};
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
        {
            _cursorInClientArea = false;
            return DefWindowProc(hWnd, msg, wParam, lParam);
        }
        _cursorInClientArea = true;

        SetCursor(_cursor);
        break;
    }
    case WM_LBUTTONDOWN:
    {
        SetCapture(_hwnd);

        MouseLeftPressedMessage message = {};
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

        MouseLeftReleasedMessage message = {};
        message.x = GET_X_LPARAM(lParam);
        message.y = GET_Y_LPARAM(lParam);
        _m_msg.lock();
        _msgQueue.push(message.Encode());
        _m_msg.unlock();
        break;
    }
    case WM_RBUTTONDOWN:
    {
        MouseRightPressedMessage message = {};
        message.x = GET_X_LPARAM(lParam);
        message.y = GET_Y_LPARAM(lParam);
        _m_msg.lock();
        _msgQueue.push(message.Encode());
        _m_msg.unlock();
        break;
    }
    case WM_RBUTTONUP:
    {
        MouseRightReleasedMessage message = {};
        message.x = GET_X_LPARAM(lParam);
        message.y = GET_Y_LPARAM(lParam);
        _m_msg.lock();
        _msgQueue.push(message.Encode());
        _m_msg.unlock();
        break;
    }
    case WM_NCMOUSEMOVE:
    {
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    case WM_NCLBUTTONDOWN:
    {
        NonClientMouseLeftPressedMessage message = {};
        message.x = GET_X_LPARAM(lParam);
        message.y = GET_Y_LPARAM(lParam);
        _m_msg.lock();
        _msgQueue.push(message.Encode());
        _m_msg.unlock();
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    case WM_NCLBUTTONUP:
    {
        NonClientMouseLeftReleasedMessage message = {};
        message.x = GET_X_LPARAM(lParam);
        message.y = GET_Y_LPARAM(lParam);
        _m_msg.lock();
        _msgQueue.push(message.Encode());
        _m_msg.unlock();
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    case WM_NCRBUTTONDOWN:
    {
        NonClientMouseRightPressedMessage message = {};
        message.x = GET_X_LPARAM(lParam);
        message.y = GET_Y_LPARAM(lParam);
        _m_msg.lock();
        _msgQueue.push(message.Encode());
        _m_msg.unlock();
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    case WM_NCRBUTTONUP:
    {
        NonClientMouseRightReleasedMessage message = {};
        message.x = GET_X_LPARAM(lParam);
        message.y = GET_Y_LPARAM(lParam);
        _m_msg.lock();
        _msgQueue.push(message.Encode());
        _m_msg.unlock();
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    case WM_MOUSEWHEEL:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        WindowMessage message = {};
        if (GET_WHEEL_DELTA_WPARAM(wParam) > 0)
        {
            MouseWheelUpMessage upMessage = {};
            upMessage.x = x;
            upMessage.y = y;
            message = upMessage.Encode();
        }
        else if (GET_WHEEL_DELTA_WPARAM(wParam) < 0)
        {
            MouseWheelDownMessage downMessage = {};
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

        WindowMoveMessage message = {};
        message.x = x;
        message.y = y;
        _m_msg.lock();
        _msgQueue.push(message.Encode());
        _m_msg.unlock();
        break;
    }
    case WM_GETMINMAXINFO:
    {
        MINMAXINFO* info = (MINMAXINFO*)lParam;
        info->ptMinTrackSize.x = _minWidth;
        info->ptMinTrackSize.y = _minHeight;
        info->ptMaxTrackSize.x = _maxWidth;
        info->ptMaxTrackSize.y = _maxHeight;
        break;
    }
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
            if (!_insideInitialShowWindowCall)
                _m_windowSize.lock();
        }

        // Disable unreleased mutex warning
#pragma warning( push )
#pragma warning( disable : 26115 )
        return DefWindowProc(hWnd, msg, wParam, lParam);
#pragma warning( pop )
    }
    case WM_SIZE:
    {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        //std::cout << "WM_SIZE: " << w << ":" << h << " - " << wParam << '\n';

        if (_messageWidth == w && _messageHeight == h)
            break;

        _messageWidth = w;
        _messageHeight = h;

        WindowSizeMessage message = {};
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

        // Disable not-locked mutex warning
#pragma warning( push )
#pragma warning( disable : 26117 26110 )
        if (!_insideInitialShowWindowCall)
            _m_windowSize.unlock();
        return DefWindowProc(hWnd, msg, wParam, lParam);
#pragma warning( pop )
        break;
    }
    case WM_KEYDOWN:
    {
        KeyDownMessage message = {};
        message.keyCode = (uint8_t)wParam;
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
        KeyUpMessage message = {};
        message.keyCode = (uint8_t)wParam;
        message.scanCode = (uint8_t)((lParam >> 16) & 0xFF);
        message.isExtended = (lParam & KF_EXTENDED) == KF_ALTDOWN;

        _m_msg.lock();
        _msgQueue.push(message.Encode());
        _m_msg.unlock();
        break;
    }
    case WM_SYSKEYDOWN:
    {
        KeyDownMessage message = {};
        message.keyCode = (uint8_t)wParam;
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
            message.keyCode = (uint8_t)wParam;
            _msgQueue.push(message.Encode());
            _m_msg.unlock();
        }

        break;
    }
    case WM_SYSKEYUP:
    {
        KeyUpMessage message = {};
        message.keyCode = (uint8_t)wParam;
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
            message.keyCode = (uint8_t)wParam;
            _msgQueue.push(message.Encode());
            _m_msg.unlock();
        }

        break;
    }
    case WM_CHAR:
    {
        CharMessage message = {};
        message.character = (wchar_t)wParam;
        message.repeatCount = (uint16_t)(lParam & 0xFFFF);
        message.scanCode = (uint8_t)((lParam >> 16) & 0xFF);

        _m_msg.lock();
        _msgQueue.push(message.Encode());
        _m_msg.unlock();
        break;
    }
    case WM_APP_SET_FULLSCREEN_STATE:
    {
        HandleFullscreenChange(wParam);
        break;
    }
    case WM_APP_SET_CURSOR_VISIBILITY:
    {
        HandleCursorVisibilityChange(wParam);
        break;
    }
    case WM_APP_SET_WINDOW_DISPLAY:
    {
        WindowDisplayType displayType = (WindowDisplayType)wParam;

        int showFlag = SW_SHOWNORMAL;
        switch (displayType)
        {
        case zwnd::WindowDisplayType::NORMAL: { showFlag = SW_SHOWNORMAL; break; }
        case zwnd::WindowDisplayType::NORMAL_NOACTIVATE: { showFlag = SW_SHOWNOACTIVATE; break; }
        case zwnd::WindowDisplayType::MINIMIZED: { showFlag = SW_SHOWMINIMIZED; break; }
        case zwnd::WindowDisplayType::MAXIMIZED: { showFlag = SW_SHOWMAXIMIZED; break; }
        case zwnd::WindowDisplayType::HIDDEN: { showFlag = SW_HIDE; break; }
        }

        if (!_initialShowWindowCallDone)
            _insideInitialShowWindowCall = true;

        ShowWindow(_hwnd, showFlag);

        if (!_initialShowWindowCallDone)
        {
            _insideInitialShowWindowCall = false;
            _initialShowWindowCallDone = true;
        }
        break;
    }
    case WM_APP_SET_WINDOW_RECT:
    {
        RECT* rect = (RECT*)lParam;
        if (rect)
        {
            SetWindowPos(_hwnd, 0, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, SWP_NOZORDER | (_activationDisabled ? SWP_NOACTIVATE : NULL));
            delete rect;
        }
        break;
    }
    case WM_APP_SET_CURSOR_ICON:
    {
        CursorIcon cursor = (CursorIcon)wParam;
        switch (cursor)
        {
        case CursorIcon::APP_STARTING: { _cursor = LoadCursor(NULL, IDC_APPSTARTING); break; }
        case CursorIcon::ARROW: { _cursor = LoadCursor(NULL, IDC_ARROW); break; }
        case CursorIcon::CROSS: { _cursor = LoadCursor(NULL, IDC_CROSS); break; }
        case CursorIcon::HAND: { _cursor = LoadCursor(NULL, IDC_HAND); break; }
        case CursorIcon::HELP: { _cursor = LoadCursor(NULL, IDC_HELP); break; }
        case CursorIcon::IBEAM: { _cursor = LoadCursor(NULL, IDC_IBEAM); break; }
        case CursorIcon::NO: { _cursor = LoadCursor(NULL, IDC_NO); break; }
        case CursorIcon::SIZE_ALL: { _cursor = LoadCursor(NULL, IDC_SIZEALL); break; }
        case CursorIcon::SIZE_NESW: { _cursor = LoadCursor(NULL, IDC_SIZENESW); break; }
        case CursorIcon::SIZE_NS: { _cursor = LoadCursor(NULL, IDC_SIZENS); break; }
        case CursorIcon::SIZE_NWSE: { _cursor = LoadCursor(NULL, IDC_SIZENWSE); break; }
        case CursorIcon::SIZE_WE: { _cursor = LoadCursor(NULL, IDC_SIZEWE); break; }
        case CursorIcon::UP_ARROW: { _cursor = LoadCursor(NULL, IDC_UPARROW); break; }
        case CursorIcon::WAIT: { _cursor = LoadCursor(NULL, IDC_WAIT); break; }
        default: break;
        }

        if (_cursorInClientArea)
            SetCursor(_cursor);
        break;
    }
    case WM_APP_SET_FOCUS:
    {
        SetFocus(_hwnd);
        break;
    }
    case WM_APP_SET_WINDOW_INTERACTION:
    {
        MouseWindowInteraction interactionType = (MouseWindowInteraction)wParam;
        if (interactionType == MouseWindowInteraction::DEFAULT)
        {
            LONG_PTR currentStyle = GetWindowLongPtr(_hwnd, GWL_EXSTYLE);
            if (currentStyle & WS_EX_TRANSPARENT)
            {
                SetWindowLongPtr(_hwnd, GWL_EXSTYLE, currentStyle & ~WS_EX_TRANSPARENT);
                SetWindowPos(_hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
            }
        }
        else if (interactionType == MouseWindowInteraction::PASS_THROUGH)
        {
            LONG_PTR currentStyle = GetWindowLongPtr(_hwnd, GWL_EXSTYLE);
            if (!(currentStyle & WS_EX_TRANSPARENT))
            {
                SetWindowLongPtr(_hwnd, GWL_EXSTYLE, currentStyle | WS_EX_TRANSPARENT);
                SetWindowPos(_hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
            }
        }
        break;
    }
    default:
    {
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    }

    return 0;
}

void zwnd::WindowBackend::HandleFullscreenChange(bool fullscreen)
{
    if (_fullscreenInternal == fullscreen)
        return;
    _fullscreenInternal = fullscreen;

    if (_fullscreenInternal)
    {
        WINDOWPLACEMENT placement = {};
        placement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(_hwnd, &placement);
        _windowedMaximized = (placement.showCmd == SW_SHOWMAXIMIZED);

        HMONITOR hMonitor = MonitorFromWindow(_hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info = {};
        info.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(hMonitor, &info);
        RECT monitor = info.rcMonitor;
        int w = monitor.right - monitor.left;
        int h = monitor.bottom - monitor.top;

        // TODO: perhaps hiding the window, changing the style and showing it again would work

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

void zwnd::WindowBackend::SetCursorIcon(CursorIcon cursor)
{
    PostMessage(_hwnd, WM_APP_SET_CURSOR_ICON, (int)cursor, NULL);
}

void zwnd::WindowBackend::SetCursorVisibility(bool visible)
{
    PostMessage(_hwnd, WM_APP_SET_CURSOR_VISIBILITY, visible, NULL);
}

void zwnd::WindowBackend::HandleCursorVisibilityChange(bool visible)
{
    if (_cursorVisible == visible)
        return;
    _cursorVisible = visible;

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

void zwnd::WindowBackend::ResetScreenTimer()
{
    SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
}

void zwnd::WindowBackend::SetMouseInteraction(MouseWindowInteraction interactionType)
{
    PostMessage(_hwnd, WM_APP_SET_WINDOW_INTERACTION, (int)interactionType, NULL);
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
    RECT* rectPtr = new RECT();
    *rectPtr = rect;
    PostMessage(_hwnd, WM_APP_SET_WINDOW_RECT, NULL, (LPARAM)rectPtr);
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
    SetWindowPos(_hwnd, 0, 0, 0, newWidth, newHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_ASYNCWINDOWPOS | (_activationDisabled ? SWP_NOACTIVATE : NULL));
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
    SetWindowPos(_hwnd, 0, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_ASYNCWINDOWPOS | (_activationDisabled ? SWP_NOACTIVATE : NULL));
}

bool zwnd::WindowBackend::Maximized()
{
    WINDOWPLACEMENT placement = {};
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
    WINDOWPLACEMENT placement = {};
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

void zwnd::WindowBackend::Focus()
{
    PostMessage(_hwnd, WM_APP_SET_FOCUS, NULL, NULL);
}

void zwnd::WindowBackend::SetDisplayType(WindowDisplayType displayType)
{
    PostMessage(_hwnd, WM_APP_SET_WINDOW_DISPLAY, (int)displayType, NULL);
}

RECT zwnd::WindowBackend::GetMonitorRectAtScreenPoint(int x, int y)
{
    HMONITOR hmon = MonitorFromPoint({ x, y }, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    GetMonitorInfo(hmon, &info);
    return info.rcMonitor;
}

RECT zwnd::WindowBackend::GetMonitorRectAtWindowPoint(int x, int y)
{
    RECT windowRect = GetWindowRectangle();
    return GetMonitorRectAtScreenPoint(windowRect.left + x, windowRect.top + y);
}

void zwnd::WindowBackend::SetFullscreen(bool fullscreen)
{
    _fullscreen.store(fullscreen);
    PostMessage(_hwnd, WM_APP_SET_FULLSCREEN_STATE, fullscreen, NULL);
}

bool zwnd::WindowBackend::IsFullscreen()
{
    return _fullscreen.load();
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

void zwnd::WindowBackend::SetCaptionHeight(int height)
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