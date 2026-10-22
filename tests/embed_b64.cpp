#include <cmrc/cmrc.hpp>

#include <iostream>

CMRC_DECLARE(embed_b64);

int main() {
    auto fs = cmrc::embed_b64::get_filesystem();
    auto data = fs.open("hello.txt");
    std::cout << std::string(data.begin(), data.end()) << '\n';
}
