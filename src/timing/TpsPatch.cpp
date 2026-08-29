#include "TpsPatch.hpp"
#include "TpsBypass.hpp"
#include "PatchEncoding.hpp"

#include <asp/iter.hpp>
#include <array>
#include <bit>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <span>
#include <vector>

#if defined(GEODE_IS_ANDROID)
#include <dlfcn.h>
#endif

#if defined(GEODE_IS_IOS)
#include <mach-o/loader.h>
#endif

using namespace geode::prelude;
using namespace toasty::timing::encoding;

namespace {
#if defined(GEODE_IS_WINDOWS) || defined(GEODE_IS_ANDROID64)
    using ExpectedType = uint32_t;
#else
    using ExpectedType = float;
#endif

    alignas(8) ExpectedType s_expected = static_cast<ExpectedType>(1);
    ExpectedType* s_expectedTarget = &s_expected;
    Patch* s_expectedPatch = nullptr;
    bool s_initialized = false;
    bool s_available = false;
    bool s_staticPatch = false;
    std::string s_error;

#if defined(GEODE_IS_IOS)
    constexpr uintptr_t PatchlessExpectedStorage = 0x8b2000;
#endif

    struct Pattern {
        std::span<int16_t const> bytes;
        size_t patchOffset;
    };

    uintptr_t findPattern(uint8_t const* start, size_t size, Pattern pattern) {
        if (!start || pattern.bytes.empty() || size < pattern.bytes.size())
            return 0;

        for (size_t offset = 0; offset <= size - pattern.bytes.size(); ++offset) {
            bool matches = asp::iter::enumerate(pattern.bytes).all([&](auto pair) {
                auto [index, expected] = pair;
                return expected < 0 || start[offset + index] == static_cast<uint8_t>(expected);
            });
            if (matches)
                return reinterpret_cast<uintptr_t>(start + offset + pattern.patchOffset);
        }
        return 0;
    }

    Result<Patch*> createPatch(uintptr_t address, std::vector<uint8_t> bytes) {
        if (!address || bytes.empty())
            return Err("empty patch");
        return Mod::get()->patch(reinterpret_cast<void*>(address), std::move(bytes));
    }

#if defined(GEODE_IS_IOS)
    Result<std::pair<uint8_t const*, size_t>> textSegment() {
        auto header = reinterpret_cast<mach_header_64 const*>(geode::base::get());
        if (!header || header->magic != MH_MAGIC_64)
            return Err("Mach-O header was not found");

        auto command = reinterpret_cast<uint8_t const*>(header + 1);
        for (uint32_t index = 0; index < header->ncmds; ++index) {
            auto load = reinterpret_cast<load_command const*>(command);
            if (load->cmd == LC_SEGMENT_64) {
                auto segment = reinterpret_cast<segment_command_64 const*>(load);
                if (std::strncmp(segment->segname, "__TEXT", sizeof(segment->segname)) == 0) {
                    auto slide = geode::base::get() - 0x100000000;
                    return Ok(
                        std::make_pair(reinterpret_cast<uint8_t const*>(slide + segment->vmaddr),
                                       static_cast<size_t>(segment->vmsize)));
                }
            }
            command += load->cmdsize;
        }
        return Err("Mach-O __TEXT segment was not found");
    }
#endif

    Result<std::pair<uint8_t const*, size_t>> expectedSearchRegion() {
#if defined(GEODE_IS_WINDOWS)
        auto base = geode::base::get();
        if (!base)
            return Err("Geometry Dash module was not found");

        auto dos = reinterpret_cast<IMAGE_DOS_HEADER const*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            return Err("Geometry Dash DOS header was invalid");

        auto nt = reinterpret_cast<IMAGE_NT_HEADERS64 const*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.SizeOfImage == 0) {
            return Err("Geometry Dash PE header was invalid");
        }

        return Ok(std::make_pair(reinterpret_cast<uint8_t const*>(base),
                                 static_cast<size_t>(nt->OptionalHeader.SizeOfImage)));
#elif defined(GEODE_IS_ANDROID)
        auto function =
            reinterpret_cast<uint8_t const*>(dlsym(RTLD_DEFAULT, "_ZN15GJBaseGameLayer6updateEf"));
        if (!function)
            return Err("GJBaseGameLayer::update could not be resolved");
        return Ok(std::make_pair(function, size_t(0x500)));
#elif defined(GEODE_IS_IOS)
        return textSegment();
#else
        return Err("unsupported platform");
#endif
    }

    Result<Patch*> createExpectedPatch() {
        GEODE_UNWRAP_INTO(auto region, expectedSearchRegion());

        uintptr_t address = 0;
        std::vector<uint8_t> bytes;

#if defined(GEODE_IS_WINDOWS)
        static constexpr std::array<int16_t, 27> signature = {
            0xff, 0x90, -1,   -1,   -1, -1, 0xf3, 0x0f, 0x10, -1,   -1,   -1,   -1,  -1,
            0xf3, 0x44, 0x0f, 0x10, -1, -1, -1,   -1,   0x00, 0xf3, 0x41, 0x0f, 0x5d};
        address = findPattern(region.first, region.second, {signature, 6});
        if (!address)
            return Err("Windows 2.2081 tick signature was not found");
        bytes = {0x48, 0xb8};
        append64(bytes, reinterpret_cast<uintptr_t>(s_expectedTarget));
        bytes.insert(bytes.end(), {0x44, 0x8b, 0x18});
        appendRelativeJump(bytes, address, address + 0x43);
        bytes.insert(bytes.end(), 4, 0x90);
#elif defined(GEODE_IS_ANDROID64)
        static constexpr std::array<int16_t, 12> signature = {
            0xab, 0x19, 0x60, 0x1e, 0x0a, 0x10, 0x62, 0x1e, 0x6a, 0x09, 0x6a, 0x1e};
        address = findPattern(region.first, region.second, {signature, 0});
        if (!address)
            return Err("Android 64-bit 2.2081 tick signature was not found");
        appendArm64Address(bytes, reinterpret_cast<uintptr_t>(s_expectedTarget), 9);
        append32(bytes, 0xb9400120);
        append32(bytes, arm64Branch(address + bytes.size(), address + 0x24));
#elif defined(GEODE_IS_ANDROID32)
        static constexpr std::array<int16_t, 15> signature = {
            0xee, -1, -1, -1, 0xee, -1, -1, 0xf7, 0xee, -1, 0x7b, 0x17, 0xee, 0x90, 0x0a};
        address = findPattern(region.first, region.second, {signature, 7});
        if (!address)
            return Err("Android 32-bit 2.2081 tick signature was not found");
        auto target = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s_expectedTarget));
        appendThumbMove(bytes, 0xf240, 1, static_cast<uint16_t>(target));
        appendThumbMove(bytes, 0xf2c0, 1, static_cast<uint16_t>(target >> 16));
        bytes.insert(bytes.end(), {0x08, 0x68, 0x00, 0xbf});
#elif defined(GEODE_IS_IOS)
        static constexpr std::array<int16_t, 12> signature = {
            0x01, 0x10, 0x2e, 0x1e, 0x00, 0x20, 0x21, 0x1e, 0x20, 0xac, 0x20, 0x1e};
        address = findPattern(region.first, region.second, {signature, 0});
        if (!address)
            return Err("Apple ARM 2.2081 tick signature was not found");
        appendArm64Address(bytes, reinterpret_cast<uintptr_t>(s_expectedTarget), 9);
        append32(bytes, 0xbd400120);
        while (bytes.size() < 40)
            append32(bytes, 0xd503201f);
#else
        return Err("unsupported platform");
#endif

        return createPatch(address, std::move(bytes));
    }

    void fail(std::string message) {
        s_error = std::move(message);
        s_available = false;
        if (Mod::get()->getSavedValue<bool>("tps-bypass", false)) {
            Mod::get()->setSavedValue<bool>("tps-bypass", false);
        }
        log::error("TPS bypass unavailable: {}", s_error);
    }
} // namespace

namespace toasty::tps::patch {
    bool initialize() {
        if (s_initialized)
            return s_available;
        s_initialized = true;

#if defined(GEODE_IS_IOS)
        if (Loader::get()->isPatchless()) {
            static_assert(GEODE_COMP_GD_VERSION == 22081,
                          "Patchless iOS TPS bypass requires Geometry Dash 2.2081");
            GEODE_MOD_STATIC_PATCH(0x1fe724,
                                   {0xa9, 0x35, 0x00, 0x90, 0x20, 0x01, 0x40, 0xbd, 0x1f, 0x20,
                                    0x03, 0xd5, 0x1f, 0x20, 0x03, 0xd5, 0x1f, 0x20, 0x03, 0xd5,
                                    0x1f, 0x20, 0x03, 0xd5, 0x1f, 0x20, 0x03, 0xd5, 0x1f, 0x20,
                                    0x03, 0xd5, 0x1f, 0x20, 0x03, 0xd5, 0x1f, 0x20, 0x03, 0xd5});
            s_expectedTarget =
                reinterpret_cast<ExpectedType*>(geode::base::get() + PatchlessExpectedStorage);
            s_staticPatch = true;
            s_available = true;
            setExpected(1);
            return true;
        }
#endif

        auto expected = createExpectedPatch();
        if (!expected) {
            fail(expected.unwrapErr());
            return false;
        }
        s_expectedPatch = expected.unwrapOr(nullptr);

        s_available = true;
        if (!setEnabled(Mod::get()->getSavedValue<bool>("tps-bypass", false))) {
            return false;
        }
        return true;
    }

    bool available() {
        return s_available;
    }

    bool staticPatch() {
        return s_staticPatch;
    }

    bool interceptsTicks() {
        return s_staticPatch || (s_expectedPatch && s_expectedPatch->isEnabled());
    }

    bool setEnabled(bool enabled) {
        if (!s_available)
            return !enabled;
        if (s_staticPatch)
            return true;

        auto expectedResult = s_expectedPatch->toggle(enabled);
        if (!expectedResult) {
            fail(fmt::format("tick patch toggle failed: {}", expectedResult.unwrapErr()));
            return false;
        }

        return true;
    }

    void setExpected(uint32_t ticks) {
        *s_expectedTarget = static_cast<ExpectedType>(ticks);
    }

    std::string const& error() {
        return s_error;
    }
} // namespace toasty::tps::patch
