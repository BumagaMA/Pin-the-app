#pragma once

#include <Windows.h>
#include <shellapi.h>

// Explorer can report one tray-icon activation as both WM_LBUTTONUP and
// NIN_SELECT. Keep the first event and discard the paired notification so a
// single click cannot immediately enable and then disable selection mode.
class TrayActivationFilter {
public:
    [[nodiscard]] bool ShouldActivate(UINT event, DWORD messageTime, DWORD duplicateWindowMilliseconds)
    {
        if (event == WM_LBUTTONUP) {
            lastMouseUpTime_ = messageTime;
            hasMouseUp_ = true;
            return true;
        }

        if (event == NIN_SELECT) {
            const bool duplicate =
                hasMouseUp_ && messageTime - lastMouseUpTime_ <= duplicateWindowMilliseconds;
            hasMouseUp_ = false;
            return !duplicate;
        }

        if (event == NIN_KEYSELECT) {
            hasMouseUp_ = false;
            return true;
        }

        return false;
    }

private:
    DWORD lastMouseUpTime_ = 0;
    bool hasMouseUp_ = false;
};
