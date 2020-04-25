
// XC_Core generics

#include "XC_Core.h"
#include "XC_CoreObj.h"
#include "XC_CoreGlobals.h"


UBOOL FGenericSystem::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	return 0;
}

UBOOL FGenericSystemDispatcher::IsTyped( const TCHAR* Type)
{
	return appStricmp( Type, TEXT("Dispatcher")) == 0;
}

UBOOL FGenericSystemDispatcher::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	INT j=0;
	for ( INT i=0 ; i<GenSystems.Num() ; i++ )
	{
		j += GenSystems(i)->Exec( Cmd, Ar );
		if ( !MultiExec && j>0 )
			return 1;
	}
	return j;
}

UBOOL FGenericSystemDispatcher::Init()
{
	for ( INT i=0 ; i<GenSystems.Num() ; i++ )
		GenSystems(i)->Init();
	return GenSystems.Num() > 0;
}

INT FGenericSystemDispatcher::Tick( FLOAT DeltaSeconds)
{
	INT j=0;
	for ( INT i=0 ; i<GenSystems.Num() ; i++ )
		j += GenSystems(i)->Tick( DeltaSeconds);
	return j;
}

void FGenericSystemDispatcher::Exit()
{
	for ( INT i=0 ; i<GenSystems.Num() ; i++ )
		GenSystems(i)->Exit();
}

FGenericSystem* FGenericSystemDispatcher::FindByType( const TCHAR* Type)
{
	for ( INT i=0 ; i<GenSystems.Num() ; i++ )
		if ( GenSystems(i)->IsTyped( Type) )
			return GenSystems(i);
	return NULL;
}

FGenericSystemDispatcher::~FGenericSystemDispatcher()
{
	GenSystems.Empty();
}

