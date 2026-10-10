#include "config.h"

#include <cstdlib>
#include <iostream>
#include <string>

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
    Expect(BuildStartupCommand(L"").empty(), "an empty executable path must be rejected");
    Expect(
        BuildStartupCommand(LR"(C:\Tools\Pin App BM\PinAppBM.exe)")
            == LR"("C:\Tools\Pin App BM\PinAppBM.exe" --autostart)",
        "startup command must quote paths containing spaces");
    Expect(BuildStartupCommand(std::wstring(260, L'a')).empty(), "overlong Run commands must be rejected");
    std::cout << "All configuration tests passed.\n";
    return EXIT_SUCCESS;
}
