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
        DEFINE_SCENE(EntryScene, Scene)
    protected:
        void Init(SceneOptionsBase* options) override;
    private:
        std::unique_ptr<DitheredBackground> _background = nullptr;
        std::unique_ptr<Label> _helloWorldLabel = nullptr;
        std::unique_ptr<Dummy> _testPanel = nullptr;
        std::unique_ptr<Button> _button = nullptr;
        std::unique_ptr<Button> _button2 = nullptr;
    };
}
