#include "pinned_order.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void Expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

} // namespace

int main()
{
    PinnedOrder<int> order;
    Expect(order.Empty(), "new order must be empty");

    order.Pin(10);
    order.Pin(20);
    order.Pin(30);
    Expect(order.Items() == std::vector<int>({10, 20, 30}), "newest pin must be last");

    order.Pin(20);
    Expect(order.Items() == std::vector<int>({10, 30, 20}), "re-pinning moves an item to the top");

    Expect(order.Unpin(30), "existing item should be removed");
    Expect(!order.Unpin(99), "missing item should not be reported as removed");
    Expect(order.Items() == std::vector<int>({10, 20}), "unpin keeps the remaining order");

    const auto removed = order.RemoveIf([](int value) { return value < 15; });
    Expect(removed == 1, "RemoveIf should report the removed count");
    Expect(order.Items() == std::vector<int>({20}), "RemoveIf should retain valid items");

    order.Clear();
    Expect(order.Empty(), "Clear should remove every item");

    std::cout << "All pin order tests passed.\n";
    return EXIT_SUCCESS;
}
