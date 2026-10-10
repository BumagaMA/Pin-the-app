#pragma once

#include <Windows.h>
#include <shellapi.h>

#include <string>

class TrayIcon {
public:
    TrayIcon() = default;
    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;
    ~TrayIcon();

    [[nodiscard]] bool Add(HWND owner, UINT callbackMessage, HICON icon, const std::wstring& tooltip);
    [[nodiscard]] bool ReAdd();
    [[nodiscard]] bool Update(HICON icon, const std::wstring& tooltip);
    void Remove();
    void SetFocus() const;
    [[nodiscard]] bool GetRectangle(RECT& rectangle) const;
    [[nodiscard]] bool ContainsPoint(POINT point) const;
    [[nodiscard]] bool UsesVersion4() const { return version4_; }

private:
    NOTIFYICONDATAW data_{};
    bool added_ = false;
    bool version4_ = false;
};
