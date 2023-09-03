#pragma once

#include "Scene.h"

struct DefaultNonClientAreaSceneOptions : public SceneOptionsBase
{

};

class DefaultNonClientAreaScene : public Scene
{
public:
    // Returns the width of the resizing area on each side of the window
    virtual RECT GetResizingBorderWidths();
    // Returns the margins on each side of the client area (title bar and content) to the window edges
    virtual RECT GetClientAreaMargins();

    // Pass the pointer to the client area bitmap before rendering the final window
    void SetClientAreaBitmap(ID2D1Bitmap* clientAreaBitmap);
    // Pass the pointer to a draw title bar bitmap before rendering the final window
    void SetTitleBarBitmap(ID2D1Bitmap* titleBarBitmap);
    // Pass the pointer to a drawn client area bitmap before rendering the final window
    void SetContentBitmap(ID2D1Bitmap* clientAreaBitmap);

public:
    DefaultNonClientAreaScene(App* app, zwnd::Window* window);

protected:
    ID2D1Bitmap* _clientAreaBitmap = nullptr;
    ID2D1Bitmap* _titleBarBitmap = nullptr;
    ID2D1Bitmap* _contentBitmap = nullptr;

    // Common canvas to which the client area, additional scene elements and window effects are drawn
    ID2D1Bitmap1* _ccanvas = nullptr;
    bool _redraw = false;

private:
    void _Init(const SceneOptionsBase* options);
    void _Uninit();
    void _Focus();
    void _Unfocus();
    void _Update();
    bool _Redraw();
    ID2D1Bitmap* _Draw(Graphics g);
    ID2D1Bitmap* _Image();
    void _Resize(int width, int height);
public:
    const char* GetName() const { return StaticName(); }
    static const char* StaticName() { return "non_client_area"; }
};
