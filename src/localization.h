#pragma once

#include <string>

struct Strings {
    std::wstring appName;
    std::wstring tooltipIdle;
    std::wstring tooltipSelecting;
    std::wstring tooltipPinned;
    std::wstring selectWithPin;
    std::wstring openWindows;
    std::wstring noWindows;
    std::wstring pinnedWindows;
    std::wstring noPinnedWindows;
    std::wstring unpinAll;
    std::wstring runAtStartup;
    std::wstring diagnosticLogging;
    std::wstring about;
    std::wstring exit;
    std::wstring accessWarningTitle;
    std::wstring accessWarning;
    std::wstring operationErrorTitle;
    std::wstring operationError;
    std::wstring startupError;
    std::wstring hookError;
    std::wstring aboutText;
};

[[nodiscard]] bool IsRussianUiLanguage();
[[nodiscard]] Strings LoadStrings();
