#include "platform.hpp"

#if defined(kal_linux)
KALPlatformType KALPlatform::current() { return KALPlatformType::Linux; }
#elif defined(kal_windows)
KALPlatformType KALPlatform::current() { return KALPlatformType::Windows; }
#elif defined(kal_apple)
KALPlatformType KALPlatform::current() { return KALPlatformType::Apple; }
#endif
