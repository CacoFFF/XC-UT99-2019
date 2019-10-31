/*=============================================================================
	XC_IpDrv.h
=============================================================================*/

#ifndef XC_IPDRV_H
#define XC_IPDRV_H

#pragma once

#define XC_CORE_API DLL_IMPORT
#define XC_IPDRV_API DLL_EXPORT

#if _MSC_VER
	#pragma warning( disable : 4201 )
#endif

#include "Engine.h"
#include "UnNet.h"

#include "Cacus/CacusThread.h"
#include "Cacus/CacusPlatform.h"

unsigned long ResolveThreadEntry( void* Arg, struct CThread* Handler);

#include "IPv6.h"
#include "Socket.h"
#include "XC_DownloadURL.h"
#include "XC_IpDrvClasses.h"
#include "XC_TcpNetDriver.h"

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

