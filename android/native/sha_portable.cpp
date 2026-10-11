#include "sha.h"
#include <array>
#include <limits>
#include <stdexcept>

namespace doaxbv {
namespace {
uint32_t right(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }
uint32_t left(uint32_t x, unsigned n) { return (x << n) | (x >> (32 - n)); }
std::vector<uint8_t> padded(const uint8_t *data, size_t size)
{
    if (size > std::numeric_limits<size_t>::max() - 73 || size > UINT64_MAX / 8)
        throw std::length_error("hash input too large");
    std::vector<uint8_t> message((size + 9 + 63) / 64 * 64, 0);
    if (size) std::copy(data, data + size, message.begin());
    message[size] = 0x80;
    uint64_t bits = uint64_t(size) * 8;
    for (unsigned i = 0; i < 8; ++i) message[message.size() - 1 - i] = uint8_t(bits >> (8 * i));
    return message;
}
uint32_t word(const uint8_t *p)
{
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}
template<size_t N> std::string hex(const std::array<uint32_t, N> &state)
{
    const char digits[] = "0123456789abcdef";
    std::string out; out.reserve(N * 8);
    for (auto v : state) for (int shift = 28; shift >= 0; shift -= 4) out += digits[(v >> shift) & 15];
    return out;
}
}
std::string sha1Hex(const uint8_t *data, size_t size)
{
    auto message = padded(data, size);
    std::array<uint32_t, 5> h = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u, 0xc3d2e1f0u};
    for (size_t offset = 0; offset < message.size(); offset += 64) {
        uint32_t w[80];
        for (unsigned i = 0; i < 16; ++i) w[i] = word(message.data() + offset + i * 4);
        for (unsigned i = 16; i < 80; ++i) w[i] = left(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (unsigned i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5a827999u; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ed9eba1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8f1bbcdcu; }
            else { f = b ^ c ^ d; k = 0xca62c1d6u; }
            uint32_t next = left(a, 5) + f + e + k + w[i];
            e = d; d = c; c = left(b, 30); b = a; a = next;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    return hex(h);
}
std::string sha1Hex(const std::vector<uint8_t> &data) { return sha1Hex(data.data(), data.size()); }
std::string sha256Hex(const std::vector<uint8_t> &data)
{
    static const uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    std::array<uint32_t, 8> h = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    auto message = padded(data.data(), data.size());
    for (size_t offset = 0; offset < message.size(); offset += 64) {
        uint32_t w[64];
        for (unsigned i = 0; i < 16; ++i) w[i] = word(message.data() + offset + i * 4);
        for (unsigned i = 16; i < 64; ++i) {
            auto x = w[i - 15], y = w[i - 2];
            w[i] = w[i - 16] + (right(x, 7) ^ right(x, 18) ^ (x >> 3)) + w[i - 7] + (right(y, 17) ^ right(y, 19) ^ (y >> 10));
        }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];
        for (unsigned i = 0; i < 64; ++i) {
            uint32_t t1 = v + (right(e, 6) ^ right(e, 11) ^ right(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
            uint32_t t2 = (right(a, 2) ^ right(a, 13) ^ right(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            v=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=v;
    }
    return hex(h);
}
}
