#pragma once

#include <filesystem>
#include <string>

class Config {
public:
    Config();

    void Load();
    [[nodiscard]] bool Save() const;

    [[nodiscard]] bool DiagnosticLogging() const { return diagnosticLogging_; }
    void SetDiagnosticLogging(bool enabled) { diagnosticLogging_ = enabled; }

    [[nodiscard]] const std::filesystem::path& Directory() const { return directory_; }
    [[nodiscard]] const std::filesystem::path& LogPath() const { return logPath_; }

private:
    std::filesystem::path directory_;
    std::filesystem::path configPath_;
    std::filesystem::path logPath_;
    bool diagnosticLogging_ = false;
};

[[nodiscard]] bool IsRunAtStartupEnabled();
[[nodiscard]] bool SetRunAtStartupEnabled(bool enabled);
[[nodiscard]] std::wstring BuildStartupCommand(const std::wstring& executablePath);
