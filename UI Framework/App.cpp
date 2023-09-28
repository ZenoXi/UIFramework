#include "App.h"

App::App(HINSTANCE hinst) : _hinst(hinst)
{
    _windowCleaningThread = std::thread(&App::_RemoveUnusedWindows, this);
}

App::~App()
{
    _closeThread.store(true);
    if (_windowCleaningThread.joinable())
        _windowCleaningThread.join();
}

std::optional<zwnd::WindowId> App::CreateTopWindow(zwnd::WindowProperties props, std::function<void(zwnd::Window* window)> initFunction)
{
    std::lock_guard<std::mutex> lock(_m_windows);
    _windows.push_back({
        std::make_unique<zwnd::Window>(this, zwnd::WindowType::TOP, std::nullopt, props, _hinst, initFunction, [&](zwnd::Window* window) {
            std::lock_guard<std::mutex> lock(_m_windows);
            _TryDestruct(window->GetWindowId());
        })
    });
    return _windows.back().window->GetWindowId();
}

std::optional<zwnd::WindowId> App::CreateChildWindow(zwnd::WindowId parentWindowId, zwnd::WindowProperties props, std::function<void(zwnd::Window* window)> initFunction)
{
    std::lock_guard<std::mutex> lock(_m_windows);

    zwnd::Window* parentWindow = _FindWindow(parentWindowId);
    if (parentWindow == nullptr)
        return std::nullopt;

    // Check if a blocking child already exists
    if (props.blockParent)
    {
        for (auto& window : _windows)
        {
            if (window.window->GetWindowType() != zwnd::WindowType::CHILD)
                continue;
            if (window.window->GetParent().value() == parentWindowId)
                return std::nullopt;
        }
    }

    _windows.push_back({
        std::make_unique<zwnd::Window>(this, zwnd::WindowType::CHILD, std::optional(parentWindowId), props, _hinst, initFunction, [&](zwnd::Window* window) {
            std::lock_guard<std::mutex> lock(_m_windows);
            _TryDestruct(window->GetWindowId());
            zwnd::Window* parentWindow = _FindWindow(parentWindowId);
            //if (parentWindow)
            //    parentWindow->ResetBlockingWindow();
        })
    });

    zwnd::WindowId windowId = _windows.back().window->GetWindowId();
    //parentWindow->SetBlockingWindow(windowId);
    return windowId;
}

std::optional<zwnd::WindowId> App::CreateToolWindow(zwnd::WindowId parentWindowId, zwnd::WindowProperties props, std::function<void(zwnd::Window* window)> initFunction)
{
    std::lock_guard<std::mutex> lock(_m_windows);

    zwnd::Window* parentWindow = _FindWindow(parentWindowId);
    if (parentWindow == nullptr)
        return std::nullopt;
    if (parentWindow->GetWindowType() != zwnd::WindowType::TOP && parentWindow->GetWindowType() != zwnd::WindowType::CHILD)
        return std::nullopt;

    _windows.push_back({
        std::make_unique<zwnd::Window>(this, zwnd::WindowType::TOOL, std::optional(parentWindowId), props, _hinst, initFunction, [&](zwnd::Window* window) {
            std::lock_guard<std::mutex> lock(_m_windows);
            _TryDestruct(window->GetWindowId());
        })
    });
    return _windows.back().window->GetWindowId();
}

Handle<zwnd::Window> App::GetWindow(zwnd::WindowId windowId)
{
    std::lock_guard<std::mutex> lock(_m_windows);
    for (auto& window : _windows)
    {
        if (window.window->GetWindowId() == windowId && !window.window->Closed())
        {
            window.handleCount++;
            zwnd::WindowId id = window.window->GetWindowId();
            return Handle<zwnd::Window>(window.window.get(), [&, id]() {
                std::lock_guard<std::mutex> lock(_m_windows);
                _ReleaseHandle(id);
            });
        }
    }
    return Handle<zwnd::Window>(nullptr, [&]() {});
}

Handle<zwnd::Window> App::GetWindowNoLock(zwnd::WindowId windowId)
{
    for (auto& window : _windows)
    {
        if (window.window->GetWindowId() == windowId && !window.window->Closed())
        {
            window.handleCount++;
            zwnd::WindowId id = window.window->GetWindowId();
            return Handle<zwnd::Window>(window.window.get(), [&, id]() {
                _ReleaseHandle(id);
            });
        }
    }
    return Handle<zwnd::Window>(nullptr, [&]() {});
}

bool App::WindowsClosed()
{
    std::lock_guard<std::mutex> lock(_m_windows);
    return _windows.empty();
}

zwnd::Window* App::_FindWindow(zwnd::WindowId windowId)
{
    for (auto& window : _windows)
    {
        if (window.window->GetWindowId() == windowId)
        {
            return window.window.get();
        }
    }
    return nullptr;
}

void App::_ReleaseHandle(zwnd::WindowId windowId)
{
    for (auto& window : _windows)
    {
        if (window.window->GetWindowId() == windowId)
        {
            window.handleCount--;
            _TryDestruct(windowId);
            return;
        }
    }
}

void App::_TryDestruct(zwnd::WindowId windowId)
{
    for (auto it = _windows.begin(); it != _windows.end(); it++)
    {
        if (it->markedForDeleting)
            continue;
        if (it->window->GetWindowId() != windowId)
            continue;
        if (!it->window->Closed())
            return;

        if (it->window->GetWindowType() == zwnd::WindowType::TOP)
        {
            // Close child and tool windows
            bool childWindowsExist = false;
            for (auto& window : _windows)
            {
                if (window.markedForDeleting)
                    continue;
                if (window.window->GetWindowType() != zwnd::WindowType::CHILD && window.window->GetWindowType() != zwnd::WindowType::TOOL)
                    continue;
                if (window.window->GetParent().value() != windowId)
                    continue;
                childWindowsExist = true;
                if (window.window->Closing())
                    continue;

                window.window->Close();
            }
            // Defer window closing until after all child windows are closed
            if (childWindowsExist)
                return;

            if (it->window->Properties().mainWindow)
            {
                // Close all remaining top-level windows
                for (auto& window : _windows)
                {
                    if (window.markedForDeleting)
                        continue;
                    if (window.window->GetWindowType() != zwnd::WindowType::TOP)
                        continue;
                    if (window.window->GetWindowId() == windowId)
                        continue;
                    if (window.window->Closing())
                        continue;

                    window.window->Close();
                }
            }

            it->markedForDeleting = true;
        }
        else if (it->window->GetWindowType() == zwnd::WindowType::CHILD)
        {
            // Close child and tool windows
            bool childWindowsExist = false;
            for (auto& window : _windows)
            {
                if (window.markedForDeleting)
                    continue;
                if (window.window->GetWindowType() != zwnd::WindowType::CHILD && window.window->GetWindowType() != zwnd::WindowType::TOOL)
                    continue;
                if (window.window->GetParent().value() != windowId)
                    continue;
                childWindowsExist = true;
                if (window.window->Closing())
                    continue;

                window.window->Close();
            }
            // Defer window closing until after all child windows are closed
            if (childWindowsExist)
                return;

            it->markedForDeleting = true;
            
            // Invoke parent destruction check
            _TryDestruct(it->window->GetParent().value());
        }
        else if (it->window->GetWindowType() == zwnd::WindowType::TOOL)
        {

        }

        return;
    }
}

void App::_RemoveUnusedWindows()
{
    while (!_closeThread.load())
    {
        std::unique_lock<std::mutex> lock(_m_windows);
        _windows.erase(
            std::remove_if(
                _windows.begin(),
                _windows.end(),
                [](const WindowInfo& info) { return info.markedForDeleting && info.handleCount <= 0; }
            ),
            _windows.end()
        );
        lock.unlock();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
