#include <cmrc/cmrc.hpp>

#include <iostream>
#include <string>

CMRC_DECLARE(header_regen);

int main() {
    auto fs = cmrc::header_regen::get_filesystem();
    auto data = fs.open("resource.txt");
    std::cout << std::string(data.begin(), data.end()) << '\n';
    return 0;
}