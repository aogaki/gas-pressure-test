#pragma once

#include <unistd.h>

#include <cstdlib>
#include <string>

// The handful of helpers the three command line parsers of Stage 2, 3 and 4
// share (TODO/09 R15). Header only and free of any ROOT, Geant4 and
// Garfield++ dependency, like the parsers themselves.

// getopt() keeps state between calls, which the unit tests exercise.
inline void ResetGetopt() {
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__NetBSD__)
  optreset = 1;
  optind = 1;
#else
  optind = 0;
#endif
}

// strtod() with a "the whole argument was a number" check.
inline bool ToDouble(const char* text, double& value) {
  char* end = nullptr;
  value = std::strtod(text, &end);
  return end != text && *end == '\0';
}

// strtol() with the same check.
inline bool ToLong(const char* text, long& value) {
  char* end = nullptr;
  value = std::strtol(text, &end, 10);
  return end != text && *end == '\0';
}

inline bool EndsWith(const std::string& text, const std::string& suffix) {
  return text.size() >= suffix.size() &&
         text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}
