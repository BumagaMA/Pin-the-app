#pragma once

#include "pinned_order.h"
#include "window_utils.h"

#include <Windows.h>

#include <unordered_map>
#include <vector>

enum class PinResult {
    Success,
    InvalidWindow,
    AccessDenied,
    SystemError,
};

class PinManager {
public:
    [[nodiscard]] PinResult Pin(HWND window);
    [[nodiscard]] PinResult Unpin(HWND window);
    [[nodiscard]] PinResult Toggle(HWND window);
    [[nodiscard]] PinResult UnpinAll();
    [[nodiscard]] bool CleanupClosed();
    [[nodiscard]] bool ForgetDestroyed(HWND window);
    void ReassertOrder();

    [[nodiscard]] bool IsPinned(HWND window) const { return order_.Contains(window); }
    [[nodiscard]] bool Empty() const { return order_.Empty(); }
    [[nodiscard]] const std::vector<HWND>& Windows() const { return order_.Items(); }

private:
    struct Record {
        WindowIdentity identity;
        bool wasTopmost = false;
    };

    [[nodiscard]] bool IsSameWindow(HWND window) const;
    [[nodiscard]] static PinResult ApplyTopmost(HWND window, bool topmost);

    PinnedOrder<HWND> order_;
    std::unordered_map<HWND, Record> records_;
};
