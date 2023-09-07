#include "App.h" // App.h and Window.h must be included first
#include "Window/Window.h"
#include "DefaultTitleBarScene.h"

#include "Helper/ResourceManager.h";

DefaultTitleBarScene::DefaultTitleBarScene(App* app, zwnd::Window* window)
    : Scene(app, window)
{}

void DefaultTitleBarScene::_Init(const SceneOptionsBase* options)
{
    DefaultTitleBarSceneOptions opt;
    if (options)
        opt = *reinterpret_cast<const DefaultTitleBarSceneOptions*>(options);

    // The following functions set up the default title bar look
    // See the function implementations for details on achieving
    // the default look

    SetBackground(D2D1::ColorF(1.0f, 1.0f, 1.0f));
    AddCloseButton();
    AddMaximizeButton();
    AddMinimizeButton();
    AddIcon(_window->resourceManager.GetImage("window_app_icon"));
    AddTitle(opt.windowTitle);
    AddMenuButton(L"File");
    AddMenuButton(L"Edit");
    AddMenuButton(L"View");
    // After the 'Add*Item*()' calls, the default item appearance and behavior can be modified through their variables
}


void DefaultTitleBarScene::SetBackground(D2D1_COLOR_F color)
{
    _canvas->SetBackgroundColor(color);
}

void DefaultTitleBarScene::AddCloseButton()
{
    _closeButton = Create<zcom::Button>();
    _closeButton->SetBaseSize(45, 29);
    _closeButton->SetHorizontalAlignment(zcom::Alignment::END);
    _closeButton->SetButtonImageAll(_window->resourceManager.GetImage("window_close"));
    _closeButton->ButtonImage()->SetPlacement(zcom::ImagePlacement::CENTER);
    _closeButton->ButtonImage()->SetPixelSnap(true);
    _closeButton->UseImageParamsForAll(_closeButton->ButtonImage());
    _closeButton->SetButtonColor(D2D1::ColorF(0, 0.0f));
    _closeButton->SetButtonHoverColor(D2D1::ColorF(/* #E81123 */ 15208739));
    _closeButton->SetButtonClickColor(D2D1::ColorF(/* #E81123 */ 15208739, 0.54f));
    _closeButton->ButtonImage()->SetTintColor(D2D1::ColorF(0));
    _closeButton->ButtonHoverImage()->SetTintColor(D2D1::ColorF(1.0f, 1.0f, 1.0f));
    _closeButton->ButtonClickImage()->SetTintColor(D2D1::ColorF(1.0f, 1.0f, 1.0f));

    _canvas->AddComponent(_closeButton.get());
}

void DefaultTitleBarScene::AddMaximizeButton()
{
    _maximizeButton = Create<zcom::Button>();
    _maximizeButton->SetBaseSize(45, 29);
    _maximizeButton->SetHorizontalAlignment(zcom::Alignment::END);
    if (_closeButton)
        _maximizeButton->SetHorizontalOffsetPixels(-45);
    _maximizeButton->SetButtonImageAll(_window->resourceManager.GetImage("window_maximize"));
    _maximizeButton->ButtonImage()->SetPlacement(zcom::ImagePlacement::CENTER);
    _maximizeButton->ButtonImage()->SetPixelSnap(true);
    _maximizeButton->ButtonImage()->SetTintColor(D2D1::ColorF(0));
    _maximizeButton->UseImageParamsForAll(_maximizeButton->ButtonImage());
    _maximizeButton->SetButtonColor(D2D1::ColorF(0, 0.0f));
    _maximizeButton->SetButtonHoverColor(D2D1::ColorF(0, 0.1f));
    _maximizeButton->SetButtonClickColor(D2D1::ColorF(0, 0.2f));

    _canvas->AddComponent(_maximizeButton.get());
}

void DefaultTitleBarScene::AddMinimizeButton()
{
    _minimizeButton = Create<zcom::Button>();
    _minimizeButton->SetBaseSize(45, 29);
    _minimizeButton->SetHorizontalAlignment(zcom::Alignment::END);
    if (_closeButton && _maximizeButton)
        _minimizeButton->SetHorizontalOffsetPixels(-90);
    else if (_closeButton || _maximizeButton)
        _minimizeButton->SetHorizontalOffsetPixels(-45);
    _minimizeButton->SetButtonImageAll(_window->resourceManager.GetImage("window_minimize"));
    _minimizeButton->ButtonImage()->SetPlacement(zcom::ImagePlacement::CENTER);
    _minimizeButton->ButtonImage()->SetPixelSnap(true);
    _minimizeButton->ButtonImage()->SetTintColor(D2D1::ColorF(0));
    _minimizeButton->UseImageParamsForAll(_minimizeButton->ButtonImage());
    _minimizeButton->SetButtonColor(D2D1::ColorF(0, 0.0f));
    _minimizeButton->SetButtonHoverColor(D2D1::ColorF(0, 0.1f));
    _minimizeButton->SetButtonClickColor(D2D1::ColorF(0, 0.2f));

    _canvas->AddComponent(_minimizeButton.get());
}

void DefaultTitleBarScene::AddIcon(ID2D1Bitmap* icon)
{
    _iconImage = Create<zcom::Image>(_window->resourceManager.GetImage("window_app_icon"));
    _iconImage->SetBaseSize(29, 29);
    _iconImage->SetPlacement(zcom::ImagePlacement::CENTER);
    _iconImage->SetPixelSnap(true);
    _iconImage->SetTintColor(D2D1::ColorF(0));

    _canvas->AddComponent(_iconImage.get());
}

void DefaultTitleBarScene::AddTitle(std::wstring title)
{
    _titleLabel = Create<zcom::Label>(title);
    _titleLabel->SetFont(L"Segoe UI");
    _titleLabel->SetFontSize(12.0f);
    _titleLabel->SetFontColor(D2D1::ColorF(0));
    _titleLabel->SetBaseSize(_titleLabel->GetTextWidth() + 1, 29);
    if (_iconImage)
        _titleLabel->SetHorizontalOffsetPixels(29 + 5);
    _titleLabel->SetHorizontalTextAlignment(zcom::TextAlignment::LEADING);
    _titleLabel->SetVerticalTextAlignment(zcom::Alignment::CENTER);

    // Enable ClearType
    _titleLabel->IgnoreAlpha(true);
    _titleLabel->SetBackgroundColor(_canvas->GetBackgroundColor());

    _canvas->AddComponent(_titleLabel.get());
}

void DefaultTitleBarScene::AddMenuButton(std::wstring name)
{

}

int DefaultTitleBarScene::TitleBarHeight()
{
    return 30;
}


RECT DefaultTitleBarScene::WindowMenuButtonRect()
{
    if (_iconImage)
    {
        return {
            _iconImage->GetX(),
            _iconImage->GetY(),
            _iconImage->GetX() + _iconImage->GetWidth(),
            _iconImage->GetY() + _iconImage->GetHeight()
        };
    }
    else {
        return { 0, 0, 0, 0 };
    }
}

std::vector<RECT> DefaultTitleBarScene::ExcludedCaptionRects()
{
    std::vector<RECT> excludedRects;

    // Add close button
    if (_closeButton)
    {
        excludedRects.push_back({
            _closeButton->GetX(),
            _closeButton->GetY(),
            _closeButton->GetX() + _closeButton->GetWidth(),
            _closeButton->GetY() + _closeButton->GetHeight()
        });
    }

    // Add minimixe button
    if (_minimizeButton)
    {
        excludedRects.push_back({
            _minimizeButton->GetX(),
            _minimizeButton->GetY(),
            _minimizeButton->GetX() + _minimizeButton->GetWidth(),
            _minimizeButton->GetY() + _minimizeButton->GetHeight()
        });
    }

    // Add maximize button
    if (_maximizeButton)
    {
        excludedRects.push_back({
            _maximizeButton->GetX(),
            _maximizeButton->GetY(),
            _maximizeButton->GetX() + _maximizeButton->GetWidth(),
            _maximizeButton->GetY() + _maximizeButton->GetHeight()
        });
    }

    // Add menu buttons
    for (int i = 0; i < _menuButtons.size(); i++)
    {
        excludedRects.push_back({
            _menuButtons[i]->GetX(),
            _menuButtons[i]->GetY(),
            _menuButtons[i]->GetX() + _menuButtons[i]->GetWidth(),
            _menuButtons[i]->GetY() + _menuButtons[i]->GetHeight()
        });
    }

    return excludedRects;
}

void DefaultTitleBarScene::_Uninit()
{
    _canvas->ClearComponents();
}

void DefaultTitleBarScene::_Focus()
{

}

void DefaultTitleBarScene::_Unfocus()
{

}

void DefaultTitleBarScene::_Update()
{
    _canvas->Update();
}

void DefaultTitleBarScene::_Resize(int width, int height)
{

}