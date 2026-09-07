#include <cmrc/cmrc.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>

// The namespace is injected by the build for each variant (CMRC_TEST_NS).
// Expand it through an indirection layer so the integer/token macro argument
// is substituted before it reaches CMRC_DECLARE and the qualified call.
#ifndef CMRC_TEST_NS
#error "CMRC_TEST_NS must be defined"
#endif

#define CMRC_DECLARE_NS(ns) CMRC_DECLARE(ns)
#define CMRC_GET_FS(ns) ::cmrc::ns::get_filesystem

CMRC_DECLARE_NS(CMRC_TEST_NS);

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Invalid arguments passed\n";
    return 2;
  }
  std::cout << "Reading flower from " << argv[1] << '\n';
  std::ifstream flower_fs{argv[1], std::ios_base::binary};
  if (!flower_fs) {
    std::cerr << "Invalid filename passed: " << argv[1] << '\n';
    return 2;
  }

  using iter = std::istreambuf_iterator<char>;
  const auto fs_size = std::distance(iter(flower_fs), iter());
  flower_fs.seekg(0);

  // Time to first resolve: covers one-time static-init base64 decode (if any)
  // plus first index build.
  const auto t0 = std::chrono::steady_clock::now();
  auto fs = CMRC_GET_FS(CMRC_TEST_NS)();
  const auto t1 = std::chrono::steady_clock::now();
  auto flower_rc = fs.open("flower.jpg");
  const auto rc_size = std::distance(flower_rc.begin(), flower_rc.end());
  const auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(t1 -
                                                                            t0)
          .count();
  std::cout << "First-resolve time: " << elapsed_ms << " ms (resource "
            << rc_size << " bytes)\n";
  if (rc_size != fs_size) {
    std::cerr << "Flower file sizes do not match: FS == " << fs_size
              << ", RC == " << rc_size << "\n";
    return 1;
  }
  if (!std::equal(flower_rc.begin(), flower_rc.end(), iter(flower_fs))) {
    std::cerr << "Flower file contents do not match\n";
    return 1;
  }
  return 0;
}
