#include <iostream>
#include <string>

int main(int argc, char** argv) {
  if (argc > 1 && std::string(argv[1]) == "--help") {
    std::cout << "Usage: compiler [options] <input-file>\n"
              << "Options:\n"
              << "  --help        Show this help message\n"
              << "  -o <file>     Output file path\n"
              << "  -O            Enable optimizations\n";
    return 0;
  }

  std::cerr << "No input provided. Use --help for usage.\n";
  return 1;
}
