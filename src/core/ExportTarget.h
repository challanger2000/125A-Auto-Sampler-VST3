#pragma once
#include <filesystem>
#include <mutex>

namespace a125 {

// Only accessed by VSTGUI, DAW state serialization and WAV writer threads.
// Never read this lock from an ASIO process() callback.
class ExportTarget {
public:
    void choose(std::filesystem::path folder) {
        std::lock_guard<std::mutex> guard(lock_);
        folder_=std::move(folder);
    }
    std::filesystem::path chosenFolder() const {
        std::lock_guard<std::mutex> guard(lock_);
        return folder_;
    }
    static std::filesystem::path resolve(
        const std::filesystem::path& selected,
        const std::filesystem::path& fallback) {
        // No I/O required here; the picker guarantees an existing folder.
        return selected.empty()?fallback:selected;
    }
private:
    mutable std::mutex lock_;
    std::filesystem::path folder_;
};
} // namespace a125
