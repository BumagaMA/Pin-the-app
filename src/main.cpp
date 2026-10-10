#include "app.h"

#include <Windows.h>
#include <objbase.h>

namespace {

constexpr wchar_t kMutexName[] = L"Local\\PinAppBM-{E3A60B64-86E5-4B21-8E56-F844091111D1}";

} // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (mutex == nullptr) {
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        return 0;
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    Application application(instance);
    const int result = application.Run(showCommand);

    if (SUCCEEDED(comResult)) {
        CoUninitialize();
    }
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return result;
}
