#pragma once
namespace openshao::mods {
inline constexpr const char* RecompVersion = "0.5.0";
// What the launcher, the overlay, the log and the reports show. A nightly build names itself here, while mods keep
// being checked against RecompVersion.
#ifdef OPENSHAO_BUILD_LABEL
inline constexpr const char* DisplayVersion = "0.5.0 " OPENSHAO_BUILD_LABEL;
#else
inline constexpr const char* DisplayVersion = RecompVersion;
#endif
}
