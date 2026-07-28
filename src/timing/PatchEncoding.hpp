#pragma once

#include <cstdint>
#include <vector>

namespace toasty::timing::encoding {
    inline void append32(std::vector<uint8_t>& bytes, uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8) {
            bytes.push_back(static_cast<uint8_t>(value >> shift));
        }
    }

    inline void append64(std::vector<uint8_t>& bytes, uint64_t value) {
        for (int shift = 0; shift < 64; shift += 8) {
            bytes.push_back(static_cast<uint8_t>(value >> shift));
        }
    }

    inline void appendRelativeJump(
        std::vector<uint8_t>& bytes,
        uintptr_t patchAddress,
        uintptr_t destination
    ) {
        auto instruction = patchAddress + bytes.size();
        bytes.push_back(0xe9);
        auto relative = static_cast<int64_t>(destination) - static_cast<int64_t>(instruction + 5);
        append32(bytes, static_cast<uint32_t>(static_cast<int32_t>(relative)));
    }

    inline uint32_t arm64MoveWide(uint32_t base, uint8_t reg, uint16_t immediate, uint8_t shift) {
        return base |
            (static_cast<uint32_t>(shift / 16) << 21) |
            (static_cast<uint32_t>(immediate) << 5) |
            reg;
    }

    inline void appendArm64Address(std::vector<uint8_t>& bytes, uintptr_t address, uint8_t reg) {
        append32(bytes, arm64MoveWide(0xd2800000, reg, static_cast<uint16_t>(address), 0));
        append32(bytes, arm64MoveWide(0xf2800000, reg, static_cast<uint16_t>(address >> 16), 16));
        append32(bytes, arm64MoveWide(0xf2800000, reg, static_cast<uint16_t>(address >> 32), 32));
        append32(bytes, arm64MoveWide(0xf2800000, reg, static_cast<uint16_t>(address >> 48), 48));
    }

    inline uint32_t arm64Branch(uintptr_t instruction, uintptr_t destination) {
        auto displacement = static_cast<int64_t>(destination) - static_cast<int64_t>(instruction);
        return 0x14000000 | (static_cast<uint32_t>(displacement >> 2) & 0x03ffffff);
    }

    inline uint32_t arm64Adrp(uint8_t reg, uintptr_t instruction, uintptr_t target) {
        auto sourcePage = static_cast<int64_t>(instruction & ~uintptr_t(0xfff));
        auto targetPage = static_cast<int64_t>(target & ~uintptr_t(0xfff));
        auto pages = (targetPage - sourcePage) >> 12;
        auto immediate = static_cast<uint64_t>(pages) & 0x1fffff;
        return 0x90000000 |
            (static_cast<uint32_t>(immediate & 3) << 29) |
            (static_cast<uint32_t>((immediate >> 2) & 0x7ffff) << 5) |
            reg;
    }

    inline uint32_t arm64Load64(uint8_t reg, uint8_t baseReg, uint32_t byteOffset) {
        return 0xf9400000 |
            ((byteOffset / 8) << 10) |
            (static_cast<uint32_t>(baseReg) << 5) |
            reg;
    }

    inline uint32_t arm64LoadFloat(uint8_t reg, uint8_t baseReg, uint32_t byteOffset) {
        return 0xbd400000 |
            ((byteOffset / 4) << 10) |
            (static_cast<uint32_t>(baseReg) << 5) |
            reg;
    }

    inline void appendThumbMove(
        std::vector<uint8_t>& bytes,
        uint16_t base,
        uint8_t reg,
        uint16_t value
    ) {
        auto first = static_cast<uint16_t>(
            base |
            (((value >> 11) & 1) << 10) |
            ((value >> 12) & 0xf)
        );
        auto second = static_cast<uint16_t>(
            (((value >> 8) & 7) << 12) |
            (static_cast<uint16_t>(reg) << 8) |
            (value & 0xff)
        );
        bytes.push_back(static_cast<uint8_t>(first));
        bytes.push_back(static_cast<uint8_t>(first >> 8));
        bytes.push_back(static_cast<uint8_t>(second));
        bytes.push_back(static_cast<uint8_t>(second >> 8));
    }
}
