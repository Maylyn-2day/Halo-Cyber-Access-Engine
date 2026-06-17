// src/ConsoleColor.h
// ANSI color codes for console output.
// Windows 10+ (build 1511+) supports ANSI escape codes.
// Need to call ConsoleColor::init() once at the beginning of main().
#ifndef CONSOLE_COLOR_H
#define CONSOLE_COLOR_H

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX          // Prevent windows.h from defining min/max macros
#define NOMINMAX          // because it will conflict with std::numeric_limits::max()
#endif
#include <windows.h>
#endif

namespace ConsoleColor {

// ============================================================================
// ANSI Escape Codes
// ============================================================================
static const char *RESET   = "\033[0m";

// Text colors
static const char *RED     = "\033[31m";
static const char *GREEN   = "\033[32m";
static const char *YELLOW  = "\033[33m";
static const char *CYAN    = "\033[36m";
static const char *WHITE   = "\033[37m";
static const char *GRAY    = "\033[90m";   // Bright Black = Dark Gray

// Bright variants
static const char *BRED    = "\033[91m";   // Bright Red (prominent error)
static const char *BGREEN  = "\033[92m";   // Bright Green
static const char *BYELLOW = "\033[93m";   // Bright Yellow / Orange-ish
static const char *BCYAN   = "\033[96m";   // Bright Cyan

// ============================================================================
// init() - Enable ANSI processing on Windows.
// On Linux/macOS, ANSI always works, this function is a no-op.
// ============================================================================
inline void init() {
#if defined(_WIN32)
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hOut == INVALID_HANDLE_VALUE) return;

  DWORD dwMode = 0;
  if (!GetConsoleMode(hOut, &dwMode)) return;

  dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  SetConsoleMode(hOut, dwMode);
#endif
}

} // namespace ConsoleColor

#endif
