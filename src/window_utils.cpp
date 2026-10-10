#include "window_utils.h"

#include <dwmapi.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace {

bool IsCloaked(HWND window)
{
    DWORD cloaked = 0;
    return SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0;
}

std::wstring EscapeMenuText(std::wstring value)
{
    std::size_t position = 0;
    while ((position = value.find(L'&', position)) != std::wstring::npos) {
        value.insert(position, 1, L'&');
        position += 2;
    }
    return value;
}

bool GetProcessCreationValue(DWORD processId, ULONGLONG& value)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (process == nullptr) {
        return false;
    }

    FILETIME creation{}, exit{}, kernel{}, user{};
    const BOOL success = GetProcessTimes(process, &creation, &exit, &kernel, &user);
    CloseHandle(process);
    if (!success) {
        return false;
    }

    value = (static_cast<ULONGLONG>(creation.dwHighDateTime) << 32U) | creation.dwLowDateTime;
    return true;
}

bool GetProcessIntegrityLevel(DWORD processId, DWORD& level)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (process == nullptr) {
        return false;
    }

    HANDLE token = nullptr;
    if (!OpenProcessToken(process, TOKEN_QUERY, &token)) {
        CloseHandle(process);
        return false;
    }

    DWORD byteCount = 0;
    GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &byteCount);
    std::vector<std::uint8_t> buffer(byteCount);
    const BOOL success = byteCount != 0
        && GetTokenInformation(token, TokenIntegrityLevel, buffer.data(), byteCount, &byteCount);
    if (success) {
        const auto* label = reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(buffer.data());
        const DWORD subAuthorityCount = *GetSidSubAuthorityCount(label->Label.Sid);
        level = *GetSidSubAuthority(label->Label.Sid, subAuthorityCount - 1);
    }

    CloseHandle(token);
    CloseHandle(process);
    return success != FALSE;
}

} // namespace

bool CaptureWindowIdentity(HWND window, WindowIdentity& identity)
{
    if (window == nullptr || !IsWindow(window)) {
        return false;
    }

    DWORD processId = 0;
    const DWORD threadId = GetWindowThreadProcessId(window, &processId);
    if (threadId == 0 || processId == 0) {
        return false;
    }

    ULONGLONG creationTime = 0;
    (void)GetProcessCreationValue(processId, creationTime);
    identity = {window, processId, threadId, creationTime};
    return true;
}

bool MatchesWindowIdentity(const WindowIdentity& identity)
{
    WindowIdentity current;
    if (!CaptureWindowIdentity(identity.handle, current)) {
        return false;
    }
    return current.processId == identity.processId && current.threadId == identity.threadId
        && (identity.processCreationTime == 0 || current.processCreationTime == 0
            || current.processCreationTime == identity.processCreationTime);
}

bool IsWindowHigherIntegrity(HWND window)
{
    DWORD targetProcessId = 0;
    GetWindowThreadProcessId(window, &targetProcessId);
    DWORD currentLevel = 0;
    DWORD targetLevel = 0;
    return targetProcessId != 0 && GetProcessIntegrityLevel(GetCurrentProcessId(), currentLevel)
        && GetProcessIntegrityLevel(targetProcessId, targetLevel) && targetLevel > currentLevel;
}

std::wstring GetWindowTitle(HWND window)
{
    const int length = GetWindowTextLengthW(window);
    if (length <= 0) {
        return {};
    }

    std::wstring title(static_cast<std::size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(window, title.data(), static_cast<int>(title.size()));
    if (copied <= 0) {
        return {};
    }
    title.resize(static_cast<std::size_t>(copied));
    return title;
}

std::wstring GetWindowProcessName(HWND window)
{
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId == 0) {
        return {};
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (process == nullptr) {
        return {};
    }

    std::wstring path(512, L'\0');
    DWORD size = static_cast<DWORD>(path.size());
    const BOOL success = QueryFullProcessImageNameW(process, 0, path.data(), &size);
    CloseHandle(process);
    if (!success) {
        return {};
    }

    path.resize(size);
    return std::filesystem::path(path).filename().wstring();
}

bool IsSelectableWindow(HWND window, DWORD ownProcessId)
{
    if (window == nullptr || !IsWindow(window) || !IsWindowVisible(window)) {
        return false;
    }

    if (GetAncestor(window, GA_ROOT) != window || window == GetShellWindow() || window == GetDesktopWindow()) {
        return false;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId == 0 || processId == ownProcessId) {
        return false;
    }

    const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    const LONG_PTR extendedStyle = GetWindowLongPtrW(window, GWL_EXSTYLE);
    if ((style & WS_CHILD) != 0 || ((extendedStyle & WS_EX_TOOLWINDOW) != 0 && (extendedStyle & WS_EX_APPWINDOW) == 0)) {
        return false;
    }

    return !IsCloaked(window) && !GetWindowTitle(window).empty();
}

HWND SelectableWindowAtPoint(POINT point, DWORD ownProcessId)
{
    HWND window = WindowFromPoint(point);
    if (window != nullptr) {
        window = GetAncestor(window, GA_ROOT);
        if (IsSelectableWindow(window, ownProcessId) && !IsIconic(window)) {
            return window;
        }

        DWORD processId = 0;
        GetWindowThreadProcessId(window, &processId);
        if (processId != ownProcessId) {
            // A taskbar, desktop, system overlay, or another non-selectable
            // window was clicked. Never select a window merely located below it.
            return nullptr;
        }
    } else {
        return nullptr;
    }

    // WindowFromPoint can encounter one of our transparent frame windows.
    // Walk top-level z-order and skip every window owned by this process.
    for (window = GetTopWindow(nullptr); window != nullptr; window = GetWindow(window, GW_HWNDNEXT)) {
        if (!IsSelectableWindow(window, ownProcessId) || IsIconic(window)) {
            continue;
        }
        const RECT bounds = GetExtendedWindowBounds(window);
        if (PtInRect(&bounds, point) != FALSE) {
            return window;
        }
    }
    return nullptr;
}

std::vector<WindowInfo> EnumerateSelectableWindows(DWORD ownProcessId)
{
    struct Context {
        DWORD ownProcessId;
        std::vector<WindowInfo>* windows;
    } context{ownProcessId, nullptr};

    std::vector<WindowInfo> windows;
    context.windows = &windows;
    EnumWindows(
        [](HWND window, LPARAM parameter) -> BOOL {
            auto& context = *reinterpret_cast<Context*>(parameter);
            if (IsSelectableWindow(window, context.ownProcessId)) {
                WindowIdentity identity;
                if (CaptureWindowIdentity(window, identity)) {
                    context.windows->push_back({identity, GetWindowTitle(window), GetWindowProcessName(window)});
                }
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&context));
    return windows;
}

std::wstring FormatWindowLabel(const WindowInfo& window, std::size_t maximumLength)
{
    std::wstring label = window.title;
    if (!window.processName.empty()) {
        label += L" — " + window.processName;
    }
    if (label.size() > maximumLength && maximumLength > 3) {
        label.resize(maximumLength - 3);
        label += L"...";
    }
    return EscapeMenuText(std::move(label));
}

RECT GetExtendedWindowBounds(HWND window)
{
    RECT bounds{};
    if (FAILED(DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &bounds, sizeof(bounds)))) {
        GetWindowRect(window, &bounds);
    }
    return bounds;
}
