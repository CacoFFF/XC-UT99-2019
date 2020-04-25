/*=============================================================================
	ScriptCompilerAdds.cpp: 
	Script Compiler addons.

	Revision history:
		* Created by Higor
=============================================================================*/

#include "XC_Core.h"
#include "XC_CoreGlobals.h"
#include "Engine.h"

//Deus-Ex script compiler
#include "UnScrCom.h"

#define PSAPI_VERSION 1
#include <Psapi.h>

#pragma comment (lib,"Psapi.lib")

//***************
// Virtual Memory
template <size_t MemSize> class TScopedVirtualProtect
{
	uint8* Address;
	uint32 RestoreProtection;

	TScopedVirtualProtect() {}
public:
	TScopedVirtualProtect( uint8* InAddress)
		: Address( InAddress)
	{
		if ( Address )	VirtualProtect( Address, MemSize, PAGE_EXECUTE_READWRITE, &RestoreProtection);
	}

	~TScopedVirtualProtect()
	{
		if ( Address )	VirtualProtect( Address, MemSize, RestoreProtection, &RestoreProtection);
	}
};

// Writes a long relative jump
static void EncodeJump( uint8* At, uint8* To)
{
	TScopedVirtualProtect<5> VirtualUnlock( At);
	uint32 Relative = To - (At + 5);
	*At = 0xE9;
	*(uint32*)(At+1) = Relative;
}
// Writes a long relative call
static void EncodeCall( uint8* At, uint8* To)
{
	TScopedVirtualProtect<5> VirtualUnlock( At);
	uint32 Relative = To - (At + 5);
	*At = 0xE8;
	*(uint32*)(At+1) = Relative;
}
// Writes bytes
/*static void WriteByte( uint8* At, uint8 Byte)
{
	TScopedVirtualProtect<1> VirtualUnlock( At);
	*At = Byte;
}*/


//***************
// Hook resources 


typedef int (*CompileScripts_Func)( TArray<UClass*>&, class FScriptCompiler_XC*, UClass*);
static CompileScripts_Func CompileScripts;
static UBOOL CompileScripts_Proxy( TArray<UClass*>& ClassList, FScriptCompiler_XC* Compiler, UClass* Class );


class FScriptCompiler_XC : public FScriptCompiler
{
public:
	UField* FindField( UStruct* Owner, const TCHAR* InIdentifier, UClass* FieldClass, const TCHAR* P4);
};

//TODO: Disassemble Editor.so for more symbols
// Hook helper
struct ScriptCompilerHelper_XC_CORE
{
public:
	UBOOL bInit;
	TArray<UFunction*> ActorFunctions; //Hardcoded Actor functions
	TArray<UFunction*> ObjectFunctions; //Hardcoded Object functions
	TArray<UFunction*> StaticFunctions; //Hardcoded static functions
	//Struct mirroring causes package dependancy, we need to copy the struct

	ScriptCompilerHelper_XC_CORE()
		: bInit(0)
	{}

	void Reset()
	{
		bInit = 0;
		ActorFunctions.Empty();
		ObjectFunctions.Empty();
		StaticFunctions.Empty();
	}

	void AddFunction( UStruct* InStruct, const TCHAR* FuncName)
	{
		UFunction* F = FindBaseFunction( InStruct, FuncName);
		if ( F )
		{
			if ( F->FunctionFlags & FUNC_Static )
				StaticFunctions.AddItem( F);
			else if ( InStruct->IsChildOf( AActor::StaticClass()) )
				ActorFunctions.AddItem( F);
			else
				ObjectFunctions.AddItem( F);
		}
	}

};
static ScriptCompilerHelper_XC_CORE Helper; //Makes C runtime init construct this object




#define ForceAssign(Member,dest) \
	__asm { \
		__asm mov eax,Member \
		__asm lea ecx,dest \
		__asm mov [ecx],eax }


int StaticInitScriptCompiler()
{
	if ( !GIsEditor /*|| true*/ ) //Do not setup in v469
		return 0; // Do not setup if game instance

	HMODULE HEditor = GetModuleHandleA("Editor.dll");
	if ( !HEditor )
		return 0;

	MODULEINFO mInfo;
	GetModuleInformation( GetCurrentProcess(), HEditor, &mInfo, sizeof(MODULEINFO));
	uint8* EditorBase = (uint8*)mInfo.lpBaseOfDll;

	static int Initialized = 0;
	if ( Initialized++ )
		return 0; //Prevent multiple recursion

//	if ( Initialized != 1337 ) //Disable for now
//		return 0;

	uint8* Tmp;

	ForceAssign( CompileScripts_Proxy, Tmp); //Proxy CompileScripts initial call
	EncodeCall( EditorBase + 0x9A8DD, Tmp);
	CompileScripts = (CompileScripts_Func)(EditorBase + 0x93E20); //Get CompileScripts global/static

	ForceAssign( FScriptCompiler_XC::FindField, Tmp); //Trampoline FScriptCompiler::FindField into our version
	EncodeJump( EditorBase + 0x964C0, Tmp);
}


static UBOOL CompileScripts_Proxy( TArray<UClass*>& ClassList, FScriptCompiler_XC* Compiler, UClass* Class )
{
	// Top call
	UBOOL Result = 1;
	if ( Class == UObject::StaticClass() )
	{
		TArray<UClass*> ImportantClasses;
		static const TCHAR* ImportantClassNames[] = { TEXT("XC_Engine_Actor"), TEXT("XC_EditorLoader")};

		for ( int i=0 ; i<ClassList.Num() ; i++ )
		for ( int j=0 ; j<ARRAY_COUNT(ImportantClassNames) ; j++ )
			if ( !appStricmp( ClassList(i)->GetName(), ImportantClassNames[j]) )
			{
				if ( j==0 ) //Needs Actor!
					ImportantClasses.AddUniqueItem( AActor::StaticClass() );
				ImportantClasses.AddUniqueItem( ClassList(i) );
				break;
			}

		if ( ImportantClasses.Num() )
		{
			Result = (*CompileScripts)(ImportantClasses,Compiler,Class); //UObject
			Helper.Reset();
		}
		if ( Result )
			Result = (*CompileScripts)(ClassList,Compiler,Class);
	}
	return Result;
}


UField* FScriptCompiler_XC::FindField( UStruct* Owner, const TCHAR* InIdentifier, UClass* FieldClass, const TCHAR* P4)
{
//	GWarn->Logf( L"FindField %s", InIdentifier);
	// Normal stuff
	check(InIdentifier);
	FName InName( InIdentifier, FNAME_Find );
	if( InName != NAME_None )
	{
		for( UStruct* St=Owner ; St ; St=Cast<UStruct>( St->GetOuter()) )
			for( TFieldIterator<UField> It(St) ; It ; ++It )
				if( It->GetFName() == InName )
				{
					if( !It->IsA(FieldClass) )
					{
						if( P4 )
							appThrowf( TEXT("%s: expecting %s, got %s"), P4, FieldClass->GetName(), It->GetClass()->GetName() );
						return nullptr;
					}
					return *It;
				}
	}

	// Initialize hardcoded opcodes
	if ( !Helper.bInit )
	{
		Helper.bInit++;
		Helper.AddFunction( ANavigationPoint::StaticClass(), TEXT("describeSpec") );
		UClass* XCGEA = FindObject<UClass>( NULL, TEXT("XC_Engine.XC_Engine_Actor"), 1);
		if ( XCGEA )
		{
			Helper.AddFunction( XCGEA, TEXT("AddToPackageMap"));
			Helper.AddFunction( XCGEA, TEXT("IsInPackageMap"));
			Helper.AddFunction( XCGEA, TEXT("PawnActors"));
			Helper.AddFunction( XCGEA, TEXT("DynamicActors"));
			Helper.AddFunction( XCGEA, TEXT("InventoryActors"));
			Helper.AddFunction( XCGEA, TEXT("CollidingActors"));
			Helper.AddFunction( XCGEA, TEXT("NavigationActors"));
			Helper.AddFunction( XCGEA, TEXT("ConnectedDests"));
			Helper.AddFunction( XCGEA, TEXT("ReplaceFunction"));
			Helper.AddFunction( XCGEA, TEXT("RestoreFunction"));
		}
		UClass* XCEL = FindObject<UClass>( NULL, TEXT("XC_Engine.XC_EditorLoader"), 1);
		if ( XCEL )
		{
			Helper.AddFunction( XCEL, TEXT("MakeColor"));
			Helper.AddFunction( XCEL, TEXT("LoadPackageContents"));
			Helper.AddFunction( XCEL, TEXT("StringToName"));
			Helper.AddFunction( XCEL, TEXT("FindObject"));
			Helper.AddFunction( XCEL, TEXT("GetParentClass"));
			Helper.AddFunction( XCEL, TEXT("AllObjects"));
			Helper.AddFunction( XCEL, TEXT("AppSeconds"));
			Helper.AddFunction( XCEL, TEXT("HasFunction"));
			Helper.AddFunction( XCEL, TEXT("Or_ObjectObject"));
			Helper.AddFunction( XCEL, TEXT("Clock"));
			Helper.AddFunction( XCEL, TEXT("UnClock"));
			Helper.AddFunction( XCEL, TEXT("AppCycles"));
			Helper.AddFunction( XCEL, TEXT("FixName"));
			Helper.AddFunction( XCEL, TEXT("HNormal"));
			Helper.AddFunction( XCEL, TEXT("HSize"));
			Helper.AddFunction( XCEL, TEXT("InvSqrt"));
			Helper.AddFunction( XCEL, TEXT("MapRoutes"));
			Helper.AddFunction( XCEL, TEXT("BuildRouteCache"));
		}
		UClass* GlobalFunctions = FindObject<UClass>( ANY_PACKAGE, TEXT("GlobalFunctions"), 1);
		if ( GlobalFunctions )
		{
			for ( TFieldIterator<UFunction>It(GlobalFunctions) ; It ; ++It )
				if ( It->GetOuter() == GlobalFunctions )
					Helper.AddFunction( GlobalFunctions, It->GetName() );
		}
	}

	if ( !FieldClass )
	{
		while ( Owner && !Owner->IsA(UClass::StaticClass()) )
			Owner = Cast<UStruct>(Owner->GetOuter());
		UBOOL IsActor = Owner && Owner->IsChildOf( AActor::StaticClass() );
		InName = FName( InIdentifier, FNAME_Find ); //Name may not have existed before
		if ( InName != NAME_None )
		{
			if ( IsActor )
				for ( int i=0 ; i<Helper.ActorFunctions.Num() ; i++ )
					if ( Helper.ActorFunctions(i)->GetFName() == InName )
						return Helper.ActorFunctions(i);
			for ( int i=0 ; i<Helper.ObjectFunctions.Num() ; i++ )
				if ( Helper.ObjectFunctions(i)->GetFName() == InName )
					return Helper.ObjectFunctions(i);
			for ( int i=0 ; i<Helper.StaticFunctions.Num() ; i++ )
				if ( Helper.StaticFunctions(i)->GetFName() == InName )
					return Helper.StaticFunctions(i);
		}
	}
	
	return nullptr;
}


