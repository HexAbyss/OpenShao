// Checks release parsing, package reading, installation and the update worker without network access.
#include "updater.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include "file_ops.h"
#include "mods/sha256.h"
namespace fs = std::filesystem;
using namespace openshao;
void Require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
std::string Get(const fs::path& p) { std::ifstream f(p, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }
void Put(const fs::path& p, const std::string& text) { fs::create_directories(p.parent_path()); std::ofstream(p, std::ios::binary) << text; }
std::string Release(const std::string& tag, const std::string& digest, bool checksums, uint64_t size = 4) {
    const auto name = update::PackageName(tag.starts_with('v') ? tag.substr(1) : tag);
    std::string assets = "{\"name\":\"" + name + "\",\"size\":" + std::to_string(size) + ",\"browser_download_url\":\"https://github.com/OAleex/OpenShao/releases/download/" + tag + "/" + name + "\"" +
                         (digest.empty() ? "" : ",\"digest\":\"" + digest + "\"") + "}";
    assets += ",{\"name\":\"OpenShao-9.9.9-Other-arm64.zip\",\"size\":1,\"browser_download_url\":\"https://github.com/OAleex/OpenShao/releases/download/x/other\"}";
    if (checksums) assets += ",{\"name\":\"checksums.txt\",\"size\":10,\"browser_download_url\":\"https://github.com/OAleex/OpenShao/releases/download/" + tag + "/checksums.txt\"}";
    return "{\"tag_name\":\"" + tag + "\",\"html_url\":\"https://github.com/OAleex/OpenShao/releases/tag/" + tag +
           "\",\"draft\":false,\"prerelease\":false,\"body\":\"Fixes \\u00e7 \\ud83d\\ude00\",\"assets\":[" + assets + "]}";
}
std::string NightlyRelease(const std::string& date, bool checksums, uint64_t size = 4, const std::string& owner = "HexAbyss") {
    const auto name = update::NightlyPackageName(date);
    const std::string base = "https://github.com/" + owner + "/OpenShao/releases/download/nightly-" + date + "/";
    std::string assets = "{\"name\":\"" + name + "\",\"size\":" + std::to_string(size) + ",\"browser_download_url\":\"" + base + name + "\"}";
    if (checksums) assets += ",{\"name\":\"checksums.txt\",\"size\":10,\"browser_download_url\":\"" + base + "checksums.txt\"}";
    return "{\"tag_name\":\"nightly-" + date + "\",\"html_url\":\"https://github.com/HexAbyss/OpenShao/releases/tag/nightly-" + date + "\",\"assets\":[" + assets + "]}";
}
void WaitFor(update::Updater& updater, update::State busy) {
    for (int i = 0; i < 500 && updater.Get().state == busy; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
int main(int argc, char** argv) {
    const auto base = fs::temp_directory_path() / ("openshao-updater-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        Require(update::ReleaseNotesUrl("0.4.1") == "https://github.com/OAleex/OpenShao/releases/tag/v0.4.1",
                "Release notes must open the installed version instead of the latest release");
        const std::string hash(64, 'a');
        auto parsed = update::ParseLatest(Release("v0.3.1", "sha256:" + std::string(64, 'A'), true));
        Require(parsed.version == "0.3.1", "The tag prefix must be removed");
        Require(parsed.package_name == update::PackageName("0.3.1") && parsed.package_url.ends_with(parsed.package_name), "The platform package must be selected");
        Require(parsed.sha256 == hash && parsed.size == 4, "The asset digest and size must be read");
        Require(parsed.checksums_url.ends_with("/checksums.txt"), "The checksum list must be found");
        parsed = update::ParseLatest(Release("0.4.0", "", false));
        Require(parsed.sha256.empty() && parsed.checksums_url.empty() && !parsed.package_url.empty(), "Missing digests must stay empty");
        bool rejected = false;
        try { update::ParseLatest("{\"tag_name\":\"latest\"}"); } catch (const std::exception&) { rejected = true; }
        Require(rejected, "A tag that is not a version must be rejected");
        const auto foreign = update::ParseLatest("{\"tag_name\":\"v1.0.0\",\"assets\":[{\"name\":\"" + update::PackageName("1.0.0") +
                                                 "\",\"browser_download_url\":\"https://example.com/" + update::PackageName("1.0.0") + "\"}]}");
        Require(foreign.package_url.empty(), "Packages outside the project's releases must be ignored");
        Require(update::ChecksumFor(hash + "  other.zip\r\n" + std::string(64, 'B') + " *" + update::PackageName("0.4.0") + "\n", update::PackageName("0.4.0")) == std::string(64, 'b'),
                "Checksum listings must match the file name");
        Require(update::ChecksumFor("not a hash  file\n", "file").empty(), "Invalid checksum rows must be ignored");


        // The nightly channel: releases of the fork tagged nightly-YYYYMMDD, compared by date.
        {
            const auto channel = update::NightlyChannel();
            Require(update::BuildDate("nightly 20261120") == "20261120" && update::BuildDate("0.5.1").empty() && update::BuildDate("").empty() &&
                    update::BuildDate("nightly 2026").empty(), "The build date is the last word of the label, when it is a date");
            auto nightly = update::ParseLatest(NightlyRelease("20261121", true, 7), channel);
            Require(nightly.version == "20261121" && nightly.package_name == update::NightlyPackageName("20261121"), "The nightly tag gives the date as the version");
            Require(nightly.package_url.ends_with(nightly.package_name) && nightly.size == 7 && nightly.checksums_url.ends_with("/checksums.txt"), "The nightly package and checksums are found");
            Require(update::ParseLatest(NightlyRelease("20261121", true, 4, "OAleex"), channel).package_url.empty(), "A nightly package outside the fork's releases is ignored");
            for (const char* tag : {"v0.5.1", "nightly-2026", "nightly-2026112a", "latest"}) {
                bool refused = false;
                try { update::ParseLatest(std::string("{\"tag_name\":\"") + tag + "\"}", channel); } catch (const std::exception&) { refused = true; }
                Require(refused, std::string("A tag that is not a nightly is refused: ") + tag);
            }
        }
        for (const char* installed : {"20261120", "20261121", "20261122"}) {
            update::Updater updater(base / "nightly" / installed, base / "nightly-cache" / installed, installed,
                [&](std::string_view url, const fs::path& file, uint64_t, const std::function<bool(uint64_t, uint64_t)>&) {
                    Require(url == update::NightlyChannel().latest_url, "A nightly build asks the fork's releases");
                    Put(file, NightlyRelease("20261121", true));
                }, update::NightlyChannel());
            Require(updater.Check() == update::CheckResult::Started, "The first check starts");
            WaitFor(updater, update::State::Checking);
            const auto view = updater.Get();
            const bool newer = std::string(installed) < "20261121";
            Require(view.state == (newer ? update::State::Available : update::State::Current), std::string("Only a newer date is offered, installed ") + installed);
            Require(view.fresh, "A release read from the network is marked fresh");
            Require(updater.Check() == update::CheckResult::Cached, "The two-hour interval holds an ordinary check back");
            Require(updater.Check(false, true) == update::CheckResult::Started, "A forced check, as at every start, ignores the interval");
            WaitFor(updater, update::State::Checking);
        }
        {
            // The release installed last counts as installed, so a build compiled before the release date does not update again and again.
            Put(base / "nightly-cache" / "marker" / "installed-version.txt", "20261121\n");
            update::Updater marked(base / "nightly" / "marker", base / "nightly-cache" / "marker", "20261120",
                [&](std::string_view, const fs::path& file, uint64_t, const std::function<bool(uint64_t, uint64_t)>&) { Put(file, NightlyRelease("20261121", true)); },
                update::NightlyChannel());
            marked.Check();
            WaitFor(marked, update::State::Checking);
            Require(marked.Get().state == update::State::Current, "The release installed last is not offered again");
        }
        // Real packages created by tools/package_release.py, passed by tests/updater_test.py.
        if (argc > 1) {
            const fs::path package = argv[1];
            const auto files = update::ReadPackage(package);
            Require(files.contains(update::ExecutableName()) && files.contains("readme.txt") && files.contains("licenses/LICENSE.txt"),
                    "The package must contain the executable, readme and licenses");
            const auto install = base / "install folder";
            Put(install / update::ExecutableName(), "old executable");
            Put(install / "readme.txt", "old readme");
            Put(install / "keep.txt", "user file");
            update::Install(install, files);
            const auto& exe = files.at(update::ExecutableName());
            Require(Get(install / update::ExecutableName()) == std::string(exe.begin(), exe.end()), "The executable must be replaced");
            Require(Get(install / (update::ExecutableName() + ".old")) == "old executable", "The previous executable must stay until the next start");
            Require(Get(install / "keep.txt") == "user file", "Files outside the package must stay");
            Require(fs::exists(install / "licenses/LICENSE.txt"), "New package folders must be created");
#ifndef _WIN32
            Require((fs::status(install / update::ExecutableName()).permissions() & fs::perms::owner_exec) != fs::perms::none, "The executable must stay executable");
#endif
            update::RemovePreviousFiles(install);
            Require(!fs::exists(install / (update::ExecutableName() + ".old")) && !fs::exists(install / "readme.txt.old"), "Previous files must be removed on the next start");
            Require(Get(install / "keep.txt") == "user file", "Cleanup must keep other files");

            // A failed installation restores the files it replaced.
            const auto broken = base / "broken";
            Put(broken / update::ExecutableName(), "running");
            Put(broken / "licenses", "a file where the package needs a folder");
            bool failed = false;
            try { update::Install(broken, files); } catch (const std::exception&) { failed = true; }
            Require(failed, "A conflicting package must fail");
            Require(Get(broken / update::ExecutableName()) == "running", "A failed update must restore the executable");
            Require(!fs::exists(broken / (update::ExecutableName() + ".new")), "A failed update must remove staged files");
            Require(Get(broken / "licenses") == "a file where the package needs a folder", "A failed update must keep existing files");

            // The worker checks, downloads, verifies and installs through an injected transfer.
            const auto release_dir = base / "release";
            const auto copy = release_dir / update::PackageName("9.0.0");
            fs::create_directories(release_dir);
            fs::copy_file(package, copy);
            const auto digest = mods::Sha256File(copy);
            for (const bool corrupt : {true, false}) {
                const auto target = base / (corrupt ? "corrupt" : "worker");
                Put(target / update::ExecutableName(), "old");
                update::Updater updater(target, base / (corrupt ? "cache-corrupt" : "cache"), "0.3.0",
                    [&](std::string_view url, const fs::path& file, uint64_t maximum, const std::function<bool(uint64_t, uint64_t)>& progress) {
                        if (url == update::kLatestReleaseUrl) { Put(file, Release("v9.0.0", "", true, fs::file_size(copy))); return; }
                        if (url.ends_with("/checksums.txt")) { Put(file, (corrupt ? std::string(64, 'c') : digest) + "  " + update::PackageName("9.0.0") + "\n"); return; }
                        Require(fs::file_size(copy) <= maximum, "The package must fit the declared limit");
                        fs::copy_file(copy, file, fs::copy_options::overwrite_existing);
                        if (progress) progress(fs::file_size(copy), fs::file_size(copy));
                    });
                auto wait = [&](update::State busy) {
                    for (int i = 0; i < 500 && updater.Get().state == busy; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    return updater.Get();
                };
                updater.Check();
                auto view = wait(update::State::Checking);
                Require(view.state == update::State::Available && view.release.version == "9.0.0", "A newer release must be offered: " + view.error);
                updater.Download();
                view = wait(update::State::Downloading);
                if (corrupt) {
                    Require(view.state == update::State::Failed && view.error.find("SHA-256") != std::string::npos, "A checksum mismatch must stop the update");
                    Require(Get(target / update::ExecutableName()) == "old", "A rejected package must not change files");
                } else {
                    Require(view.state == update::State::Ready, "A verified package must be installed: " + view.error);
                    Require(Get(target / update::ExecutableName()) == std::string(exe.begin(), exe.end()), "The worker must install the package");
                }
            }

            // A nightly install writes the release it installed, and the next start does not offer it again.
            {
                const auto nightly_copy = release_dir / update::NightlyPackageName("20261121");
                fs::copy_file(package, nightly_copy);
                const auto nightly_digest = mods::Sha256File(nightly_copy);
                const auto target = base / "nightly-worker", cache = base / "nightly-worker-cache";
                Put(target / update::ExecutableName(), "old");
                const auto fetch = [&](std::string_view url, const fs::path& file, uint64_t, const std::function<bool(uint64_t, uint64_t)>& progress) {
                    if (url == update::NightlyChannel().latest_url) { Put(file, NightlyRelease("20261121", true, fs::file_size(nightly_copy))); return; }
                    if (url.ends_with("/checksums.txt")) { Put(file, nightly_digest + "  " + update::NightlyPackageName("20261121") + "\n"); return; }
                    fs::copy_file(nightly_copy, file, fs::copy_options::overwrite_existing);
                    if (progress) progress(fs::file_size(nightly_copy), fs::file_size(nightly_copy));
                };
                {
                    update::Updater updater(target, cache, "20261120", fetch, update::NightlyChannel());
                    updater.Check();
                    WaitFor(updater, update::State::Checking);
                    Require(updater.Get().state == update::State::Available, "A newer nightly is offered");
                    updater.Download();
                    WaitFor(updater, update::State::Downloading);
                    Require(updater.Get().state == update::State::Ready, "The nightly package is verified and installed: " + updater.Get().error);
                    Require(Get(target / update::ExecutableName()) == std::string(exe.begin(), exe.end()), "The nightly worker installs the package");
                }
                Require(Get(cache / "installed-version.txt") == "20261121\n", "The installed release is remembered");
                update::Updater restarted(target, cache, "20261120", fetch, update::NightlyChannel());
                Require(restarted.Get().state == update::State::Current, "After the restart the same nightly is not offered again");
                Require(!restarted.Get().fresh, "A release known only from the cache is not fresh");
            }
            update::Updater current(base / "current", base / "cache-current", "9.0.0",
                [&](std::string_view, const fs::path& file, uint64_t, const std::function<bool(uint64_t, uint64_t)>&) { Put(file, Release("v9.0.0", "", true)); });
            current.Check();
            for (int i = 0; i < 500 && current.Get().state == update::State::Checking; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
            Require(current.Get().state == update::State::Current, "The installed version must not be offered again");
        }
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        std::error_code ignored; fs::remove_all(base, ignored);
        return 1;
    }
    std::error_code ignored; fs::remove_all(base, ignored);
    std::cout << "PASS: release metadata, package formats, verified installation, rollback and cleanup\n";
    return 0;
}
