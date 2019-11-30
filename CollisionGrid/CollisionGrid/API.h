#pragma once

#include "Engine.h"
#include "PlatformTypes.h"

extern FMemStack* Mem;

namespace cg
{
	struct Vector;
	struct Integers;
}




#ifdef _MSC_VER
//***********************************************************************************
//Microsoft Visual C++ directives, done to avoid linking to new versions of C Runtime
//The only 'default' library to use should be VC++ 6.0 MSVCRT.LIB

extern "C" int _fltused;
extern "C" int __cdecl _purecall();

//#include <Windows.h>

#else
//***********************************************************************************
//Other directives, use standard functions

#endif




//***********************************************************************************
// Quick CheatEngine debugger helpers
class DebugToken
{
public:
	DebugToken( char C);
	~DebugToken();
};

class DebugLock
{
public:
	DebugLock( const char* Keyword, char Lock);
};

//***********************************************************************************
// Simple text parser and concatenator
class PlainText
{
	TCHAR* TAddr;
	uint32 Size;
public:
	PlainText();
	PlainText(const TCHAR* T);
	PlainText operator+ (const TCHAR* T);
	PlainText operator+ (UObject* O);
	PlainText operator+ (int32 N);
	PlainText operator+ (uint32 N);
	PlainText operator+ (float F);
	PlainText operator+ (const FVector& V);
	PlainText operator+ (const cg::Vector& V);
	PlainText operator+ (const cg::Integers& V);
	PlainText operator+ (const void* Ptr);

	const TCHAR* operator*();
	void Reset();
#if UNICODE
	char* Ansi();
#else
	char* Ansi() { return TAddr; };
#endif
};




//***********************************************************************************
//General methods
void* appMallocAligned( uint32 Size, uint32 Align);
void* appFreeAligned( void* Ptr);


enum EAlign { A_16 = 16 };
enum ESize
{	SIZE_Bytes = 0,
	SIZE_KBytes = 10,
	SIZE_MBytes = 20	};
enum EStack { E_Stack = 0 };


void* operator new( size_t Size, ESize Units, uint32 RealSize); //Allocate objects using additional memory (alignment optional)
void* operator new( size_t Size, EAlign Tag );
void* operator new( size_t Size, EAlign Tag, ESize Units, uint32 RealSize);

//Placement new
inline void* operator new( size_t Size, void* Address, EStack Tag)
{
	return Address;
}


//Delete objects created with new(A_16)
#define Delete_A( A) if (A) { \
					A->Exit(); \
					appFreeAligned( (void*)A); \
					A = nullptr; }


