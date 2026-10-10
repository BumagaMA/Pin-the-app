#pragma once

#include <Windows.h>

class IconSet {
public:
    IconSet() = default;
    IconSet(const IconSet&) = delete;
    IconSet& operator=(const IconSet&) = delete;
    ~IconSet();

    [[nodiscard]] bool Create();
    void Destroy();

    [[nodiscard]] HICON Idle() const { return idle_; }
    [[nodiscard]] HICON Selecting() const { return selecting_; }
    [[nodiscard]] HICON Pinned() const { return pinned_; }
    [[nodiscard]] HCURSOR SelectionCursor() const { return selectionCursor_; }

private:
    HICON idle_ = nullptr;
    HICON selecting_ = nullptr;
    HICON pinned_ = nullptr;
    HCURSOR selectionCursor_ = nullptr;
};
