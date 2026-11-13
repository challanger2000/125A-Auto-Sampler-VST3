#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace a125 {
using StereoFrame = std::array<float, 2>;

struct TrimSettings {
    float silenceThreshold = 0.001f; // Linear amplitude, approx -60 dBFS
    std::size_t preFrames = 32;
    std::size_t postFrames = 256;
    std::size_t fadeFrames = 64;
};

// Offline export processing. Preserve the original velocities and relative levels;
// normalization is intentionally not applied automatically.
inline std::vector<StereoFrame> trimAndFade(
        std::span<const StereoFrame> audio, const TrimSettings& settings) {
    if (settings.silenceThreshold < 0.f || settings.silenceThreshold > 1.f)
        throw std::invalid_argument("invalid silence threshold");
    std::size_t first = audio.size();
    std::size_t last = 0;
    for (std::size_t i = 0; i < audio.size(); ++i) {
        if (std::max(std::abs(audio[i][0]), std::abs(audio[i][1])) >
            settings.silenceThreshold) {
            first = std::min(first, i);
            last = i;
        }
    }
    if (first == audio.size()) return {}; // Silence: do not emit phantom WAV
    const auto begin = first > settings.preFrames ? first - settings.preFrames : 0;
    const auto tail = audio.size() - (last + 1);
    const auto end = last + 1 + std::min(tail, settings.postFrames);
    std::vector<StereoFrame> result(audio.begin() + begin, audio.begin() + end);
    const auto nfade = std::min(settings.fadeFrames, result.size() / 2);
    for (std::size_t i = 0; i < nfade; ++i) {
        const float gainIn = static_cast<float>(i) / static_cast<float>(nfade);
        const float gainOut = static_cast<float>(i) / static_cast<float>(nfade);
        for (int ch = 0; ch < 2; ++ch) {
            result[i][ch] *= gainIn;
            result[result.size() - 1 - i][ch] *= gainOut;
        }
    }
    return result;
}

inline void writeLE16(std::ostream& out, std::uint16_t value) {
    out.put(static_cast<char>(value & 255));
    out.put(static_cast<char>((value >> 8) & 255));
}
inline void writeLE32(std::ostream& out, std::uint32_t value) {
    writeLE16(out, static_cast<std::uint16_t>(value & 65535));
    writeLE16(out, static_cast<std::uint16_t>(value >> 16));
}

// Must be called on a worker thread, NEVER the VST3 process() audio callback.
inline void writeWav24(const std::string& path, std::uint32_t sampleRate,
                       std::span<const StereoFrame> audio) {
    if (sampleRate < 8000 || sampleRate > 384000)
        throw std::invalid_argument("invalid sample rate");
    if (audio.size() > (std::numeric_limits<std::uint32_t>::max() - 36u) / 6u)
        throw std::length_error("WAV exceeds RIFF size limit");
    const auto dataBytes = static_cast<std::uint32_t>(audio.size() * 6);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot open WAV output");
    out.write("RIFF", 4); writeLE32(out, 36u + dataBytes);
    out.write("WAVEfmt ", 8); writeLE32(out, 16);
    writeLE16(out, 1); writeLE16(out, 2);
    writeLE32(out, sampleRate); writeLE32(out, sampleRate * 6u);
    writeLE16(out, 6); writeLE16(out, 24);
    out.write("data", 4); writeLE32(out, dataBytes);
    for (const auto& frame : audio) {
        for (const float sample : frame) {
            const double sanitized = std::isfinite(sample) ? sample : 0.;
            const double clipped = std::clamp(sanitized, -1., 1.);
            const auto pcm = static_cast<std::int32_t>(
                std::lround(clipped * (clipped < 0. ? 8388608. : 8388607.)));
            const auto bits = static_cast<std::uint32_t>(pcm);
            out.put(static_cast<char>(bits & 255));
            out.put(static_cast<char>((bits >> 8) & 255));
            out.put(static_cast<char>((bits >> 16) & 255));
        }
    }
    if (!out) throw std::runtime_error("WAV write failed");
}
} // namespace a125
