#pragma once

#include "config.h"
#include "icon_factory.h"
#include "localization.h"
#include "logger.h"
#include "pin_manager.h"
#include "selection_overlay.h"
#include "tray_activation.h"
#include "tray_icon.h"
#include "window_utils.h"

#include <Windows.h>

#include <atomic>
#include <vector>

class Application {
public:
    explicit Application(HINSTANCE instance);
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    ~Application();

    [[nodiscard]] int Run(int showCommand);

private:
    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK MouseHookProcedure(int code, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK KeyboardHookProcedure(int code, WPARAM wParam, LPARAM lParam);
    static void CALLBACK WinEventProcedure(
        HWINEVENTHOOK hook, DWORD event, HWND window, LONG objectId, LONG childId, DWORD threadId, DWORD time);

    [[nodiscard]] bool Initialize();
    void Shutdown();
    LRESULT HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    void BeginSelection();
    void EndSelection();
    void UpdateSelectionAtCursor();
    void CommitSelection();
    void UpdateTrayState();
    void ShowContextMenu(POINT point);
    void ExecuteMenuCommand(UINT command);
    void ApplyPinAction(HWND window, bool toggle);
    void ShowPinError(PinResult result);

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    UINT taskbarCreatedMessage_ = 0;
    bool initialized_ = false;
    bool shuttingDown_ = false;
    unsigned int trayRestoreAttempts_ = 0;
    bool sessionNotificationsRegistered_ = false;
    bool selecting_ = false;
    bool suppressMouseUp_ = false;
    bool suppressEscapeUp_ = false;
    bool cancelAfterMouseUp_ = false;
    RECT trayRectangle_{};
    bool hasTrayRectangle_ = false;
    HWND selectionTarget_ = nullptr;
    HHOOK mouseHook_ = nullptr;
    HHOOK keyboardHook_ = nullptr;
    HWINEVENTHOOK foregroundEventHook_ = nullptr;
    HWINEVENTHOOK destroyEventHook_ = nullptr;
    std::atomic<LONG> cursorX_{0};
    std::atomic<LONG> cursorY_{0};
    std::atomic<LONG> clickX_{0};
    std::atomic<LONG> clickY_{0};
    std::atomic<bool> moveMessagePending_{false};

    Strings strings_;
    Config config_;
    Logger logger_;
    IconSet icons_;
    TrayActivationFilter trayActivationFilter_;
    TrayIcon tray_;
    SelectionOverlay overlay_;
    PinManager pins_;
    std::vector<WindowIdentity> openMenuWindows_;
    std::vector<WindowIdentity> pinnedMenuWindows_;

    static Application* activeInstance_;
};
