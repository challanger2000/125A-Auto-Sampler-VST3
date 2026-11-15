#pragma once
// P026: deterministic local .125A instrument browser.
// Called only from UI or core tests. No directory access on an audio callback.
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <system_error>
#include <vector>

namespace a125 {

struct InstrumentNeighbors {
    std::vector<std::filesystem::path> paths;
    std::size_t current=0;
    bool found=false;
};

// Enumerate only immediate regular .125A files in the current instrument's
// folder. Bounded and fail-closed: a broken/huge directory never causes
// partial navigation or an unbounded GUI allocation.
inline InstrumentNeighbors listInstrumentNeighbors(
        const std::filesystem::path& active,std::size_t limit=4096) {
    namespace fs=std::filesystem;
    InstrumentNeighbors result;
    if(active.empty() || active.extension()!=".125A" || limit==0)
        return result;
    std::error_code ec;
    fs::directory_iterator it(active.parent_path(),
        fs::directory_options::skip_permission_denied,ec),end;
    if(ec)return result;
    for(;it!=end;it.increment(ec)){
        if(ec)return {};
        std::error_code statusError;
        if(!it->is_regular_file(statusError) || statusError)continue;
        if(it->path().extension()!=".125A")continue;
        if(result.paths.size()==limit)return {};
        result.paths.emplace_back(it->path());
    }
    if(ec)return {};
    std::sort(result.paths.begin(),result.paths.end(),
        [](const fs::path& a,const fs::path& b){
            return a.filename().native()<b.filename().native();
        });
    const auto current=std::find_if(result.paths.begin(),result.paths.end(),
        [&](const fs::path& p){return p.filename()==active.filename();});
    if(current==result.paths.end())return {};
    result.current=static_cast<std::size_t>(
        std::distance(result.paths.begin(),current));
    result.found=true;
    return result;
}

// "step" counts from current; +1 = next, -1 = previous. Wraps around.
// The currently loaded archive is never returned as a browsing candidate.
inline std::vector<std::filesystem::path> instrumentBrowseOrder(
        const InstrumentNeighbors& siblings,int step){
    std::vector<std::filesystem::path> order;
    const auto count=siblings.paths.size();
    if(!siblings.found || count<2 || step==0 ||
       siblings.current>=count)return order;
    order.reserve(count-1);
    for(std::size_t i=1;i<count;++i){
        const auto position=step>0 ?
            (siblings.current+i)%count:
            (siblings.current+count-i)%count;
        order.push_back(siblings.paths[position]);
    }
    return order;
}

} // namespace a125
