#include "localization.h"

#include <Windows.h>

bool IsRussianUiLanguage()
{
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_RUSSIAN;
}

Strings LoadStrings()
{
    if (IsRussianUiLanguage()) {
        return {
            L"Pin App BM",
            L"Pin App BM — нажмите, чтобы выбрать окно",
            L"Pin App BM — выберите окно (Esc — отмена)",
            L"Pin App BM — есть закреплённые окна",
            L"Выбрать окно булавкой",
            L"Открытые окна",
            L"Нет доступных окон",
            L"Закреплённые окна",
            L"Нет закреплённых окон",
            L"Открепить все",
            L"Запускать вместе с Windows",
            L"Диагностический журнал",
            L"О программе",
            L"Выход",
            L"Недостаточно прав",
            L"Windows не разрешила изменить выбранное окно. Возможно, оно запущено от имени администратора. Pin App BM не требует постоянного запуска с повышенными правами.",
            L"Не удалось выполнить действие",
            L"Windows вернула ошибку при изменении режима «поверх остальных».",
            L"Не удалось изменить настройку автозапуска.",
            L"Не удалось включить режим выбора окна.",
            L"Pin App BM " PIN_APP_VERSION L"\n\nЗакрепляет отдельные окна поверх остальных.\nC++20 / Win32 / MIT License",
        };
    }

    return {
        L"Pin App BM",
        L"Pin App BM — click to select a window",
        L"Pin App BM — select a window (Esc to cancel)",
        L"Pin App BM — windows are pinned",
        L"Select a window with the pin",
        L"Open windows",
        L"No available windows",
        L"Pinned windows",
        L"No pinned windows",
        L"Unpin all",
        L"Run at Windows startup",
        L"Diagnostic logging",
        L"About",
        L"Exit",
        L"Insufficient permissions",
        L"Windows did not allow the selected window to be changed. It may be running as administrator. Pin App BM does not require permanent elevated privileges.",
        L"The action failed",
        L"Windows reported an error while changing the always-on-top state.",
        L"The startup setting could not be changed.",
        L"Window selection mode could not be started.",
        L"Pin App BM " PIN_APP_VERSION L"\n\nPins individual windows above other applications.\nC++20 / Win32 / MIT License",
    };
}
