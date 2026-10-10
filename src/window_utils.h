#pragma once

#include <Windows.h>

#include <string>
#include <vector>

struct WindowIdentity {
    HWND handle = nullptr;
    DWORD processId = 0;
    DWORD threadId = 0;
    ULONGLONG processCreationTime = 0;
};

struct WindowInfo {
    WindowIdentity identity;
    std::wstring title;
    std::wstring processName;
};

[[nodiscard]] bool CaptureWindowIdentity(HWND window, WindowIdentity& identity);
[[nodiscard]] bool MatchesWindowIdentity(const WindowIdentity& identity);
[[nodiscard]] bool IsWindowHigherIntegrity(HWND window);
[[nodiscard]] bool IsSelectableWindow(HWND window, DWORD ownProcessId);
[[nodiscard]] HWND SelectableWindowAtPoint(POINT point, DWORD ownProcessId);
[[nodiscard]] std::vector<WindowInfo> EnumerateSelectableWindows(DWORD ownProcessId);
[[nodiscard]] std::wstring GetWindowTitle(HWND window);
[[nodiscard]] std::wstring GetWindowProcessName(HWND window);
[[nodiscard]] std::wstring FormatWindowLabel(const WindowInfo& window, std::size_t maximumLength = 96);
[[nodiscard]] RECT GetExtendedWindowBounds(HWND window);
