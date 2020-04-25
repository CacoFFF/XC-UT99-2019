/*=============================================================================
	XC_Globals.cpp: 
	Implementation of some globals
=============================================================================*/

#include "XC_Core.h"
#include "XC_CoreGlobals.h"
#include "UnLinker.h"


//*************************************************
// Name case fixing
// Makes sure that a name has proper casing
// Helps preventing DLL bind failures
//*************************************************
XC_CORE_API UBOOL FixNameCase( const TCHAR* NameToFix)
{
	FName AName( NameToFix, FNAME_Find);
	if ( AName != NAME_None )
	{
		TCHAR* Ch = (TCHAR*) *AName;
		for ( INT i=0 ; i<63 && Ch[i] ; i++ )
			Ch[i] = NameToFix[i];
	}
	return AName != NAME_None;
}


//*************************************************
// Script stuff
// Finds a (non-state) function in a struct
// Finds a variable in a struct, increments *Found
//*************************************************
XC_CORE_API UFunction* FindBaseFunction( UStruct* InStruct, const TCHAR* FuncName)
{
	FName NAME_FuncName = FName( FuncName, FNAME_Find);
	if ( NAME_FuncName != NAME_None )
	{
		for( TFieldIterator<UFunction> Func( InStruct ); Func; ++Func )
			if( Func->GetFName() == NAME_FuncName )
				return (UFunction*) *Func;
	}
	return NULL;	
}

XC_CORE_API UProperty* FindScriptVariable( UStruct* InStruct, const TCHAR* PropName, INT* Found)
{
	FName NAME_PropName = FName( PropName, FNAME_Find);
	if ( NAME_PropName != NAME_None )
	{
		for( TFieldIterator<UProperty> Prop( InStruct ); Prop; ++Prop )
			if( Prop->GetFName() == NAME_PropName )
			{
				if ( Found )
					(*Found)++;
				return (UProperty*) *Prop;
			}
	}
	return NULL;	
}


//*************************************************
// String table sorting
//*************************************************
//Caching OList generates 3 more instructions at start
//But also removes two instructions on the appStricmp call


template<class T> inline void ExchangeUsingRegisters( T& A, T& B )
{
	register INT Tmp;
	for ( INT i=0 ; i<sizeof(FString)/sizeof(Tmp) ; i++ )
	{
		Tmp = ((INT*)&A)[i];
		((INT*)&A)[i] = ((INT*)&B)[i];
		((INT*)&B)[i] = Tmp;
	}
}

//Dynamic array
XC_CORE_API void SortStringsA( TArray<FString>* List)
{
	SortStringsSA( (FString*) List->GetData(), List->Num() );
}

//Static array, although not fully optimized for speed
XC_CORE_API void SortStringsSA( FString* List, INT ArrayMax)
{
	INT iTop=1;
	for ( INT i=1 ; i<ArrayMax ; i=++iTop )
/*	{
		//Optimized for long sorts, test later
		while ( appStricmp( *List[iTop], *List[i-1]) < 0 )
			if ( --i == 0 )
				break;
		if ( i != iTop )
		{
			INT* Ptr = (INT*) &List[iTop];
			INT Buffer[3] = { Ptr[0], Ptr[1], Ptr[2] };
			appMemmove( List + i + 1, List + i, (iTop-i)*3 );
			Ptr = (INT*) &List[i];
			Ptr[0] = Buffer[0];
			Ptr[1] = Buffer[1];
			Ptr[2] = Buffer[2];
		}
	}*/
	//Optimized for short sorts
		while ( appStricmp( *List[i], *List[i-1]) < 0 )
		{
			//Atomically swap data (no FArray reallocs)
			//Compiler does a nice job here
			ExchangeUsingRegisters( List[i], List[i-1] );
/*			INT* Ptr = (INT*) &List[i];
			INT iStore = *Ptr;
			*Ptr = *(Ptr-3);
			*(Ptr-3) = iStore;
			iStore = *(Ptr+1);
			*(Ptr+1) = *(Ptr-2);
			*(Ptr-2) = iStore;
			iStore = *(Ptr+2);
			*(Ptr+2) = *(Ptr-1);
			*(Ptr-1) = iStore;*/
			//Bottom index, forced leave
			if ( i == 1 )
				break;
			i--;
		}
}


//*************************************************
// Enhanced package file finder
// Strict loading by Name/GUID combo for net games
//*************************************************

inline FGenerationInfo::FGenerationInfo( INT InExportCount, INT InNameCount )
	: ExportCount(InExportCount), NameCount( InNameCount)
{
}

XC_CORE_API FPackageFileSummary LoadPackageSummary( const TCHAR* File)
{
	guard(LoadPackageSummary);
	FPackageFileSummary Summary;
	FArchive* Ar = GFileManager->CreateFileReader( File);
	if ( Ar )
	{
		*Ar << Summary;
		delete Ar;
	}
	return Summary;
	unguard;
}
