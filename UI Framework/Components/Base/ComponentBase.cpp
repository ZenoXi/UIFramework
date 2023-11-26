#include "ComponentBase.h"

#include "App.h"
#include "Window/Window.h"

void zcom::Component::SafeFullRelease(IUnknown** res)
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

void zcom::Component::_ApplyCursor()
{
    _scene->GetWindow()->Backend().SetCursorIcon(_cursor);
}

void zcom::Component::_ShowHoverText()
{
    if (_hoverText.empty())
        return;

    //_scene->GetApp()->Overlay()->ShowHoverText(_hoverText, GetWindowX() + GetMousePosX(), GetWindowY() + GetMousePosY());
}