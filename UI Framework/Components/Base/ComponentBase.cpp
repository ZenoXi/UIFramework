#include "ComponentBase.h"

#include "App.h"
#include "Window/Window.h"

void zcom::Base::SafeFullRelease(IUnknown** res)
{
    _scene->GetWindow()->Backend().Graphics()->ReleaseResource(res);
    //App::Instance()->window.gfx.ReleaseResource(res);
}

void zcom::SafeRelease(IUnknown** res)
{
    if (*res)
    {
        (*res)->Release();
        *res = nullptr;
    }
}

void zcom::Base::_ApplyCursor()
{
    _scene->GetApp()->window.SetCursorIcon(_cursor);
}

void zcom::Base::_ShowHoverText()
{
    if (_hoverText.empty())
        return;

    //_scene->GetApp()->Overlay()->ShowHoverText(_hoverText, GetScreenX() + GetMousePosX(), GetScreenY() + GetMousePosY());
}