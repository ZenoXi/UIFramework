#include "App.h" // App.h and Window.h must be included first
#include "Window/Window.h"
#include "DefaultTitleBarScene.h"

#include "Helper/ResourceManager.h";

zcom::DefaultTitleBarScene::DefaultTitleBarScene(App* app, zwnd::Window* window)
    : Scene(app, window)
{}

void zcom::DefaultTitleBarScene::_Init(const SceneOptionsBase* options)
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


void zcom::DefaultTitleBarScene::SetBackground(D2D1_COLOR_F color)
{
    _canvas->SetBackgroundColor(color);
}

void zcom::DefaultTitleBarScene::AddCloseButton()
{
    _closeButton = Create<Button>(ButtonPreset::NO_EFFECTS);
    _closeButton->SetBaseSize(45, 29);
    _closeButton->SetHorizontalAlignment(Alignment::END);
    _closeButton->SetButtonImageAll(_window->resourceManager.GetImage("window_close"));
    _closeButton->ButtonImage()->SetPlacement(ImagePlacement::CENTER);
    _closeButton->ButtonImage()->SetPixelSnap(true);
    _closeButton->UseImageParamsForAll(_closeButton->ButtonImage());
    _closeButton->SetButtonColor(D2D1::ColorF(0, 0.0f));
    _closeButton->SetButtonHoverColor(D2D1::ColorF(0xE81123));
    _closeButton->SetButtonClickColor(D2D1::ColorF(0xE81123, 0.54f));
    _closeButton->ButtonImage()->SetTintColor(D2D1::ColorF(0));
    _closeButton->ButtonHoverImage()->SetTintColor(D2D1::ColorF(1.0f, 1.0f, 1.0f));
    _closeButton->ButtonClickImage()->SetTintColor(D2D1::ColorF(1.0f, 1.0f, 1.0f));
    _closeButton->SetActivation(ButtonActivation::RELEASE);
    _closeButton->SetOnActivated([&]() {
        _window->Close();
    });

    _canvas->AddComponent(_closeButton.get());
}

void zcom::DefaultTitleBarScene::AddMaximizeButton()
{
    _maximizeButton = Create<Button>(ButtonPreset::NO_EFFECTS);
    _maximizeButton->SetBaseSize(45, 29);
    _maximizeButton->SetHorizontalAlignment(Alignment::END);
    if (_closeButton)
        _maximizeButton->SetHorizontalOffsetPixels(-45);
    _maximizeButton->SetButtonImageAll(_window->resourceManager.GetImage("window_maximize"));
    _maximizeButton->ButtonImage()->SetPlacement(ImagePlacement::CENTER);
    _maximizeButton->ButtonImage()->SetPixelSnap(true);
    _maximizeButton->ButtonImage()->SetTintColor(D2D1::ColorF(0));
    _maximizeButton->UseImageParamsForAll(_maximizeButton->ButtonImage());
    _maximizeButton->SetButtonColor(D2D1::ColorF(0, 0.0f));
    _maximizeButton->SetButtonHoverColor(D2D1::ColorF(0, 0.1f));
    _maximizeButton->SetButtonClickColor(D2D1::ColorF(0, 0.2f));
    _maximizeButton->SetActivation(ButtonActivation::RELEASE);
    _maximizeButton->SetOnActivated([&]() {
        if (_window->Backend().Maximized())
            _window->Backend().Restore();
        else
            _window->Backend().Maximize();
    });

    _canvas->AddComponent(_maximizeButton.get());
}

void zcom::DefaultTitleBarScene::AddMinimizeButton()
{
    _minimizeButton = Create<Button>(ButtonPreset::NO_EFFECTS);
    _minimizeButton->SetBaseSize(45, 29);
    _minimizeButton->SetHorizontalAlignment(Alignment::END);
    if (_closeButton && _maximizeButton)
        _minimizeButton->SetHorizontalOffsetPixels(-90);
    else if (_closeButton || _maximizeButton)
        _minimizeButton->SetHorizontalOffsetPixels(-45);
    _minimizeButton->SetButtonImageAll(_window->resourceManager.GetImage("window_minimize"));
    _minimizeButton->ButtonImage()->SetPlacement(ImagePlacement::CENTER);
    _minimizeButton->ButtonImage()->SetPixelSnap(true);
    _minimizeButton->ButtonImage()->SetTintColor(D2D1::ColorF(0));
    _minimizeButton->UseImageParamsForAll(_minimizeButton->ButtonImage());
    _minimizeButton->SetButtonColor(D2D1::ColorF(0, 0.0f));
    _minimizeButton->SetButtonHoverColor(D2D1::ColorF(0, 0.1f));
    _minimizeButton->SetButtonClickColor(D2D1::ColorF(0, 0.2f));
    _minimizeButton->SetActivation(ButtonActivation::RELEASE);
    _minimizeButton->SetOnActivated([&]() {
        _window->Backend().Minimize();
    });

    _canvas->AddComponent(_minimizeButton.get());
}

void zcom::DefaultTitleBarScene::AddIcon(ID2D1Bitmap* icon)
{
    _iconImage = Create<Image>(_window->resourceManager.GetImage("window_app_icon"));
    _iconImage->SetBaseSize(29, 29);
    _iconImage->SetPlacement(ImagePlacement::CENTER);
    _iconImage->SetPixelSnap(true);
    _iconImage->SetTintColor(D2D1::ColorF(0));

    _canvas->AddComponent(_iconImage.get());
}

void zcom::DefaultTitleBarScene::AddTitle(std::wstring title)
{
    _titleLabel = Create<Label>(title);
    _titleLabel->SetFont(L"Segoe UI");
    _titleLabel->SetFontSize(12.0f);
    _titleLabel->SetFontColor(D2D1::ColorF(0));
    _titleLabel->SetBaseSize(_titleLabel->GetTextWidth() + 1, 29);
    if (_iconImage)
        _titleLabel->SetHorizontalOffsetPixels(29 + 5);
    _titleLabel->SetHorizontalTextAlignment(TextAlignment::LEADING);
    _titleLabel->SetVerticalTextAlignment(Alignment::CENTER);

    // Enable ClearType
    _titleLabel->IgnoreAlpha(true);
    _titleLabel->SetBackgroundColor(_canvas->GetBackgroundColor());

    _canvas->AddComponent(_titleLabel.get());
}

void zcom::DefaultTitleBarScene::AddMenuButton(std::wstring name)
{

}

int zcom::DefaultTitleBarScene::TitleBarSceneHeight()
{
    return 30;
}

int zcom::DefaultTitleBarScene::CaptionHeight()
{
    return 30;
}


RECT zcom::DefaultTitleBarScene::WindowMenuButtonRect()
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

std::vector<RECT> zcom::DefaultTitleBarScene::ExcludedCaptionRects()
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

void zcom::DefaultTitleBarScene::_Uninit()
{
    _canvas->ClearComponents();
}

void zcom::DefaultTitleBarScene::_Focus()
{

}

void zcom::DefaultTitleBarScene::_Unfocus()
{

}

void zcom::DefaultTitleBarScene::_Update()
{
    _canvas->Update();
}

void zcom::DefaultTitleBarScene::_Resize(int width, int height, ResizeInfo info)
{
    if (_maximizeButton)
    {
        if (info.windowMaximized)
            _maximizeButton->SetButtonImageAll(_window->resourceManager.GetImage("window_restore"));
        else if (info.windowRestored)
            _maximizeButton->SetButtonImageAll(_window->resourceManager.GetImage("window_maximize"));
    }
}