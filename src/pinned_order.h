#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

// Stores items from the oldest pin to the newest pin. Keeping this container
// independent from Win32 makes the ordering policy easy to test.
template <typename T>
class PinnedOrder {
public:
    [[nodiscard]] bool Contains(const T& value) const
    {
        return std::find(items_.begin(), items_.end(), value) != items_.end();
    }

    void Pin(const T& value)
    {
        (void)Unpin(value);
        items_.push_back(value);
    }

    [[nodiscard]] bool Unpin(const T& value)
    {
        const auto iterator = std::find(items_.begin(), items_.end(), value);
        if (iterator == items_.end()) {
            return false;
        }

        items_.erase(iterator);
        return true;
    }

    template <typename Predicate>
    std::size_t RemoveIf(Predicate predicate)
    {
        const auto oldSize = items_.size();
        std::erase_if(items_, predicate);
        return oldSize - items_.size();
    }

    void Clear() { items_.clear(); }

    [[nodiscard]] bool Empty() const { return items_.empty(); }
    [[nodiscard]] std::size_t Size() const { return items_.size(); }
    [[nodiscard]] const std::vector<T>& Items() const { return items_; }

private:
    std::vector<T> items_;
};
