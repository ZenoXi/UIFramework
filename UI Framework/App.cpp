#include "App.h"

#include "Scenes/EntryScene.h"

#include "Helper/Time.h"

App* App::_instance = nullptr;
bool App::_exited = false;

App::App(DisplayWindow& dw)
  : window(dw)
{
    dw.AddMouseHandler(&mouseManager);
    dw.AddKeyboardHandler(&keyboardManager);
}

void App::Init(DisplayWindow& dw)
{
    if (!_instance)
    {
        _instance = new App(dw);
    }

    // Add scenes
    Instance()->_scenes.push_back(new zcom::EntryScene(_instance, nullptr));
}

void App::Start()
{
    _exited = false;

    // Start main loop
    Instance()->_mainThread = std::thread(&App::LoopThread, Instance());
}

bool App::Exited()
{
    return _exited;
}

App* App::Instance()
{
    return _instance;
}

zcom::Scene* App::CurrentScene()
{
    if (_currentSceneIndex == -1)
    {
        return nullptr;
    }
    return _scenes.at(_currentSceneIndex);
}

void App::Fullscreen(bool fullscreen)
{
    if (fullscreen == _fullscreen)
        return;

    _fullscreen = fullscreen;
    if (_fullscreen)
    {
        mouseManager.SetTopMenuVisibility(false);
        window.SetFullscreen(true);
        _sceneChanged = true;
    }
    else
    {
        mouseManager.SetTopMenuVisibility(true);
        window.SetFullscreen(false);
        _sceneChanged = true;
    }

}

bool App::InitScene(std::string name, zcom::SceneOptionsBase* options)
{
    zcom::Scene* scene = FindScene(name);
    if (!scene) return false;
    if (FindActiveScene(name)) return true;

    scene->Init(options);
    _activeScenes.insert(_activeScenes.begin(), scene);
    if (_activeScenes.size() == 1)
    {
        _activeScenes.back()->Focus();
    }
    _sceneChanged = true;
    return true;
}

bool App::ReinitScene(std::string name, zcom::SceneOptionsBase* options)
{
    zcom::Scene* scene = FindActiveScene(name);
    if (scene)
    {
        // Remove scene from uninit pending
        auto it = std::find(_scenesToUninitialize.begin(), _scenesToUninitialize.end(), name);
        if (it != _scenesToUninitialize.end())
            _scenesToUninitialize.erase(it);

        bool focused = scene->Focused();
        scene->Uninit();
        scene->Init(options);
        if (focused)
            scene->Focus();
        _sceneChanged = true;
        return true;
    }
    else
    {
        return InitScene(name, options);
    }
}

void App::UninitScene(std::string name)
{
    _scenesToUninitialize.push_back(name);
}

void App::_UninitScene(std::string name)
{
    zcom::Scene* scene = FindActiveScene(name);
    if (!scene)
        return;

    bool newFocus = false;
    if (_activeScenes.back() == scene)
        newFocus = true;

    scene->Uninit();
    _activeScenes.erase(std::find(_activeScenes.begin(), _activeScenes.end(), scene));
    if (newFocus && !_activeScenes.empty())
        _activeScenes.back()->Focus();
    _sceneChanged = true;
}

bool App::MoveSceneToFront(std::string name)
{
    int index = FindActiveSceneIndex(name);
    if (index == -1) return false;
    if (index == _activeScenes.size() - 1) return true;

    _activeScenes.back()->Unfocus();
    _activeScenes.push_back(_activeScenes[index]);
    _activeScenes.erase(_activeScenes.begin() + index);
    _activeScenes.back()->Focus();
    _sceneChanged = true;
    return true;
}

bool App::MoveSceneToBack(std::string name)
{
    int index = FindActiveSceneIndex(name);
    if (index == -1) return false;
    if (index == 0) return true;

    zcom::Scene* scene = _activeScenes[index];
    _activeScenes.erase(_activeScenes.begin() + index);
    _activeScenes.insert(_activeScenes.begin(), scene);
    if (index == _activeScenes.size() - 1)
    {
        scene->Unfocus();
        _activeScenes.back()->Focus();
    }
    _sceneChanged = true;
    return true;
}

bool App::MoveSceneUp(std::string name)
{
    int index = FindActiveSceneIndex(name);
    if (index == -1) return false;
    if (index == _activeScenes.size() - 1) return true;

    if (index == _activeScenes.size() - 2)
    {
        _activeScenes[index]->Focus();
        _activeScenes[index + 1]->Unfocus();
    }
    std::swap(_activeScenes[index], _activeScenes[index + 1]);
    _sceneChanged = true;
    return true;
}

bool App::MoveSceneDown(std::string name)
{
    int index = FindActiveSceneIndex(name);
    if (index == -1) return false;
    if (index == 0) return true;

    if (index == _activeScenes.size() - 1)
    {
        _activeScenes[index]->Unfocus();
        _activeScenes[index + -1]->Focus();
    }
    std::swap(_activeScenes[index], _activeScenes[index - 1]);
    _sceneChanged = true;
    return true;
}

bool App::MoveSceneBehind(std::string name, std::string behind)
{
    int sceneIndex = FindActiveSceneIndex(name);
    if (sceneIndex == -1) return false;
    int inFrontSceneIndex = FindActiveSceneIndex(behind);
    if (inFrontSceneIndex == -1) return false;
    if (sceneIndex < inFrontSceneIndex) return true;

    zcom::Scene* scene = _activeScenes[sceneIndex];
    _activeScenes.erase(_activeScenes.begin() + sceneIndex);
    _activeScenes.insert(_activeScenes.begin() + inFrontSceneIndex, scene);
    if (sceneIndex == _activeScenes.size() - 1)
    {
        scene->Unfocus();
        _activeScenes.back()->Focus();
    }
    _sceneChanged = true;
    return true;
}

bool App::MoveSceneInFront(std::string name, std::string inFront)
{
    int sceneIndex = FindActiveSceneIndex(name);
    if (sceneIndex == -1) return false;
    int behindSceneIndex = FindActiveSceneIndex(inFront);
    if (behindSceneIndex == -1) return false;
    if (sceneIndex > behindSceneIndex) return true;

    _activeScenes.insert(_activeScenes.begin() + behindSceneIndex + 1, _activeScenes[sceneIndex]);
    _activeScenes.erase(_activeScenes.begin() + sceneIndex);
    if (behindSceneIndex == _activeScenes.size() - 1)
    {
        _activeScenes[behindSceneIndex - 1]->Unfocus();
        _activeScenes.back()->Focus();
    }
    _sceneChanged = true;
    return true;
}

std::vector<zcom::Scene*> App::ActiveScenes()
{
    return _activeScenes;
}

zcom::Scene* App::FindScene(std::string name)
{
    for (auto scene : _scenes)
    {
        if (scene->GetName() == name)
        {
            return scene;
        }
    }
    return nullptr;
}

zcom::Scene* App::FindActiveScene(std::string name)
{
    for (auto scene : _activeScenes)
    {
        if (scene->GetName() == name)
        {
            return scene;
        }
    }
    return nullptr;
}

int App::FindSceneIndex(std::string name)
{
    for (int i = 0; i < _scenes.size(); i++)
    {
        if (_scenes[i]->GetName() == name)
        {
            return i;
        }
    }
    return -1;
}

int App::FindActiveSceneIndex(std::string name)
{
    for (int i = 0; i < _activeScenes.size(); i++)
    {
        if (_activeScenes[i]->GetName() == name)
        {
            return i;
        }
    }
    return -1;
}

void App::LoopThread()
{
    int framecounter = 0;
    Clock frameTimer = Clock(0);

    while (true)
    {
        window.ProcessQueueMessages();
        WindowMessage wmMove = window.GetMoveResult();
        WindowMessage wmSize = window.GetSizeResult();
        WindowMessage wmExit = window.GetExitResult();

        // Check for exit message
        if (!wmExit.handled)
        {
            // Uninit all scenes
            for (auto& scene : _activeScenes)
                scene->Uninit();

            _exited = true;
            break;
        }

        ztime::clock[CLOCK_GAME].Update();
        ztime::clock[CLOCK_MAIN].Update();

        // Render frame
        window.gfx.Lock();
        window.gfx.BeginFrame();

        // Resize UI
        // This part is after "Render frame" because window.gfx.Lock() needs to be called prior
        // to resizing the scene
        if (!wmSize.handled)
        {
            int w = LOWORD(wmSize.lParam);
            int h = HIWORD(wmSize.lParam);

            auto activeScenes = ActiveScenes();
            for (auto& scene : activeScenes)
                scene->Resize(w, h);
        }

        bool redraw = false;
        if (_sceneChanged)
        {
            _sceneChanged = false;
            redraw = true;
            window.gfx.GetGraphics().target->BeginDraw();
            window.gfx.GetGraphics().target->Clear(D2D1::ColorF(0));
        }

        auto activeScenes = ActiveScenes();
        // Update scenes
        for (auto& scene : activeScenes)
        {
            scene->Update();
        }
        // Draw scenes
        for (auto& scene : activeScenes)
        {
            if (scene->Redraw())
            {
                if (!redraw)
                    window.gfx.GetGraphics().target->BeginDraw();

                scene->Draw(window.gfx.GetGraphics());
                redraw = true;
            }
        }
        if (redraw)
        {
            std::cout << "Redrawn (" << framecounter++ << ")\n";
            window.gfx.GetGraphics().target->Clear(D2D1::ColorF(0.3f, 0.3f, 0.3f));
            for (auto& scene : activeScenes)
            {
                window.gfx.GetGraphics().target->DrawBitmap(scene->ContentImage());
            }
            window.gfx.GetGraphics().target->EndDraw();
        }

        // Uninit scenes
        while (!_scenesToUninitialize.empty())
        {
            _UninitScene(_scenesToUninitialize.front());
            _scenesToUninitialize.pop_front();
        }

        window.gfx.EndFrame(redraw);
        window.gfx.Unlock();

        // Prevent deadlock from extremely short unlock-lock cycle
        // (it doesn't make sense to me either but for some reason
        // the mutexes aren't locked in order of 'Lock()' calls)
        Clock sleepTimer = Clock(0);
        do sleepTimer.Update();
        while (sleepTimer.Now().GetTime(MICROSECONDS) < 10);
    }
}
