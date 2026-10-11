#pragma once
#include <cstdint>
#include <string>
#include <vector>

constexpr std::uint32_t kTranslationAddress = 0x06000000u;
// Private text tables are rebased only after every original field validates.
bool doaxbv_prepare_translation(std::vector<std::uint8_t>& guest,
                                std::vector<std::uint8_t>& text,
                                std::string& error);
