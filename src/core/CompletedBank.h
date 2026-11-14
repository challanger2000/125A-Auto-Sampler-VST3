#pragma once
#include <cstdint>
#include <filesystem>
#include <mutex>
namespace a125 {
// Generation, owner epoch and archive path form ONE indivisible publication.
class CompletedBankPublication {
public:
    struct Snapshot {
        std::uint32_t generation=0;
        std::uint64_t ownerEpoch=0;
        std::filesystem::path path{};
        std::uint64_t fingerprint=0;
    };
    void publish(const std::filesystem::path& path,std::uint32_t generation,
                 std::uint64_t epoch,std::uint64_t fingerprint=0){
        std::lock_guard<std::mutex> guard(mutex_);
        latest_=Snapshot{generation,epoch,path,fingerprint};
    }
    Snapshot read() const {
        std::lock_guard<std::mutex> guard(mutex_);
        return latest_;
    }
    void reset(){
        std::lock_guard<std::mutex> guard(mutex_);
        latest_=Snapshot{};
    }
private:
    mutable std::mutex mutex_;
    Snapshot latest_{};
};
} // namespace a125
