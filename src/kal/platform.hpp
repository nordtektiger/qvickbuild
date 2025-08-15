#ifndef KAL_HPP
#define KAL_HPP

/* the kal (kernel abstraction layer) is the platform specific implementations
 * of internal qvickbuild algorithms and systems. the rest of the codebase
 * should be completely platform independent. */

/* this file should be included in all kal submodules so that the platform
 * macros are defined properly. */

#if defined(__linux__)
#define kal_linux
#elif defined(_WIN32)
#error "Cannot continue: The Windows KAL platform is incomplete."
#define kal_windows
#elif defined(__APPLE__)
// caution: the apple kal platform is inadequately tested.
#define kal_apple
#else
#error "Unsupported KAL platform."
#endif

enum class KALPlatformType {
  Linux,
  Windows,
  Apple,
};

namespace KALPlatform {
KALPlatformType current();
}

#endif
