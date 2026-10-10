# Pin App BM

Нативная x64-утилита для Windows 10 22H2 и Windows 11, которая закрепляет отдельные окна поверх остальных приложений. Работает из области уведомлений, не требует установки и не отправляет данные по сети.

Native x64 utility for Windows 10 22H2 and Windows 11 that pins individual windows above other applications. It runs in the notification area, requires no installation, and makes no network requests.

## Возможности

- ЛКМ по белой булавке включает выбор окна; булавка становится синей.
- Тонкая светло-серая рамка показывает окно под курсором.
- Первый клик выбирает окно и не передаётся ему.
- Повторный выбор закреплённого окна открепляет его.
- Красная булавка означает, что закреплено хотя бы одно окно.
- Можно закрепить несколько окон; последнее закреплённое располагается выше остальных управляемых окон.
- ПКМ открывает список открытых и закреплённых окон, автозапуск, диагностику и выход.
- `Esc` или повторный ЛКМ по значку отменяют выбор.
- Русский интерфейс выбирается для русского UI-языка Windows, английский — для остальных.
- При штатном выходе все изменённые приложением окна открепляются.

## Features

- Left-click the white pin to enter selection mode; the pin turns blue.
- A thin light-gray frame shows the window under the pointer.
- The first click selects the window and is not delivered to it.
- Selecting a managed pinned window again unpins it.
- A red pin means at least one window is pinned.
- Multiple windows are supported; the most recently pinned managed window stays above the others.
- Right-click opens lists of open and pinned windows, startup settings, diagnostics, and Exit.
- Press `Esc` or left-click the tray icon again to cancel selection.
- Russian UI is selected for a Russian Windows UI language; English is the fallback.
- A normal exit unpins every window changed by the utility.

## Запуск / Run

1. Скачайте `PinAppBM.exe` из GitHub Releases или артефакта сборки.
2. Запустите файл. Установка и права администратора не нужны.
3. Найдите белую булавку в области уведомлений.

Download `PinAppBM.exe`, run it, and find the white pin in the notification area. No installation or administrator rights are required.

Windows SmartScreen может показать предупреждение «Неизвестный издатель», поскольку тестовые сборки не подписаны цифровым сертификатом. / Windows SmartScreen may display an “Unknown publisher” warning because development builds are unsigned.

## Сборка / Build

Требуются CMake 3.20+, MSVC с поддержкой C++20 и Windows SDK. Сторонних библиотек нет. Команды нужно запускать из **Developer PowerShell for Visual Studio** или **x64 Native Tools Command Prompt**, чтобы `cmake`, компилятор и SDK были доступны в `PATH`.

Requires CMake 3.20+, a C++20-capable MSVC toolchain, and the Windows SDK. There are no third-party libraries. Run the commands from **Developer PowerShell for Visual Studio** or an **x64 Native Tools Command Prompt** so that CMake, MSVC, and the SDK are on `PATH`.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix dist
```

Готовый файл появится в `dist/PinAppBM.exe`. Release использует статический MSVC runtime.

В Visual Studio 2022 также можно использовать генератор `"Visual Studio 17 2022" -A x64`; в Visual Studio 2026 — установленный генератор соответствующей версии. GitHub Actions проверяет проект на Visual Studio 2022.

## Настройки / Settings

- Автозапуск хранится в `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` и не требует повышения прав.
- Конфигурация диагностики хранится в `%LOCALAPPDATA%\PinAppBM\settings.ini`.
- Журнал `%LOCALAPPDATA%\PinAppBM\diagnostic.log` создаётся только при включённой диагностике и ротируется после 1 MiB.
- Список закреплённых окон никогда не сохраняется и сбрасывается после перезапуска.
- Если portable-файл перемещён, выключите и снова включите автозапуск, чтобы обновить путь.

## Ограничения / Limitations

- Pin App BM управляет конкретным верхнеуровневым окном (`HWND`), а не всеми окнами процесса.
- Порядок гарантируется только между окнами, управляемыми Pin App BM. Чужие topmost-окна могут располагаться выше.
- Для окон, запущенных с повышенными правами, Windows может запретить управление; приложение покажет предупреждение.
- UAC secure desktop и экран входа недоступны обычным приложениям.
- Эксклюзивный полноэкранный режим игры или видео может обходить Windows DWM. Обычные и borderless-окна поддерживаются.
- Окно остаётся на своём виртуальном рабочем столе.
- Окно, которое уже было topmost до закрепления, сохраняет исходное topmost-состояние после удаления из управления Pin App BM.
- После аварийного завершения Windows может не дать приложению снять topmost; используйте штатный пункт «Выход».

## Лицензия

[MIT](LICENSE)
