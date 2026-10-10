#include "pin_manager.h"

#include <algorithm>

namespace {

constexpr UINT kPositionFlags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER;
constexpr UINT kResponsivenessTimeoutMilliseconds = 250;

} // namespace

PinResult PinManager::ApplyTopmost(HWND window, bool topmost)
{
    DWORD_PTR response = 0;
    SetLastError(ERROR_SUCCESS);
    if (SendMessageTimeoutW(
            window,
            WM_NULL,
            0,
            0,
            SMTO_ABORTIFHUNG | SMTO_BLOCK,
            kResponsivenessTimeoutMilliseconds,
            &response)
        == 0) {
        return GetLastError() == ERROR_ACCESS_DENIED ? PinResult::AccessDenied : PinResult::SystemError;
    }

    SetLastError(ERROR_SUCCESS);
    if (SetWindowPos(window, topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, kPositionFlags) != FALSE) {
        return PinResult::Success;
    }
    return GetLastError() == ERROR_ACCESS_DENIED ? PinResult::AccessDenied : PinResult::SystemError;
}

PinResult PinManager::Pin(HWND window)
{
    if (window == nullptr || !IsWindow(window)) {
        return PinResult::InvalidWindow;
    }
    if (IsPinned(window)) {
        return PinResult::Success;
    }

    WindowIdentity identity;
    if (!CaptureWindowIdentity(window, identity)) {
        return PinResult::InvalidWindow;
    }

    SetLastError(ERROR_SUCCESS);
    const bool wasTopmost = (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    if (!wasTopmost && GetLastError() != ERROR_SUCCESS) {
        return GetLastError() == ERROR_ACCESS_DENIED ? PinResult::AccessDenied : PinResult::SystemError;
    }
    const PinResult result = ApplyTopmost(window, true);
    if (result != PinResult::Success) {
        return result;
    }

    records_[window] = {identity, wasTopmost};
    order_.Pin(window);
    ReassertOrder();
    return PinResult::Success;
}

PinResult PinManager::Unpin(HWND window)
{
    const auto record = records_.find(window);
    if (record == records_.end() || !order_.Contains(window)) {
        return PinResult::InvalidWindow;
    }

    if (IsSameWindow(window) && !record->second.wasTopmost) {
        const PinResult result = ApplyTopmost(window, false);
        if (result != PinResult::Success) {
            return result;
        }
    }

    (void)order_.Unpin(window);
    records_.erase(record);
    ReassertOrder();
    return PinResult::Success;
}

PinResult PinManager::Toggle(HWND window)
{
    return IsPinned(window) ? Unpin(window) : Pin(window);
}

PinResult PinManager::UnpinAll()
{
    PinResult aggregateResult = PinResult::Success;
    const std::vector<HWND> windows = order_.Items();
    for (auto iterator = windows.rbegin(); iterator != windows.rend(); ++iterator) {
        const PinResult result = Unpin(*iterator);
        if (result != PinResult::Success && result != PinResult::InvalidWindow) {
            aggregateResult = result;
        }
    }
    return aggregateResult;
}

bool PinManager::CleanupClosed()
{
    const std::size_t removed = order_.RemoveIf([this](HWND window) {
        if (IsSameWindow(window)) {
            return false;
        }
        records_.erase(window);
        return true;
    });
    return removed != 0;
}

bool PinManager::ForgetDestroyed(HWND window)
{
    const bool removed = order_.Unpin(window);
    records_.erase(window);
    return removed;
}

void PinManager::ReassertOrder()
{
    (void)CleanupClosed();
    for (HWND window : order_.Items()) {
        (void)ApplyTopmost(window, true);
    }
}

bool PinManager::IsSameWindow(HWND window) const
{
    const auto record = records_.find(window);
    if (record == records_.end() || !IsWindow(window)) {
        return false;
    }

    return MatchesWindowIdentity(record->second.identity);
}
