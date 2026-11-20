#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace openshao::update {
inline constexpr const char* kLatestReleaseUrl = "https://api.github.com/repos/OAleex/OpenShao/releases/latest";
inline constexpr const char* kReleasesPage = "https://github.com/OAleex/OpenShao/releases/latest";
inline std::string ReleaseNotesUrl(std::string_view version) {
    return "https://github.com/OAleex/OpenShao/releases/tag/v" + std::string(version);
}

// Where releases come from and how their tags are read. The default is the official project; a nightly build uses NightlyChannel().
struct Channel {
    std::string latest_url = kLatestReleaseUrl;
    std::string download_prefix = "https://github.com/OAleex/OpenShao/releases/download/";
    std::string page = kReleasesPage;
    bool nightly = false;  // tags are nightly-YYYYMMDD and releases are compared by that date
};
Channel NightlyChannel();
// The date in a build label such as "nightly 20261120"; empty when the label has none.
std::string BuildDate(std::string_view label);
std::string NightlyPackageName(std::string_view date);

struct Release {
    std::string version, page, package_name, package_url, checksums_url, sha256;
    uint64_t size = 0;
};
std::string PackageName(std::string_view version);
std::string ExecutableName();
Release ParseLatest(std::string_view json);
Release ParseLatest(std::string_view json, const Channel& channel);
std::string ChecksumFor(std::string_view listing, std::string_view file);
using Files = std::map<std::string, std::vector<uint8_t>>;
Files ReadPackage(const std::filesystem::path& archive);
// Installation rolls back on failure; replaced files are cleaned up on the next start.
void Install(const std::filesystem::path& folder, const Files& files);
void RemovePreviousFiles(const std::filesystem::path& folder);
bool Relaunch(const std::filesystem::path& folder, std::string& error);

enum class State { Idle, Checking, Current, Available, Downloading, Ready, Failed };
enum class CheckResult { Started, Cached, Busy };
struct View {
    State state = State::Idle;
    Release release;
    std::string error;
    uint64_t done = 0, total = 0;
    bool fresh = false;  // the latest release was read from the network in this run, not only from the cache
};
class Updater {
public:
    using Fetch = std::function<void(std::string_view url, const std::filesystem::path& file, uint64_t maximum,
                                     const std::function<bool(uint64_t, uint64_t)>& progress)>;
    Updater(std::filesystem::path install, std::filesystem::path cache, std::string current, Fetch fetch = {}, Channel channel = {});
    ~Updater();
    View Get() const;
    // Automatic checks use a two-hour interval; manual checks use a 15-minute interval. `force` ignores the interval (a rate limit still holds it back).
    CheckResult Check(bool manual = false, bool force = false);
    void Tick();
    void Download();
    void Cancel();
private:
    void Start(std::function<void()> job);
    void SaveCheckState() const;
    int Compare(std::string_view release, std::string_view installed) const;
    std::filesystem::path install_, cache_;
    std::string current_;
    Fetch fetch_;
    Channel channel_;
    mutable std::mutex mutex_;
    View view_;
    int64_t last_check_ = 0, blocked_until_ = 0;
    std::atomic<bool> cancel_{false};
    std::jthread worker_;
};
}
