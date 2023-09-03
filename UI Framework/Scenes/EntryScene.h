#pragma once

#include "Scene.h"

#include "Components/Base/ComponentBase.h"
#include "Components/Base/Label.h"
#include "Components/Base/EmptyPanel.h"
#include "Components/Custom/DitheredBackground.h"

struct EntrySceneOptions : public SceneOptionsBase
{

};

class EntryScene : public Scene
{
    std::unique_ptr<zcom::DitheredBackground> _background = nullptr;
    std::unique_ptr<zcom::Label> _helloWorldLabel = nullptr;
    std::unique_ptr<zcom::EmptyPanel> _testPanel = nullptr;

public:
    EntryScene(App* app, zwnd::Window* window);

    const char* GetName() const { return "entry"; }
    static const char* StaticName() { return "entry"; }

private:
    void _Init(const SceneOptionsBase* options);
    void _Uninit();
    void _Focus();
    void _Unfocus();
    void _Update();
    void _Resize(int width, int height);
};
