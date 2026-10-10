#include "logger.h"

#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <system_error>

namespace {

constexpr std::uintmax_t kMaximumLogSize = 1024 * 1024;

} // namespace

void Logger::Configure(bool enabled, const std::filesystem::path& path)
{
    std::scoped_lock lock(mutex_);
    enabled_ = enabled;
    path_ = path;
}

void Logger::RotateIfNeeded()
{
    std::error_code error;
    if (!std::filesystem::exists(path_, error) || std::filesystem::file_size(path_, error) < kMaximumLogSize) {
        return;
    }

    std::filesystem::path oldPath = path_;
    oldPath += L".old";
    std::filesystem::remove(oldPath, error);
    error.clear();
    std::filesystem::rename(path_, oldPath, error);
}

void Logger::Write(std::wstring_view message)
{
    std::scoped_lock lock(mutex_);
    if (!enabled_ || path_.empty()) {
        return;
    }

    std::error_code error;
    std::filesystem::create_directories(path_.parent_path(), error);
    if (error) {
        return;
    }

    RotateIfNeeded();

    SYSTEMTIME time{};
    GetLocalTime(&time);
    std::wofstream output(path_, std::ios::app);
    if (!output) {
        return;
    }

    output << std::format(
                  L"{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03} ",
                  time.wYear,
                  time.wMonth,
                  time.wDay,
                  time.wHour,
                  time.wMinute,
                  time.wSecond,
                  time.wMilliseconds)
           << message << L'\n';
}
