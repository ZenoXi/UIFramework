#pragma once

#include "Scene.h"

#include "Components/Base/ComponentBase.h"
#include "Components/Base/Label.h"
#include "Components/Base/Button.h"
#include "Components/Base/Dummy.h"
#include "Components/Custom/DitheredBackground.h"

namespace zcom
{
    struct EntrySceneOptions : public SceneOptionsBase
    {

    };

    class EntryScene : public Scene
    {
        std::unique_ptr<DitheredBackground> _background = nullptr;
        std::unique_ptr<Label> _helloWorldLabel = nullptr;
        std::unique_ptr<Dummy> _testPanel = nullptr;
        std::unique_ptr<Button> _button = nullptr;
        std::unique_ptr<Button> _button2 = nullptr;

    public:
        EntryScene(App* app, zwnd::Window* window);

        const char* GetName() const { return "entry"; }
        static const char* StaticName() { return "entry"; }

    private:
        void _Init(SceneOptionsBase* options);
        void _Uninit();
        void _Focus();
        void _Unfocus();
        void _Update();
        void _Resize(int width, int height, ResizeInfo info);
    };
}
