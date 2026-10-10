#include "selection_overlay.h"

#include "window_utils.h"

#include <algorithm>

namespace {

constexpr wchar_t kOverlayClassName[] = L"PinAppBM.SelectionOverlay";
constexpr wchar_t kCursorOverlayClassName[] = L"PinAppBM.CursorOverlay";
constexpr COLORREF kBorderColor = RGB(208, 208, 208);
constexpr COLORREF kCursorTransparencyColor = RGB(1, 2, 3);
constexpr int kCursorSize = 32;
constexpr int kCursorHotspotX = 16;
constexpr int kCursorHotspotY = 27;

} // namespace

SelectionOverlay::~SelectionOverlay()
{
    Destroy();
}

bool SelectionOverlay::Create(HINSTANCE instance)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.lpszClassName = kOverlayClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (RegisterClassExW(&windowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    WNDCLASSEXW cursorWindowClass{};
    cursorWindowClass.cbSize = sizeof(cursorWindowClass);
    cursorWindowClass.hInstance = instance;
    cursorWindowClass.lpfnWndProc = CursorWindowProcedure;
    cursorWindowClass.lpszClassName = kCursorOverlayClassName;
    cursorWindowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (RegisterClassExW(&cursorWindowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    for (HWND& window : windows_) {
        window = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            kOverlayClassName,
            L"",
            WS_POPUP,
            0,
            0,
            0,
            0,
            nullptr,
            nullptr,
            instance,
            nullptr);
        if (window == nullptr) {
            Destroy();
            return false;
        }
        SetLayeredWindowAttributes(window, 0, 225, LWA_ALPHA);
    }

    cursorWindow_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kCursorOverlayClassName,
        L"",
        WS_POPUP,
        0,
        0,
        kCursorSize,
        kCursorSize,
        nullptr,
        nullptr,
        instance,
        this);
    if (cursorWindow_ == nullptr) {
        Destroy();
        return false;
    }
    SetLayeredWindowAttributes(cursorWindow_, kCursorTransparencyColor, 255, LWA_COLORKEY);
    return true;
}

void SelectionOverlay::ShowFor(HWND target)
{
    if (target == nullptr || !IsWindow(target) || IsIconic(target)) {
        Hide();
        return;
    }

    const RECT bounds = GetExtendedWindowBounds(target);
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    if (width <= 0 || height <= 0) {
        Hide();
        return;
    }

    const UINT dpi = GetDpiForWindow(target);
    const int thickness = std::max(2, MulDiv(2, dpi == 0 ? 96 : dpi, 96));
    const int verticalHeight = std::max(0, height - thickness * 2);
    const std::array<RECT, 4> rectangles{
        RECT{bounds.left, bounds.top, bounds.right, bounds.top + thickness},
        RECT{bounds.left, bounds.bottom - thickness, bounds.right, bounds.bottom},
        RECT{bounds.left, bounds.top + thickness, bounds.left + thickness, bounds.top + thickness + verticalHeight},
        RECT{bounds.right - thickness, bounds.top + thickness, bounds.right, bounds.top + thickness + verticalHeight},
    };

    for (std::size_t index = 0; index < windows_.size(); ++index) {
        const RECT& rectangle = rectangles[index];
        SetWindowPos(
            windows_[index],
            HWND_TOPMOST,
            rectangle.left,
            rectangle.top,
            rectangle.right - rectangle.left,
            rectangle.bottom - rectangle.top,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    currentTarget_ = target;
}

void SelectionOverlay::ShowCursor(POINT position, HICON icon)
{
    if (cursorWindow_ == nullptr || icon == nullptr) {
        return;
    }

    cursorIcon_ = icon;
    InvalidateRect(cursorWindow_, nullptr, TRUE);
    SetWindowPos(
        cursorWindow_,
        HWND_TOPMOST,
        position.x - kCursorHotspotX,
        position.y - kCursorHotspotY,
        kCursorSize,
        kCursorSize,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void SelectionOverlay::Hide()
{
    for (HWND window : windows_) {
        if (window != nullptr) {
            ShowWindow(window, SW_HIDE);
        }
    }
    if (cursorWindow_ != nullptr) {
        ShowWindow(cursorWindow_, SW_HIDE);
    }
    cursorIcon_ = nullptr;
    currentTarget_ = nullptr;
}

void SelectionOverlay::Destroy()
{
    for (HWND& window : windows_) {
        if (window != nullptr) {
            DestroyWindow(window);
            window = nullptr;
        }
    }
    if (cursorWindow_ != nullptr) {
        DestroyWindow(cursorWindow_);
        cursorWindow_ = nullptr;
    }
    cursorIcon_ = nullptr;
    currentTarget_ = nullptr;
}

LRESULT CALLBACK SelectionOverlay::WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC deviceContext = BeginPaint(window, &paint);
        HBRUSH brush = CreateSolidBrush(kBorderColor);
        FillRect(deviceContext, &paint.rcPaint, brush);
        DeleteObject(brush);
        EndPaint(window, &paint);
        return 0;
    }
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

LRESULT CALLBACK SelectionOverlay::CursorWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* overlay = reinterpret_cast<SelectionOverlay*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        overlay = static_cast<SelectionOverlay*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(overlay));
    }

    switch (message) {
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC deviceContext = BeginPaint(window, &paint);
        HBRUSH background = CreateSolidBrush(kCursorTransparencyColor);
        FillRect(deviceContext, &paint.rcPaint, background);
        DeleteObject(background);
        if (overlay != nullptr && overlay->cursorIcon_ != nullptr) {
            DrawIconEx(
                deviceContext,
                0,
                0,
                overlay->cursorIcon_,
                kCursorSize,
                kCursorSize,
                0,
                nullptr,
                DI_NORMAL);
        }
        EndPaint(window, &paint);
        return 0;
    }
    case WM_NCDESTROY:
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        return DefWindowProcW(window, message, wParam, lParam);
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}
