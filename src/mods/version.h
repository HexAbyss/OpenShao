#pragma once
namespace openshao::mods {
inline constexpr const char* RecompVersion = "0.5.1";
// What the launcher, the overlay, the log and the reports show. A nightly build names itself here, while mods keep
// being checked against RecompVersion.
#if defined(OPENSHAO_BUILD_LABEL) && defined(OPENSHAO_RECOMP_VERSION)
inline constexpr const char* DisplayVersion = OPENSHAO_RECOMP_VERSION " " OPENSHAO_BUILD_LABEL;
#else
inline constexpr const char* DisplayVersion = RecompVersion;
#endif
}
