#include "localization.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "runtime.h"

namespace {
bool russian_enabled=false;
struct TranslationEntry {
    std::uint32_t field, original, offset;
    const char* expected;
    std::size_t expected_size;
};
#include "ru_catalog.inc"
#include "ru_font.inc"
constexpr std::uint32_t kRectAddress = kTranslationAddress + 0x10000u;
constexpr std::uint32_t kPairAddress = kTranslationAddress + 0x14000u;
constexpr std::uint32_t kTextureAddress = kTranslationAddress + 0x16000u;
constexpr std::uint32_t kPaletteAddress = kTranslationAddress + 0x16020u;
constexpr std::uint32_t kPaletteData = kTranslationAddress + 0x38000u;
constexpr std::uint32_t kPixelAddress = kTranslationAddress + 0x18000u;
// Header +24 is the complete texture view: 29 packed slots + 509 cache slots.
// Russian glyphs must sit beyond that cache, never in its first dynamic slot.
constexpr unsigned kOriginalGlyphs = 927, kOriginalTextures = 538;
// The sprite renderer uses 538 as its initial "no bound texture" sentinel.
constexpr unsigned kCyrillicTextureSlot = kOriginalTextures + 1u;
constexpr unsigned kAddedGlyphs = sizeof(kCyrillicGlyphs) / sizeof(kCyrillicGlyphs[0]);
static_assert(sizeof(kRussianText) <= 0x10000u);
static_assert((kOriginalGlyphs + kAddedGlyphs) * 12u <= 0x4000u);
static_assert((kCyrillicTextureSlot + 5u) * 8u <= 0x2000u);
static_assert(sizeof(kCyrillicAtlas) == 512u * 256u);
}

bool doaxbv_prepare_translation(std::vector<std::uint8_t>& guest,
                                std::vector<std::uint8_t>& text,
                                std::string& error) {
    russian_enabled=false;
    const char* language=std::getenv("RECOMP_LANGUAGE");
    if(language && std::strcmp(language,"en")==0){
        text.clear();
        error.clear();
        std::fprintf(stderr,"English localization: original text and font\n");
        return true;
    }
    for (const auto& e : kTranslations) {
        std::uint32_t original = 0;
        if (e.field > guest.size() - sizeof(original) ||
            e.original >= guest.size() ||
            e.expected_size > guest.size() - e.original) {
            error = "Russian text table is outside mapped image";
            return false;
        }
        std::memcpy(&original, guest.data() + e.field, sizeof(original));
        if (original != e.original ||
            std::memcmp(guest.data() + e.original, e.expected, e.expected_size) != 0) {
            error = "Russian text table identity mismatch";
            return false;
        }
    }
    // Keep the backing storage stable for the entire mapped runtime lifetime.
    text.assign(0x40000u, 0);
    std::memcpy(text.data(), kRussianText, sizeof(kRussianText));
    std::memcpy(text.data() + 0x18000u, kCyrillicAtlas, sizeof(kCyrillicAtlas));
    // Some cached text passes copy RGB without blending the alpha channel.
    // P8 retains zero RGB as well as zero alpha outside each letter.
    for (unsigned i=0; i<256; ++i) {
        const std::uint32_t color=i*0x01010101u;
        std::memcpy(text.data()+0x38000u+i*4u,&color,4u);
    }
    for (const auto& e : kTranslations) {
        const std::uint32_t translated = kTranslationAddress + e.offset;
        std::memcpy(guest.data() + e.field, &translated, sizeof(translated));
    }
    std::fprintf(stderr, "Russian localization: %zu verified fields, %zu text bytes\n",
                 sizeof(kTranslations) / sizeof(kTranslations[0]), sizeof(kRussianText));
    russian_enabled=true;
    return true;
}

extern "C" void sub_00096E10(void);
extern "C" void sub_00096E90(void);
static bool attach_cyrillic_font() {
    const std::uint32_t font = *recomp_memory_u32(0x00317764u);
    if (!font || font > RECOMP_XBOX_RAM_SIZE - 40u) return false;
    const std::uint32_t coords = *recomp_memory_u32(font + 8u);
    const std::uint32_t uploaded = *recomp_memory_u32(font + 4u);
    if (coords == kRectAddress && uploaded == kPairAddress) return true;
    if (!coords || !uploaded ||
        *recomp_memory_u32(font + 20u) != kOriginalGlyphs ||
        *recomp_memory_u32(font + 24u) != kOriginalTextures ||
        *recomp_memory_u32(font + 28u) != 29u) return false;
    // Extend the two views only. The game's original packed resources and
    // ownership counts stay intact, so its cleanup never releases our storage.
    recomp_guest_memcpy(kRectAddress, coords, kOriginalGlyphs * 12u);
    recomp_guest_memcpy(kPairAddress, uploaded, kOriginalTextures * 8u);
    for (unsigned i = 0; i < kAddedGlyphs; ++i) {
        const auto& g = kCyrillicGlyphs[i];
        const std::int16_t rect[] = {g.x0, g.y0, g.x1, g.y1};
        const std::uint32_t address = kRectAddress + (kOriginalGlyphs + i) * 12u;
        recomp_guest_store(address, rect, sizeof(rect));
        *recomp_memory_u32(address + 8u) = kCyrillicTextureSlot;
    }
    const std::uint32_t resource[] = {0x00040001u, kPixelAddress, 0u, 0x08910b2au, 0u};
    recomp_guest_store(kTextureAddress, resource, sizeof(resource));
    const std::uint32_t palette[] = {0x00830001u,kPaletteData,0u};
    recomp_guest_store(kPaletteAddress,palette,sizeof(palette));
    // Authenticated font-special-cache.txt: styles 4/5 temporarily add 2
    // (highlight) or 4 (shadow) to a glyph's texture slot before enqueueing.
    // Supply those companions too. Otherwise the extra pass binds a null
    // texture and fills the whole glyph rectangle with its diffuse color.
    // All three passes use the same coverage; the game's colors and blend
    // modes still control the highlight and shadow.
    for (const unsigned variant : {0u, 2u, 4u}) {
        const auto pair = kPairAddress + (kCyrillicTextureSlot + variant) * 8u;
        *recomp_memory_u32(pair) = kTextureAddress;
        *recomp_memory_u32(pair + 4u) = kPaletteAddress;
    }
    *recomp_memory_u32(font + 4u) = kPairAddress;
    *recomp_memory_u32(font + 8u) = kRectAddress;
    std::fprintf(stderr, "Russian font attached: %u added glyphs, original resources preserved\n", kAddedGlyphs);
    return true;
}

static bool russian_glyph(bool alternate) {
    auto& r = recomp_runtime.registers;
    const unsigned code = *recomp_memory_u32(r.esp + 4u) & 255u;
    if (code < 128u || (!alternate && *recomp_memory_u32(r.esp + 8u) != 0)) return false;
    for (unsigned i = 0; i < kAddedGlyphs; ++i) {
        if (kCyrillicGlyphs[i].code == code && attach_cyrillic_font()) {
            r.eax = kOriginalGlyphs + i;
            r.esp += 4u;
            return true;
        }
    }
    return false;
}
static void map_normal_font() {
    if (russian_glyph(false)) return;
    sub_00096E10();
}
static void map_alternate_font() {
    if (russian_glyph(true)) return;
    sub_00096E90();
}

// Authenticated XBE 492db2a8..., bounded glyph-index/font-load Ghidra queries:
// cdecl byte mapper, tail-call thunk and alternate font mapper. ASCII and
// controller icons retain the original generated leaf behavior.
extern "C" RecompFunction recomp_russian_lookup(std::uint32_t address) {
    if(!russian_enabled)return nullptr;
    if (address == 0x00096e10u || address == 0x00096e00u) return map_normal_font;
    if (address == 0x00096e90u) return map_alternate_font;
    return nullptr;
}
