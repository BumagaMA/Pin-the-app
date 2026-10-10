#pragma once

#include <Windows.h>

#include <array>

class SelectionOverlay {
public:
    SelectionOverlay() = default;
    SelectionOverlay(const SelectionOverlay&) = delete;
    SelectionOverlay& operator=(const SelectionOverlay&) = delete;
    ~SelectionOverlay();

    [[nodiscard]] bool Create(HINSTANCE instance);
    void ShowFor(HWND target);
    void ShowCursor(POINT position, HICON icon);
    void Hide();
    void Destroy();

private:
    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK CursorWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    std::array<HWND, 4> windows_{};
    HWND cursorWindow_ = nullptr;
    HICON cursorIcon_ = nullptr;
    HWND currentTarget_ = nullptr;
};
