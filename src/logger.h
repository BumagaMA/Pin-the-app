#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

class Logger {
public:
    void Configure(bool enabled, const std::filesystem::path& path);
    void Write(std::wstring_view message);
    [[nodiscard]] bool Enabled() const { return enabled_; }

private:
    void RotateIfNeeded();

    bool enabled_ = false;
    std::filesystem::path path_;
    std::mutex mutex_;
};
