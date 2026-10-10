#include "tray_icon.h"

#include <strsafe.h>

TrayIcon::~TrayIcon()
{
    Remove();
}

bool TrayIcon::Add(HWND owner, UINT callbackMessage, HICON icon, const std::wstring& tooltip)
{
    Remove();
    data_ = {};
    data_.cbSize = sizeof(data_);
    data_.hWnd = owner;
    data_.uID = 1;
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data_.uCallbackMessage = callbackMessage;
    data_.hIcon = icon;
    StringCchCopyW(data_.szTip, ARRAYSIZE(data_.szTip), tooltip.c_str());

    added_ = Shell_NotifyIconW(NIM_ADD, &data_) != FALSE;
    if (added_) {
        data_.uVersion = NOTIFYICON_VERSION_4;
        version4_ = Shell_NotifyIconW(NIM_SETVERSION, &data_) != FALSE;
    }
    return added_;
}

bool TrayIcon::ReAdd()
{
    if (data_.hWnd == nullptr) {
        return false;
    }
    const HWND owner = data_.hWnd;
    const UINT callbackMessage = data_.uCallbackMessage;
    const HICON icon = data_.hIcon;
    const std::wstring tooltip = data_.szTip;
    added_ = false;
    return Add(owner, callbackMessage, icon, tooltip);
}

bool TrayIcon::Update(HICON icon, const std::wstring& tooltip)
{
    data_.uFlags = NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data_.hIcon = icon;
    StringCchCopyW(data_.szTip, ARRAYSIZE(data_.szTip), tooltip.c_str());
    return added_ && Shell_NotifyIconW(NIM_MODIFY, &data_) != FALSE;
}

void TrayIcon::Remove()
{
    if (added_) {
        Shell_NotifyIconW(NIM_DELETE, &data_);
        added_ = false;
    }
    version4_ = false;
}

void TrayIcon::SetFocus() const
{
    if (added_) {
        Shell_NotifyIconW(NIM_SETFOCUS, const_cast<NOTIFYICONDATAW*>(&data_));
    }
}

bool TrayIcon::ContainsPoint(POINT point) const
{
    RECT rectangle{};
    return GetRectangle(rectangle) && PtInRect(&rectangle, point) != FALSE;
}

bool TrayIcon::GetRectangle(RECT& rectangle) const
{
    NOTIFYICONIDENTIFIER identifier{};
    identifier.cbSize = sizeof(identifier);
    identifier.hWnd = data_.hWnd;
    identifier.uID = data_.uID;
    return SUCCEEDED(Shell_NotifyIconGetRect(&identifier, &rectangle));
}
