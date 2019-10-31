/*=============================================================================
	XC_Drv.cpp
	Package and UClasses implementations.
=============================================================================*/

#include "XC_IpDrv.h"

/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

// Register things.
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) FName XC_IPDRV_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "XC_IpDrvClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY
static inline void RegisterNames()
{
	static INT Registered=0;
	if(!Registered++)
	{
		#define NAMES_ONLY
		#define AUTOGENERATE_NAME(name) extern XC_IPDRV_API FName XC_IPDRV_##name; XC_IPDRV_##name=FName(TEXT(#name),FNAME_Intrinsic);
		#define AUTOGENERATE_FUNCTION(cls,idx,name)
		#include "XC_IpDrvClasses.h"
		#undef DECLARE_NAME
		#undef NAMES_ONLY
	}
}


IMPLEMENT_PACKAGE(XC_IpDrv);

/*-----------------------------------------------------------------------------
	Definitions.
-----------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

