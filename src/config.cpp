#include "config.h"

#include <Windows.h>
#include <ShlObj.h>

#include <system_error>

namespace {

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"Pin App BM";

std::filesystem::path GetApplicationDataDirectory()
{
    PWSTR rawPath = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &rawPath))) {
        return {};
    }

    const std::filesystem::path path(rawPath);
    CoTaskMemFree(rawPath);
    return path / L"PinAppBM";
}

std::wstring CurrentExecutablePath()
{
    std::wstring path(260, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0) {
            return {};
        }
        if (length < path.size() - 1) {
            path.resize(length);
            return path;
        }
        path.resize(path.size() * 2);
    }
}

} // namespace

Config::Config()
    : directory_(GetApplicationDataDirectory())
    , configPath_(directory_ / L"settings.ini")
    , logPath_(directory_ / L"diagnostic.log")
{
}

void Config::Load()
{
    if (configPath_.empty()) {
        diagnosticLogging_ = false;
        return;
    }

    diagnosticLogging_ = GetPrivateProfileIntW(L"settings", L"diagnostic_logging", 0, configPath_.c_str()) != 0;
}

bool Config::Save() const
{
    if (directory_.empty()) {
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(directory_, error);
    if (error) {
        return false;
    }

    return WritePrivateProfileStringW(
               L"settings",
               L"diagnostic_logging",
               diagnosticLogging_ ? L"1" : L"0",
               configPath_.c_str()) != FALSE;
}

std::wstring BuildStartupCommand(const std::wstring& executablePath)
{
    if (executablePath.empty()) {
        return {};
    }
    std::wstring command = L"\"" + executablePath + L"\" --autostart";
    return command.size() < 260 ? command : std::wstring{};
}

bool IsRunAtStartupEnabled()
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }

    const std::wstring expected = BuildStartupCommand(CurrentExecutablePath());
    if (expected.empty()) {
        RegCloseKey(key);
        return false;
    }
    DWORD type = 0;
    DWORD byteCount = 0;
    LONG result = RegGetValueW(key, nullptr, kRunValue, RRF_RT_REG_SZ, &type, nullptr, &byteCount);
    if (result != ERROR_SUCCESS || byteCount < sizeof(wchar_t)) {
        RegCloseKey(key);
        return false;
    }

    std::wstring value(byteCount / sizeof(wchar_t), L'\0');
    result = RegGetValueW(key, nullptr, kRunValue, RRF_RT_REG_SZ, &type, value.data(), &byteCount);
    RegCloseKey(key);
    if (result != ERROR_SUCCESS) {
        return false;
    }

    if (!value.empty() && value.back() == L'\0') {
        value.pop_back();
    }
    return value == expected;
}

bool SetRunAtStartupEnabled(bool enabled)
{
    HKEY key = nullptr;
    const LONG openResult = RegCreateKeyExW(
        HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (openResult != ERROR_SUCCESS) {
        return false;
    }

    LONG result = ERROR_SUCCESS;
    if (enabled) {
        const std::wstring executablePath = CurrentExecutablePath();
        const std::wstring command = BuildStartupCommand(executablePath);
        if (command.empty()) {
            RegCloseKey(key);
            return false;
        }
        result = RegSetValueExW(
            key,
            kRunValue,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(key, kRunValue);
        if (result == ERROR_FILE_NOT_FOUND) {
            result = ERROR_SUCCESS;
        }
    }

    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}
