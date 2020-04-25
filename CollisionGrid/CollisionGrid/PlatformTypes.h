#pragma once


#if 1
	#define UE_DEV_THROW(n,t) {}
	#define UE_DEV_LOG(t,...) 
	#define UE_DEV_LOG_ANSI(t) 
	#define guard_slow(n) {
	#define unguard_slow }
#else
	#define UE_DEV_THROW(n,t) if(n) { appFailAssert(t,"",0); }
	#define UE_DEV_LOG(t,...) debugf(t,__VA_ARGS__)
	#define UE_DEV_LOG_ANSI(t) debugf_ansi(t)
	#define guard_slow(n) guard(n)
	#define unguard_slow unguard
#endif

// Unsigned base types.
typedef unsigned char 		uint8;		// 8-bit  unsigned.
typedef unsigned short int	uint16;		// 16-bit unsigned.
typedef unsigned long		uint32;		// 32-bit unsigned.
typedef unsigned long long	uint64;		// 64-bit unsigned.

// Signed base types.
typedef	signed char			int8;		// 8-bit  signed.
typedef signed short int	int16;		// 16-bit signed.
typedef signed int	 		int32;		// 32-bit signed.
typedef signed long long	int64;		// 64-bit signed.

//Export symbols to make disassembling easier
#if 0
	#define DE TEST_EXPORT
#else
	#define DE 
#endif

#if _WINDOWS
	#define TEST_EXPORT DLL_EXPORT
#else
	#define DLLIMPORT	__attribute__ ((visibility ("default")))
	#define TEST_EXPORT	extern "C" __attribute__ ((visibility ("default"))) 
#endif
