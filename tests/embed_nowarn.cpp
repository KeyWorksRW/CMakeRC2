#include <cmrc/cmrc.hpp>

#include <iostream>

CMRC_DECLARE(embed_nowarn);

int main() {
    auto fs = cmrc::embed_nowarn::get_filesystem();
    auto data = fs.open("hello.txt");
    std::cout << std::string(data.begin(), data.end()) << '\n';
}
