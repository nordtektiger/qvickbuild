#include "platform.hpp"
#include "cassert"

#if defined(kal_linux)
KALPlatformType KALPlatform::current() { return KALPlatformType::Linux; }
#elif defined(kal_windows)
KALPlatformType KALPlatform::current() { return KALPlatformType::Windows; }
#elif defined(kal_apple)
KALPlatformType KALPlatform::current() { return KALPlatformType::Apple; }
#endif

std::string KALPlatform::get_version_string() {
  KALPlatformType platform = KALPlatform::current();
  switch (platform) {
  case KALPlatformType::Linux:
    return QVICKBUILD_VERSION "/kal-linux";
  case KALPlatformType::Windows:
    return QVICKBUILD_VERSION "/kal-windows";
  case KALPlatformType::Apple:
    return QVICKBUILD_VERSION "/kal-apple";
  default:
    assert(false && "unrecognized kal platform");
  }
}
