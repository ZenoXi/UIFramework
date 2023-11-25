#include "App.h" // App.h must be included first
#include "Window/Window.h"
#include "EntryScene.h"
#include "ContextMenuScene.h"

zcom::EntryScene::EntryScene(App* app, zwnd::Window* window)
    : Scene(app, window)
{}

void zcom::EntryScene::_Init(SceneOptionsBase* options)
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
    _button->SubscribeOnActivated([&]() {
        //_window->Fullscreen(!_window->Fullscreen());

        auto titleBarScene = _window->GetTitleBarScene();
        int titleBarHeight = 0;
        if (titleBarScene)
            titleBarHeight = titleBarScene->TitleBarSceneHeight();
        RECT clientAreaMargins = _window->GetNonClientAreaScene()->GetClientAreaMargins();
        RECT buttonRectInScreenCoords = {
            _button->GetScreenX() + clientAreaMargins.left,
            _button->GetScreenY() + clientAreaMargins.top + titleBarHeight,
            _button->GetScreenX() + clientAreaMargins.left + _button->GetWidth(),
            _button->GetScreenY() + clientAreaMargins.top + titleBarHeight + _button->GetHeight()
        };
        RECT windowRect = _window->Backend().GetWindowRectangle();
        buttonRectInScreenCoords.left += windowRect.left;
        buttonRectInScreenCoords.top += windowRect.top;
        buttonRectInScreenCoords.right += windowRect.left;
        buttonRectInScreenCoords.bottom += windowRect.top;

        //std::thread thr = std::thread([buttonRectInScreenCoords, this]() {
            std::optional<zwnd::WindowId> menuId = _app->CreateToolWindow(
                _window->GetWindowId(),
                zwnd::WindowProperties()
                    .WindowClassName(L"wndClassMenu")
                    .InitialSize(100, 100)
                    .InitialDisplay(zwnd::WindowDisplayType::HIDDEN)
                    .DisableWindowAnimations()
                    .DisableWindowActivation(),
                [buttonRectInScreenCoords](zwnd::Window* wnd)
                {
                    wnd->resourceManager.SetImageResourceFilePath("Resources/Images/resources.resc");
                    wnd->resourceManager.InitAllImages();
                    wnd->LoadNonClientAreaScene<zcom::DefaultNonClientAreaScene>(nullptr);

                    MenuTemplate::Menu innerMenu2;
                    MenuTemplate::MenuItem item2p1;
                    item2p1.text = L"Test 2.1";
                    item2p1.onClick = [](bool b) {};
                    MenuTemplate::MenuItem item2p2;
                    item2p2.text = L"Test 2.2";
                    item2p2.onClick = [](bool b) {};
                    innerMenu2.items.push_back(item2p1);
                    innerMenu2.items.push_back(item2p2);

                    MenuTemplate::Menu innerMenu1;
                    MenuTemplate::MenuItem item1p1;
                    item1p1.text = L"Test 1.1";
                    item1p1.onClick = [](bool b) {};
                    MenuTemplate::MenuItem item1p2;
                    item1p2.text = L"Test 1.2";
                    item1p2.nestedMenu = innerMenu2;
                    innerMenu1.items.push_back(item1p1);
                    innerMenu1.items.push_back(item1p2);

                    MenuTemplate::Menu menuTemplate;
                    MenuTemplate::MenuItem item1;
                    item1.text = L"Test 1";
                    item1.onClick = [](bool b) {};
                    MenuTemplate::MenuItem separator;
                    separator.separator = true;
                    MenuTemplate::MenuItem item2;
                    item2.text = L"Test 2";
                    item2.nestedMenu = innerMenu1;
                    MenuTemplate::MenuItem item3;
                    item3.text = L"Test 3";
                    item3.nestedMenu = innerMenu1;
                    MenuTemplate::MenuItem item4;
                    item4.text = L"Test 4";
                    item4.nestedMenu = innerMenu1;
                    menuTemplate.items.push_back(item1);
                    menuTemplate.items.push_back(separator);
                    menuTemplate.items.push_back(item2);
                    menuTemplate.items.push_back(item3);
                    menuTemplate.items.push_back(item4);

                    zcom::ContextMenuSceneOptions opt;
                    opt.params.parentRect = buttonRectInScreenCoords;
                    opt.params.menuTemplate = menuTemplate;
                    wnd->LoadStartingScene<zcom::ContextMenuScene>(&opt);
                }
            );
        //});
        //thr.detach();
    }).Detach();

    _canvas->AddComponent(_button.get());
    //_canvas->AddComponent(_background.get());
    //_canvas->AddComponent(_helloWorldLabel.get());
    //_canvas->AddComponent(_testPanel.get());
    _canvas->SetBackgroundColor(D2D1::ColorF(0.1f, 0.1f, 0.1f));
    _canvas->SetOcclusive(false);
    _canvas->BasePanel()->SubscribeOnMouseMove([](Component* item, int, int) {
        item->InvokeRedraw();
    }).Detach();
    _canvas->BasePanel()->SubscribePostDraw([](Component* item, Graphics g) {
        D2D1_RECT_F rect = D2D1::RectF(
            item->GetMousePosX() - 5.0f,
            item->GetMousePosY() - 5.0f,
            item->GetMousePosX() + 5.0f,
            item->GetMousePosY() + 5.0f
        );
        ID2D1SolidColorBrush* brush;
        g.target->CreateSolidColorBrush(D2D1::ColorF(0xFF0000), &brush);
        g.target->FillRectangle(rect, brush);
        brush->Release();
    }).Detach();
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