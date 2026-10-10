#include "app.h"

#include <Windowsx.h>
#include <WtsApi32.h>

#include <algorithm>
#include <array>
#include <format>

namespace {

constexpr wchar_t kWindowClassName[] = L"PinAppBM.HiddenWindow";
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kSelectionMoveMessage = WM_APP + 2;
constexpr UINT kCommitSelectionMessage = WM_APP + 3;
constexpr UINT kCancelSelectionMessage = WM_APP + 4;
constexpr UINT kWindowEventMessage = WM_APP + 5;
constexpr UINT_PTR kSelectionTimer = 1;
constexpr UINT_PTR kHousekeepingTimer = 2;
constexpr UINT_PTR kTrayRestoreTimer = 3;
constexpr unsigned int kMaximumTrayRestoreAttempts = 5;

constexpr UINT kMenuSelect = 100;
constexpr UINT kMenuUnpinAll = 101;
constexpr UINT kMenuStartup = 102;
constexpr UINT kMenuDiagnostics = 103;
constexpr UINT kMenuAbout = 104;
constexpr UINT kMenuExit = 105;
constexpr UINT kOpenWindowMenuBase = 1000;
constexpr UINT kPinnedWindowMenuBase = 2000;

constexpr UINT kMenuTextFlags = MF_STRING;

} // namespace

Application* Application::activeInstance_ = nullptr;

Application::Application(HINSTANCE instance)
    : instance_(instance)
    , strings_(LoadStrings())
{
}

Application::~Application()
{
    Shutdown();
}

int Application::Run(int)
{
    if (!Initialize()) {
        Shutdown();
        return 1;
    }

    MSG message{};
    BOOL messageResult = 0;
    while ((messageResult = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return messageResult == -1 ? 1 : static_cast<int>(message.wParam);
}

bool Application::Initialize()
{
    activeInstance_ = this;
    config_.Load();
    logger_.Configure(config_.DiagnosticLogging(), config_.LogPath());
    logger_.Write(L"Application starting");

    taskbarCreatedMessage_ = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance_;
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.lpszClassName = kWindowClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(101));
    windowClass.hIconSm = windowClass.hIcon;
    if (RegisterClassExW(&windowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        logger_.Write(L"Failed to register the hidden window class");
        return false;
    }

    window_ = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        kWindowClassName,
        strings_.appName.c_str(),
        WS_POPUP,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        instance_,
        this);
    if (window_ == nullptr) {
        logger_.Write(L"Failed to create the hidden window");
        return false;
    }
    if (!icons_.Create()) {
        logger_.Write(L"Failed to create application icons");
        return false;
    }
    if (!overlay_.Create(instance_)) {
        logger_.Write(L"Failed to create selection overlays");
        return false;
    }

    if (!tray_.Add(window_, kTrayMessage, icons_.Idle(), strings_.tooltipIdle)) {
        logger_.Write(L"Failed to add the notification-area icon");
        return false;
    }

    foregroundEventHook_ = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,
        EVENT_SYSTEM_FOREGROUND,
        nullptr,
        WinEventProcedure,
        0,
        0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    destroyEventHook_ = SetWinEventHook(
        EVENT_OBJECT_DESTROY,
        EVENT_OBJECT_DESTROY,
        nullptr,
        WinEventProcedure,
        0,
        0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    sessionNotificationsRegistered_ = WTSRegisterSessionNotification(window_, NOTIFY_FOR_THIS_SESSION) != FALSE;

    SetTimer(window_, kHousekeepingTimer, 1000, nullptr);
    initialized_ = true;
    logger_.Write(L"Application initialized");
    return true;
}

void Application::Shutdown()
{
    if (shuttingDown_) {
        return;
    }
    shuttingDown_ = true;
    logger_.Write(L"Application shutting down");

    EndSelection();
    (void)pins_.UnpinAll();

    if (foregroundEventHook_ != nullptr) {
        UnhookWinEvent(foregroundEventHook_);
        foregroundEventHook_ = nullptr;
    }
    if (destroyEventHook_ != nullptr) {
        UnhookWinEvent(destroyEventHook_);
        destroyEventHook_ = nullptr;
    }
    if (window_ != nullptr) {
        KillTimer(window_, kHousekeepingTimer);
        KillTimer(window_, kTrayRestoreTimer);
        if (sessionNotificationsRegistered_) {
            WTSUnRegisterSessionNotification(window_);
            sessionNotificationsRegistered_ = false;
        }
    }

    overlay_.Destroy();
    tray_.Remove();
    icons_.Destroy();
    initialized_ = false;
    activeInstance_ = nullptr;

    if (window_ != nullptr) {
        const HWND window = window_;
        window_ = nullptr;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        if (IsWindow(window)) {
            DestroyWindow(window);
        }
    }
}

LRESULT CALLBACK Application::WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    Application* application = reinterpret_cast<Application*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        application = static_cast<Application*>(create->lpCreateParams);
        application->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(application));
    }

    if (application != nullptr) {
        return application->HandleMessage(window, message, wParam, lParam);
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT Application::HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == taskbarCreatedMessage_ && taskbarCreatedMessage_ != 0) {
        KillTimer(window, kTrayRestoreTimer);
        trayRestoreAttempts_ = 0;
        if (tray_.ReAdd()) {
            UpdateTrayState();
        } else {
            SetTimer(window, kTrayRestoreTimer, 1000, nullptr);
            logger_.Write(L"Notification-area icon restore scheduled for retry");
        }
        return 0;
    }

    switch (message) {
    case kTrayMessage: {
        const UINT event = LOWORD(lParam);
        if (trayActivationFilter_.ShouldActivate(
                event, static_cast<DWORD>(GetMessageTime()), GetDoubleClickTime())) {
            if (selecting_) {
                EndSelection();
            } else {
                BeginSelection();
            }
            return 0;
        }
        if (event == WM_CONTEXTMENU || event == WM_RBUTTONUP) {
            EndSelection();
            POINT point{GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam)};
            if (!tray_.UsesVersion4() || event != WM_CONTEXTMENU || (point.x == -1 && point.y == -1)) {
                GetCursorPos(&point);
            }
            ShowContextMenu(point);
            return 0;
        }
        break;
    }
    case kSelectionMoveMessage:
        moveMessagePending_.store(false);
        UpdateSelectionAtCursor();
        return 0;
    case kCommitSelectionMessage:
        CommitSelection();
        return 0;
    case kCancelSelectionMessage:
        if (suppressMouseUp_) {
            cancelAfterMouseUp_ = true;
        } else {
            EndSelection();
        }
        return 0;
    case kWindowEventMessage: {
        const DWORD event = static_cast<DWORD>(wParam);
        const HWND eventWindow = reinterpret_cast<HWND>(lParam);
        if (event == EVENT_OBJECT_DESTROY) {
            if (pins_.ForgetDestroyed(eventWindow)) {
                UpdateTrayState();
            }
        } else if (event == EVENT_SYSTEM_FOREGROUND && pins_.IsPinned(eventWindow)) {
            pins_.ReassertOrder();
        }
        return 0;
    }
    case WM_TIMER:
        if (wParam == kSelectionTimer && selecting_) {
            UpdateSelectionAtCursor();
        } else if (wParam == kHousekeepingTimer) {
            if (pins_.CleanupClosed()) {
                UpdateTrayState();
            }
            if (foregroundEventHook_ == nullptr && !pins_.Empty()) {
                pins_.ReassertOrder();
            }
        } else if (wParam == kTrayRestoreTimer) {
            ++trayRestoreAttempts_;
            if (tray_.ReAdd()) {
                KillTimer(window, kTrayRestoreTimer);
                trayRestoreAttempts_ = 0;
                UpdateTrayState();
                logger_.Write(L"Notification-area icon restored");
            } else if (trayRestoreAttempts_ >= kMaximumTrayRestoreAttempts) {
                KillTimer(window, kTrayRestoreTimer);
                logger_.Write(L"Notification-area icon restore failed after retries");
            }
        }
        return 0;
    case WM_QUERYENDSESSION:
        return TRUE;
    case WM_ENDSESSION:
        if (wParam != FALSE) {
            EndSelection();
            (void)pins_.UnpinAll();
        }
        return 0;
    case WM_WTSSESSION_CHANGE:
        if (wParam == WTS_SESSION_LOCK || wParam == WTS_SESSION_LOGOFF) {
            EndSelection();
        }
        return 0;
    case WM_DISPLAYCHANGE:
        if (selecting_) {
            UpdateSelectionAtCursor();
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        if (sessionNotificationsRegistered_) {
            WTSUnRegisterSessionNotification(window);
            sessionNotificationsRegistered_ = false;
        }
        window_ = nullptr;
        Shutdown();
        PostQuitMessage(0);
        return 0;
    case WM_NCDESTROY:
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        return DefWindowProcW(window, message, wParam, lParam);
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void Application::BeginSelection()
{
    if (selecting_ || shuttingDown_) {
        return;
    }

    hasTrayRectangle_ = tray_.GetRectangle(trayRectangle_);
    selecting_ = true;
    activeInstance_ = this;
    mouseHook_ = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProcedure, instance_, 0);
    keyboardHook_ = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProcedure, instance_, 0);
    if (mouseHook_ == nullptr || keyboardHook_ == nullptr) {
        EndSelection();
        MessageBoxW(window_, strings_.hookError.c_str(), strings_.operationErrorTitle.c_str(), MB_OK | MB_ICONERROR);
        return;
    }

    SetTimer(window_, kSelectionTimer, 33, nullptr);
    UpdateTrayState();
    UpdateSelectionAtCursor();
    logger_.Write(L"Selection mode enabled");
}

void Application::EndSelection()
{
    if (mouseHook_ != nullptr) {
        UnhookWindowsHookEx(mouseHook_);
        mouseHook_ = nullptr;
    }
    if (keyboardHook_ != nullptr) {
        UnhookWindowsHookEx(keyboardHook_);
        keyboardHook_ = nullptr;
    }
    if (window_ != nullptr) {
        KillTimer(window_, kSelectionTimer);
    }

    const bool wasSelecting = selecting_;
    selecting_ = false;
    suppressMouseUp_ = false;
    suppressEscapeUp_ = false;
    cancelAfterMouseUp_ = false;
    hasTrayRectangle_ = false;
    selectionTarget_ = nullptr;
    moveMessagePending_.store(false);
    overlay_.Hide();
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    if (!shuttingDown_) {
        UpdateTrayState();
    }
    if (wasSelecting) {
        logger_.Write(L"Selection mode disabled");
    }
}

void Application::UpdateSelectionAtCursor()
{
    if (!selecting_) {
        return;
    }

    POINT point{};
    if (!GetCursorPos(&point)) {
        overlay_.Hide();
        selectionTarget_ = nullptr;
        return;
    }
    cursorX_.store(point.x);
    cursorY_.store(point.y);
    SetCursor(icons_.SelectionCursor());

    selectionTarget_ = SelectableWindowAtPoint(point, GetCurrentProcessId());
    if (selectionTarget_ != nullptr) {
        overlay_.ShowFor(selectionTarget_);
    } else {
        overlay_.Hide();
    }
    overlay_.ShowCursor(point, icons_.Selecting());
}

void Application::CommitSelection()
{
    if (!selecting_) {
        return;
    }

    const POINT point{clickX_.load(), clickY_.load()};
    HWND target = SelectableWindowAtPoint(point, GetCurrentProcessId());
    EndSelection();
    if (target != nullptr) {
        ApplyPinAction(target, true);
    }
}

void Application::UpdateTrayState()
{
    if (!initialized_ && !selecting_) {
        return;
    }

    bool updated = false;
    if (selecting_) {
        updated = tray_.Update(icons_.Selecting(), strings_.tooltipSelecting);
    } else if (!pins_.Empty()) {
        updated = tray_.Update(icons_.Pinned(), strings_.tooltipPinned);
    } else {
        updated = tray_.Update(icons_.Idle(), strings_.tooltipIdle);
    }
    if (!updated) {
        logger_.Write(L"Failed to update the notification-area icon");
    }
}

void Application::ShowContextMenu(POINT point)
{
    (void)pins_.CleanupClosed();
    openMenuWindows_.clear();
    pinnedMenuWindows_.clear();

    HMENU menu = CreatePopupMenu();
    HMENU openWindowsMenu = CreatePopupMenu();
    HMENU pinnedWindowsMenu = CreatePopupMenu();
    if (menu == nullptr || openWindowsMenu == nullptr || pinnedWindowsMenu == nullptr) {
        if (menu != nullptr) {
            DestroyMenu(menu);
        }
        if (openWindowsMenu != nullptr) {
            DestroyMenu(openWindowsMenu);
        }
        if (pinnedWindowsMenu != nullptr) {
            DestroyMenu(pinnedWindowsMenu);
        }
        return;
    }

    AppendMenuW(menu, kMenuTextFlags, kMenuSelect, strings_.selectWithPin.c_str());

    const auto openWindows = EnumerateSelectableWindows(GetCurrentProcessId());
    for (const WindowInfo& info : openWindows) {
        const UINT command = kOpenWindowMenuBase + static_cast<UINT>(openMenuWindows_.size());
        UINT flags = MF_STRING;
        if (pins_.IsPinned(info.identity.handle)) {
            flags |= MF_CHECKED | MF_GRAYED;
        }
        const std::wstring label = FormatWindowLabel(info);
        AppendMenuW(openWindowsMenu, flags, command, label.c_str());
        openMenuWindows_.push_back(info.identity);
    }
    if (openMenuWindows_.empty()) {
        AppendMenuW(openWindowsMenu, MF_STRING | MF_GRAYED, 0, strings_.noWindows.c_str());
    }
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(openWindowsMenu), strings_.openWindows.c_str());

    const auto& pinned = pins_.Windows();
    for (auto iterator = pinned.rbegin(); iterator != pinned.rend(); ++iterator) {
        const HWND window = *iterator;
        WindowIdentity identity;
        if (!CaptureWindowIdentity(window, identity)) {
            continue;
        }
        WindowInfo info{identity, GetWindowTitle(window), GetWindowProcessName(window)};
        const UINT command = kPinnedWindowMenuBase + static_cast<UINT>(pinnedMenuWindows_.size());
        const std::wstring label = FormatWindowLabel(info);
        AppendMenuW(pinnedWindowsMenu, MF_STRING | MF_CHECKED, command, label.c_str());
        pinnedMenuWindows_.push_back(identity);
    }
    if (pinnedMenuWindows_.empty()) {
        AppendMenuW(pinnedWindowsMenu, MF_STRING | MF_GRAYED, 0, strings_.noPinnedWindows.c_str());
    }
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(pinnedWindowsMenu), strings_.pinnedWindows.c_str());
    AppendMenuW(menu, MF_STRING | (pins_.Empty() ? MF_GRAYED : 0), kMenuUnpinAll, strings_.unpinAll.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(
        menu,
        MF_STRING | (IsRunAtStartupEnabled() ? MF_CHECKED : 0),
        kMenuStartup,
        strings_.runAtStartup.c_str());
    AppendMenuW(
        menu,
        MF_STRING | (config_.DiagnosticLogging() ? MF_CHECKED : 0),
        kMenuDiagnostics,
        strings_.diagnosticLogging.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuAbout, strings_.about.c_str());
    AppendMenuW(menu, MF_STRING, kMenuExit, strings_.exit.c_str());

    SetForegroundWindow(window_);
    const UINT command = TrackPopupMenuEx(
        menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, point.x, point.y, window_, nullptr);
    DestroyMenu(menu);
    PostMessageW(window_, WM_NULL, 0, 0);
    tray_.SetFocus();
    if (command != 0) {
        ExecuteMenuCommand(command);
    }
}

void Application::ExecuteMenuCommand(UINT command)
{
    if (command >= kOpenWindowMenuBase && command < kOpenWindowMenuBase + openMenuWindows_.size()) {
        const WindowIdentity& identity = openMenuWindows_[command - kOpenWindowMenuBase];
        if (MatchesWindowIdentity(identity)) {
            ApplyPinAction(identity.handle, false);
        }
        return;
    }
    if (command >= kPinnedWindowMenuBase && command < kPinnedWindowMenuBase + pinnedMenuWindows_.size()) {
        const WindowIdentity& identity = pinnedMenuWindows_[command - kPinnedWindowMenuBase];
        if (MatchesWindowIdentity(identity)) {
            ApplyPinAction(identity.handle, true);
        }
        return;
    }

    switch (command) {
    case kMenuSelect:
        BeginSelection();
        break;
    case kMenuUnpinAll:
        if (const PinResult result = pins_.UnpinAll(); result != PinResult::Success) {
            ShowPinError(result);
        } else {
            UpdateTrayState();
            logger_.Write(L"All managed windows unpinned");
        }
        break;
    case kMenuStartup:
        if (!SetRunAtStartupEnabled(!IsRunAtStartupEnabled())) {
            MessageBoxW(window_, strings_.startupError.c_str(), strings_.operationErrorTitle.c_str(), MB_OK | MB_ICONERROR);
        }
        break;
    case kMenuDiagnostics: {
        config_.SetDiagnosticLogging(!config_.DiagnosticLogging());
        if (!config_.Save()) {
            config_.SetDiagnosticLogging(!config_.DiagnosticLogging());
            MessageBoxW(window_, strings_.operationError.c_str(), strings_.operationErrorTitle.c_str(), MB_OK | MB_ICONERROR);
            break;
        }
        logger_.Configure(config_.DiagnosticLogging(), config_.LogPath());
        logger_.Write(L"Diagnostic logging enabled");
        break;
    }
    case kMenuAbout:
        MessageBoxW(window_, strings_.aboutText.c_str(), strings_.appName.c_str(), MB_OK | MB_ICONINFORMATION);
        break;
    case kMenuExit:
        DestroyWindow(window_);
        break;
    default:
        break;
    }
}

void Application::ApplyPinAction(HWND window, bool toggle)
{
    const bool willPin = !pins_.IsPinned(window);
    if (willPin && IsWindowHigherIntegrity(window)) {
        ShowPinError(PinResult::AccessDenied);
        return;
    }
    const PinResult result = toggle ? pins_.Toggle(window) : pins_.Pin(window);
    if (result == PinResult::Success) {
        UpdateTrayState();
        logger_.Write(pins_.IsPinned(window) ? L"A window was pinned" : L"A window was unpinned");
    } else {
        ShowPinError(result);
    }
}

void Application::ShowPinError(PinResult result)
{
    if (result == PinResult::InvalidWindow) {
        (void)pins_.CleanupClosed();
        UpdateTrayState();
        return;
    }

    const bool accessDenied = result == PinResult::AccessDenied;
    MessageBoxW(
        window_,
        accessDenied ? strings_.accessWarning.c_str() : strings_.operationError.c_str(),
        accessDenied ? strings_.accessWarningTitle.c_str() : strings_.operationErrorTitle.c_str(),
        MB_OK | (accessDenied ? MB_ICONWARNING : MB_ICONERROR));
    logger_.Write(accessDenied ? L"Pin operation denied" : L"Pin operation failed");
}

LRESULT CALLBACK Application::MouseHookProcedure(int code, WPARAM wParam, LPARAM lParam)
{
    Application* application = activeInstance_;
    if (code != HC_ACTION || application == nullptr || !application->selecting_) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const auto* mouse = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
    if (wParam == WM_MOUSEMOVE) {
        if (application->suppressMouseUp_) {
            return 1;
        }
        application->cursorX_.store(mouse->pt.x);
        application->cursorY_.store(mouse->pt.y);
        SetCursor(application->icons_.SelectionCursor());
        if (!application->moveMessagePending_.exchange(true)) {
            PostMessageW(application->window_, kSelectionMoveMessage, 0, 0);
        }
    } else if (wParam == WM_LBUTTONDOWN) {
        if (application->hasTrayRectangle_ && PtInRect(&application->trayRectangle_, mouse->pt) != FALSE) {
            return CallNextHookEx(nullptr, code, wParam, lParam);
        }
        application->clickX_.store(mouse->pt.x);
        application->clickY_.store(mouse->pt.y);
        application->suppressMouseUp_ = true;
        return 1;
    } else if (wParam == WM_LBUTTONUP && application->suppressMouseUp_) {
        application->suppressMouseUp_ = false;
        const UINT message = application->cancelAfterMouseUp_ ? kCancelSelectionMessage : kCommitSelectionMessage;
        application->cancelAfterMouseUp_ = false;
        PostMessageW(application->window_, message, 0, 0);
        return 1;
    }

    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK Application::KeyboardHookProcedure(int code, WPARAM wParam, LPARAM lParam)
{
    Application* application = activeInstance_;
    if (code != HC_ACTION || application == nullptr || !application->selecting_) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const auto* keyboard = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
    if (keyboard->vkCode == VK_ESCAPE && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        application->suppressEscapeUp_ = true;
        return 1;
    }
    if (keyboard->vkCode == VK_ESCAPE && (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
        && application->suppressEscapeUp_) {
        application->suppressEscapeUp_ = false;
        if (application->suppressMouseUp_) {
            application->cancelAfterMouseUp_ = true;
        } else {
            PostMessageW(application->window_, kCancelSelectionMessage, 0, 0);
        }
        return 1;
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

void CALLBACK Application::WinEventProcedure(
    HWINEVENTHOOK, DWORD event, HWND window, LONG objectId, LONG childId, DWORD, DWORD)
{
    Application* application = activeInstance_;
    if (application == nullptr || application->window_ == nullptr || window == nullptr || childId != CHILDID_SELF) {
        return;
    }
    if (event == EVENT_OBJECT_DESTROY && objectId != OBJID_WINDOW) {
        return;
    }
    PostMessageW(application->window_, kWindowEventMessage, event, reinterpret_cast<LPARAM>(window));
}
