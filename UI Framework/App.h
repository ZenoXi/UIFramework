#pragma once

#include "Window/DisplayWindow.h"
#include "Scenes/Scene.h"
#include "Window/MouseManager.h"
#include "Window/KeyboardManager.h"

#include <thread>
#include <mutex>
#include <deque>

class OverlayScene;

// Singleton
class App
{
public:
    DisplayWindow& window;
    MouseManager mouseManager;
    KeyboardManager keyboardManager;

private: // Singleton interface
    App(DisplayWindow& dw);
    static App* _instance;
    static bool _exited;
public:
    static void Init(DisplayWindow& dw);
    static App* Instance();
    static void Start();
    static bool Exited();

private: // Scene control
    std::vector<Scene*> _scenes;
    int _currentSceneIndex = -1;
    std::vector<Scene*> _activeScenes;
    std::deque<std::string> _scenesToUninitialize;
    bool _fullscreen = false;
public:
    Scene* CurrentScene();
    bool Fullscreen() const { return _fullscreen; }
    void Fullscreen(bool fullscreen);
    // Initializes the scene and places it behind all scenes, unfocused (unless no scenes are initialized)
    bool InitScene(std::string name, SceneOptionsBase* options);
    // Uninitializes and immediatelly initializes the scene with new options, keeping focus/z-order the same
    // If scene is not initialized, it just gets initialized as usual
    bool ReinitScene(std::string name, SceneOptionsBase* options);
    // Primes the scene to be uninitialized
    void UninitScene(std::string name);
private:
    void _UninitScene(std::string name);
public:
    bool MoveSceneToFront(std::string name);
    bool MoveSceneToBack(std::string name);
    bool MoveSceneUp(std::string name);
    bool MoveSceneDown(std::string name);
    // If the scene is already behind, it isn't moved
    bool MoveSceneBehind(std::string name, std::string behind);
    // If the scene is already in front, it isn't moved
    bool MoveSceneInFront(std::string name, std::string inFront);
    std::vector<Scene*> ActiveScenes();
    Scene* FindScene(std::string name);
    Scene* FindActiveScene(std::string name);
private:
    int FindSceneIndex(std::string name);
    int FindActiveSceneIndex(std::string name);

private: // Main app thread
    std::thread _mainThread;
    std::mutex _m_main;
    bool _sceneChanged = false;
public:
    void LoopThread();
};