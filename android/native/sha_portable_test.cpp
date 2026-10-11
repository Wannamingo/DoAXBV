#include "sha.h"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc, char **argv)
{
    using namespace doaxbv;
    std::vector<uint8_t> empty, abc = {'a','b','c'}, million(1000000, 'a');
    if (sha1Hex(empty) != "da39a3ee5e6b4b0d3255bfef95601890afd80709" ||
        sha1Hex(abc) != "a9993e364706816aba3e25717850c26c9cd0d89d" ||
        sha1Hex(million) != "34aa973cd4c4daa4f61eeb2bdbad27316534016f") return 1;
    if (sha256Hex(empty) != "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" ||
        sha256Hex(abc) != "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" ||
        sha256Hex(million) != "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") return 2;
    if (argc > 1) {
        std::ifstream file(argv[1], std::ios::binary);
        if (!file) return 3;
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), {});
        std::cout << sha1Hex(data) << '\n' << sha256Hex(data) << '\n';
    } else std::cout << "SHA-1/SHA-256 standard vectors passed.\n";
    return 0;
}
