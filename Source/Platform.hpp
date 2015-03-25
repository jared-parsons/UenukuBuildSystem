#ifndef GUARD_Platform_hpp
#define GUARD_Platform_hpp

#define PLATFORM_LINUX			0
#define PLATFORM_MAC_OSX		1

#ifdef __APPLE__
#define PLATFORM				PLATFORM_MAC_OSX
#else
#define PLATFORM				PLATFORM_LINUX
#endif

#endif
