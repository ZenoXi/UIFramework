#pragma once

#include "Scene.h"

#include "Components/Base/ComponentBase.h"
#include "Components/Base/Button.h"
#include "Components/Base/Label.h"
#include "Components/Base/Image.h"

namespace zcom
{
    struct DefaultTitleBarSceneOptions : public SceneOptionsBase
    {
        std::wstring windowTitle = L"UI Framework";
    };

    class DefaultTitleBarScene : public Scene
    {
    public:
        void SetBackground(D2D1_COLOR_F color);
        void AddCloseButton();
        void AddMaximizeButton();
        void AddMinimizeButton();
        void AddIcon(ID2D1Bitmap* icon);
        void AddTitle(std::wstring title);
        void AddMenuButton(std::wstring name);

        virtual int TitleBarHeight();
        // Returns the RECT (in title bar scene coordinates) which can be considered as the window menu button
        virtual RECT WindowMenuButtonRect();
        // Returns a list of rects (in title bar scene coordinates) which should not be considered as caption area
        virtual std::vector<RECT> ExcludedCaptionRects();

    public:
        DefaultTitleBarScene(App* app, zwnd::Window* window);

    protected:
        std::unique_ptr<Button> _closeButton = nullptr;
        std::unique_ptr<Button> _maximizeButton = nullptr;
        std::unique_ptr<Button> _minimizeButton = nullptr;
        std::unique_ptr<Image> _iconImage = nullptr;
        std::unique_ptr<Label> _titleLabel = nullptr;
        std::vector<std::unique_ptr<Button>> _menuButtons;

    private:
        void _Init(const SceneOptionsBase* options);
        void _Uninit();
        void _Focus();
        void _Unfocus();
        void _Update();
        void _Resize(int width, int height, ResizeInfo info);
    public:
        const char* GetName() const { return StaticName(); }
        static const char* StaticName() { return "title_bar"; }
    };
}
