#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

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

// Testable WAV serializer: a deliberately failing streambuf can inject a
// late flush error without filling a real Windows disk. Never on audio.
inline void writeWav24Stream(std::ostream& out,std::uint32_t sampleRate,
                             std::span<const StereoFrame> audio) {
    if (sampleRate < 8000 || sampleRate > 384000)
        throw std::invalid_argument("invalid sample rate");
    if (audio.size() > (std::numeric_limits<std::uint32_t>::max() - 36u) / 6u)
        throw std::length_error("WAV exceeds RIFF size limit");
    const auto dataBytes = static_cast<std::uint32_t>(audio.size() * 6);
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
    out.flush();
    if(!out)throw std::runtime_error("WAV write/flush failed");
}
// Publish fully closed worker outputs without replacing any existing file.
// On Windows, MoveFileExW without REPLACE_EXISTING is the atomic OS gate.
// The caller removes its private temporary file if this returns false.
inline bool publishFileExclusive(const std::filesystem::path& prepared,
                                 const std::filesystem::path& destination) noexcept {
#ifdef _WIN32
    return MoveFileExW(prepared.c_str(),destination.c_str(),
                       MOVEFILE_WRITE_THROUGH)!=0;
#else
    std::error_code ec;
    if(std::filesystem::exists(destination,ec) || ec)return false;
    std::filesystem::rename(prepared,destination,ec);
    return !ec;
#endif
}
// Must be called on a worker thread, NEVER the VST3 process() audio callback.
inline void writeWav24(const std::string& path, std::uint32_t sampleRate,
                       std::span<const StereoFrame> audio) {
    namespace fs=std::filesystem;
    const fs::path destination(path);
    if(fs::exists(destination))
        throw std::runtime_error("WAV destination already exists");
    // Each writer owns a private stage, reserved with atomic mkdir. Two
    // simultaneous captures can never truncate the same .partial file.
    fs::path stageDir;
    try {
        const auto seed=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned attempt=0;attempt<4096 && stageDir.empty();++attempt){
            fs::path candidate=destination;
            candidate+=".stage-"+std::to_string(seed)+"-"+std::to_string(attempt);
            if(fs::create_directory(candidate))stageDir=std::move(candidate);
        }
        if(stageDir.empty())throw std::runtime_error("WAV staging exhausted");
        const auto partial=stageDir/"sample.partial";
        std::ofstream out(partial,std::ios::binary|std::ios::trunc);
        if(!out)throw std::runtime_error("cannot open WAV private stage");
        writeWav24Stream(out,sampleRate,audio);
        out.close();
        if(!out)throw std::runtime_error("WAV close failed");
        const auto expected=44ull+static_cast<std::uint64_t>(audio.size())*6ull;
        if(fs::file_size(partial)!=expected)
            throw std::runtime_error("WAV final length mismatch");
        if(!publishFileExclusive(partial,destination))
            throw std::runtime_error("WAV atomic exclusive publish failed");
        std::error_code ec;
        fs::remove(stageDir,ec);
    }catch(...){
        if(!stageDir.empty()){
            std::error_code ec;
            fs::remove_all(stageDir,ec);
        }
        throw;
    }
}
} // namespace a125
