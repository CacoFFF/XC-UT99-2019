/*=============================================================================
Stuff
This should only be included by XC_Core project, include individual headers
you need and add the proper macros and definitions in your own private header
=============================================================================*/

#ifndef INC_XC_CORE
#define INC_XC_CORE

//Visual Studio 2015 directives
#pragma warning (disable : 4456) //Allow functions to override global variable names
#pragma warning (disable : 4457) //Allow functions to override global variable names
#pragma warning (disable : 4458) //Allow functions to override global variable names
#pragma warning (disable : 4459) //Allow functions to override global variable names
#pragma warning (disable : 4297) //Constructors and destructors allowed to throw
#pragma warning (disable : 4305) //Truncation from 'double' to 'float'
#define _CRT_SECURE_NO_WARNINGS //Because older string methods are no longer used

#pragma warning (disable : 4714) //Allow forceinline to fail


//Safely empty a dynamic array on UT v451
#define SafeEmpty( A) if (A.GetData()) A.Empty()
#define SafeEmptyR( A) if (A->GetData()) A->Empty()


#define warnf				GWarn->Logf
#define debugf				GLog->Logf

#define ENGINE_API DLL_IMPORT

#ifndef __LINUX_X86__
	#define CORE_API DLL_IMPORT
	#define XC_CORE_API DLL_EXPORT
	typedef char CHAR;
	#include <windows.h>
	#undef TEXT
#else
	#include <unistd.h>	
#endif

// Unreal engine includes.
//#include "Engine.h"


#include "Core.h"
#include "Cacus/CacusPlatform.h"
#include "XC_Template.h"
#include "XC_CoreClasses.h"

// Private functions
int StaticInitScriptCompiler();

#ifdef __LINUX_X86__
	#undef CPP_PROPERTY
	#define CPP_PROPERTY(name) \
		EC_CppProperty, (BYTE*)&((ThisClass*)1)->name - (BYTE*)1

	inline int StaticInitScriptCompiler() { return 0; }
#endif


inline void OSpath( const TCHAR* Path)
{
#ifndef _MSC_VER
	TCHAR* Buf = (TCHAR*)Path;
	while ( *Buf )
	{
		if ( *Buf == '\\' )
			*Buf = '/';
		Buf++;
	}
#endif
}

#endif
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
