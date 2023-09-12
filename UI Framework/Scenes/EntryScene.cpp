#include "App.h" // App.h must be included first
#include "Window/Window.h"
#include "EntryScene.h"

zcom::EntryScene::EntryScene(App* app, zwnd::Window* window)
    : Scene(app, window)
{}

void zcom::EntryScene::_Init(const SceneOptionsBase* options)
{
    EntrySceneOptions opt;
    if (options)
    {
        opt = *reinterpret_cast<const EntrySceneOptions*>(options);
    }

    //_background = Create<zcom::DitheredBackground>();
    //_background->SetParentSizePercent(1.0f, 1.0f);

    //_helloWorldLabel = Create<zcom::Label>(L"Hello, world!");
    //_helloWorldLabel->SetBaseSize(200, 50);
    //_helloWorldLabel->SetAlignment(zcom::Alignment::CENTER, zcom::Alignment::CENTER);
    //_helloWorldLabel->SetFontSize(32.0f);
    //_helloWorldLabel->SetHorizontalTextAlignment(zcom::TextAlignment::CENTER);
    //_helloWorldLabel->SetVerticalTextAlignment(zcom::Alignment::CENTER);

    //_testPanel = Create<zcom::EmptyPanel>();
    //_testPanel->SetBaseSize(20, 20);
    //_testPanel->SetVerticalAlignment(zcom::Alignment::END);
    //_testPanel->SetBackgroundColor(D2D1::ColorF(0.5f, 0.5f, 0.5f));
    //_testPanel->AddOnLeftReleased([&](zcom::Base*, int, int)
    //{
    //    _window->Fullscreen(!_window->Fullscreen());
    //});

    _button = Create<Button>(L"Entry");
    _button->SetBaseSize(100, 40);
    _button->SetAlignment(Alignment::CENTER, Alignment::CENTER);
    _button->SetOnActivated([&]() {
        _window->Fullscreen(!_window->Fullscreen());
    });

    _canvas->AddComponent(_button.get());
    //_canvas->AddComponent(_background.get());
    //_canvas->AddComponent(_helloWorldLabel.get());
    //_canvas->AddComponent(_testPanel.get());
    _canvas->SetBackgroundColor(D2D1::ColorF(0, 0.0f));
    _canvas->SetOcclusive(false);
    //_canvas->SetBackgroundColor(D2D1::ColorF(0.0f, 0.0f, 0.0f));
}

void zcom::EntryScene::_Uninit()
{
    _canvas->ClearComponents();
}

void zcom::EntryScene::_Focus()
{

}

void zcom::EntryScene::_Unfocus()
{

}

void zcom::EntryScene::_Update()
{
    _canvas->Update();
}

void zcom::EntryScene::_Resize(int width, int height, ResizeInfo info)
{

}