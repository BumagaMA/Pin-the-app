#include "pin_manager.h"

#include <Windows.h>

#include <cstdlib>
#include <iostream>

namespace {

void Expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool IsTopmost(HWND window)
{
    return (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
}

HWND CreateTestWindow(bool topmost)
{
    return CreateWindowExW(
        topmost ? WS_EX_TOPMOST : 0,
        L"STATIC",
        L"Pin App BM integration test",
        WS_POPUP,
        0,
        0,
        100,
        100,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
}

bool IsAbove(HWND candidate, HWND other)
{
    for (HWND window = GetTopWindow(nullptr); window != nullptr; window = GetWindow(window, GW_HWNDNEXT)) {
        if (window == candidate) {
            return true;
        }
        if (window == other) {
            return false;
        }
    }
    return false;
}

} // namespace

int main()
{
    PinManager manager;

    HWND ordinaryWindow = CreateTestWindow(false);
    Expect(ordinaryWindow != nullptr, "ordinary test window should be created");
    Expect(manager.Pin(ordinaryWindow) == PinResult::Success, "ordinary window should be pinned");
    Expect(manager.IsPinned(ordinaryWindow), "manager should track the pinned window");
    Expect(IsTopmost(ordinaryWindow), "pinned window should be topmost");
    Expect(manager.Unpin(ordinaryWindow) == PinResult::Success, "ordinary window should be unpinned");
    Expect(!IsTopmost(ordinaryWindow), "unpin should restore an ordinary window");
    DestroyWindow(ordinaryWindow);

    HWND originalTopmostWindow = CreateTestWindow(true);
    Expect(originalTopmostWindow != nullptr, "topmost test window should be created");
    Expect(manager.Pin(originalTopmostWindow) == PinResult::Success, "existing topmost window should be tracked");
    Expect(manager.Unpin(originalTopmostWindow) == PinResult::Success, "existing topmost window should be untracked");
    Expect(IsTopmost(originalTopmostWindow), "pre-existing topmost state should be preserved");
    DestroyWindow(originalTopmostWindow);

    HWND olderWindow = CreateTestWindow(false);
    HWND newerWindow = CreateTestWindow(false);
    Expect(olderWindow != nullptr && newerWindow != nullptr, "ordering test windows should be created");
    Expect(manager.Pin(olderWindow) == PinResult::Success, "older window should be pinned");
    Expect(manager.Pin(newerWindow) == PinResult::Success, "newer window should be pinned");
    Expect(IsAbove(newerWindow, olderWindow), "most recently pinned window should be above older managed windows");
    Expect(manager.UnpinAll() == PinResult::Success, "all ordered windows should be unpinned");
    DestroyWindow(olderWindow);
    DestroyWindow(newerWindow);

    HWND destroyedWindow = CreateTestWindow(false);
    Expect(manager.Pin(destroyedWindow) == PinResult::Success, "window should be pinned before destruction");
    DestroyWindow(destroyedWindow);
    Expect(manager.ForgetDestroyed(destroyedWindow), "destroy event should forget the exact HWND");
    Expect(manager.Empty(), "destroyed window should not remain in the manager");

    std::cout << "All pin manager integration tests passed.\n";
    return EXIT_SUCCESS;
}
