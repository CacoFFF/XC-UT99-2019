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


class FPropertyBase_XC : public FPropertyBase
{
public:
	typedef FPropertyBase_XC* (FPropertyBase_XC::*Constructor_UProp)(UProperty* PropertyObj);
	static Constructor_UProp FPropertyBase_UProp;
public:

	FPropertyBase_XC* ConstructorProxy_UProp( UProperty* PropertyObj);
};
FPropertyBase_XC::Constructor_UProp FPropertyBase_XC::FPropertyBase_UProp = nullptr;


class FScriptCompiler_XC : public FScriptCompiler
{
public:

	typedef int (FScriptCompiler_XC::*CompileExpr_Func)( FPropertyBase_XC, const TCHAR*, FToken*, int, FPropertyBase_XC*);
	static CompileExpr_Func CompileExpr_Org;

	UField* FindField( UStruct* Owner, const TCHAR* InIdentifier, UClass* FieldClass, const TCHAR* P4);
	int CompileExpr_FunctionParam( FPropertyBase_XC Type, const TCHAR* Error, FToken* Token, int unk_p4, FPropertyBase_XC* unk_p5);

};
FScriptCompiler_XC::CompileExpr_Func FScriptCompiler_XC::CompileExpr_Org = nullptr;

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

	UProperty* LastProperty;
	UFunction* Array_Length;
	UFunction* Array_Insert;
	UFunction* Array_Remove;

	ScriptCompilerHelper_XC_CORE()
		: bInit(0), LastProperty(nullptr), Array_Length(nullptr), Array_Insert(nullptr), Array_Remove(nullptr)
	{}

	void Reset()
	{
		bInit = 0;
		ActorFunctions.Empty();
		ObjectFunctions.Empty();
		StaticFunctions.Empty();
	}

	UFunction* AddFunction( UStruct* InStruct, const TCHAR* FuncName)
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
		return F;
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

	if ( Initialized != 1337 ) //Disable for now
		return 0;

	uint8* Tmp;

	Tmp = EditorBase + 0x90A80; //Get FScriptCompiler::CompileExpr
	ForceAssign( Tmp, FScriptCompiler_XC::CompileExpr_Org);

	Tmp = EditorBase + 0x87EF0; //Get FPropertyBase::FPropertyBase( UProperty*) --- real ---
	ForceAssign( Tmp, FPropertyBase_XC::FPropertyBase_UProp);

	ForceAssign( CompileScripts_Proxy, Tmp); //Proxy CompileScripts initial call
	EncodeCall( EditorBase + 0x9B53D, Tmp);
	CompileScripts = (CompileScripts_Func)(EditorBase + 0x94AA0); //Get CompileScripts global/static

	ForceAssign( FScriptCompiler_XC::FindField, Tmp); //Trampoline FScriptCompiler::FindField into our version
	EncodeJump( EditorBase + 0x97140, Tmp);

	ForceAssign( FScriptCompiler_XC::CompileExpr_FunctionParam, Tmp); //Proxy FScriptCompiler::CompileExpr for function params
	EncodeCall( EditorBase + 0x9378A, Tmp); //above "type mismatch in parameter"

	ForceAssign( FPropertyBase_XC::ConstructorProxy_UProp, Tmp); //Middleman FPropertyBase::FPropertyBase( UProperty*) using it's jumper
	EncodeJump( EditorBase + 0x20B3, Tmp);
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


FPropertyBase_XC* FPropertyBase_XC::ConstructorProxy_UProp( UProperty* PropertyObj)
{
	Helper.LastProperty = PropertyObj;
	(this->*FPropertyBase_UProp)( PropertyObj);
	return this;
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
			Helper.Array_Length = Helper.AddFunction( XCEL, TEXT("Array_Length"));
			Helper.Array_Insert = Helper.AddFunction( XCEL, TEXT("Array_Insert"));
			Helper.Array_Remove = Helper.AddFunction( XCEL, TEXT("Array_Remove"));
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

int FScriptCompiler_XC::CompileExpr_FunctionParam( FPropertyBase_XC Type, const TCHAR* Error, FToken* Token, int unk_p4, FPropertyBase_XC* unk_p5)
{
	guard(CompileExpr_FunctionParam)

	UObject* Container = Helper.LastProperty ? Helper.LastProperty->GetOuter() : nullptr;
	int Result = (this->*CompileExpr_Org)(Type,Error,Token,unk_p4,unk_p5);

	if ( (Result == -1) && Container && (Type.ArrayDim == 0) && (Token->ArrayDim == 0) ) //Dynamic array mismatch, see if we can hardcode behaviour
	{
		if ( Container == Helper.Array_Length || Container == Helper.Array_Insert || Container == Helper.Array_Remove )
			Result = 1; // This is the first parameter of any of these array handlers
	}

	return Result;
	unguard
}


#ifdef _WINDOWS
/*
//
// Add missing definitions not exported in DLL
//

UBOOL FScriptCompiler::GetIdentifier( FToken& Token, INT NoConsts)
{
	if( GetToken( Token, NULL, NoConsts ) )
	{
		if( Token.TokenType == TOKEN_Identifier )
			return 1;
		UngetToken(Token);
	}
	return 0;
}

UBOOL FScriptCompiler::MatchIdentifier( FName Match)
{
	FToken Token;
	if( GetToken(Token) )
	{
		if( (Token.TokenType==TOKEN_Identifier) && Token.TokenName==Match )
			return 1;
		UngetToken(Token);
	}
	return 0;
}

UBOOL FScriptCompiler::MatchSymbol( const TCHAR* Match)
{
	FToken Token;
	if( GetToken(Token,NULL,1) )
	{
		if( Token.TokenType==TOKEN_Symbol && !appStricmp(Token.Identifier,Match) )
			return 1;
		UngetToken(Token);
	}
	return 0;
}

void FScriptCompiler::UngetToken( FToken& Token)
{
	InputPos = Token.StartPos;
	InputLine = Token.StartLine;
}

void FScriptCompiler::RequireSymbol( const TCHAR* Match, const TCHAR* Tag)
{
	if( !MatchSymbol(Match) )
		appThrowf( TEXT("Missing '%s' in %s"), Match, Tag );
}

void FScriptCompiler::CheckAllow( const TCHAR* Thing, DWORD AllowFlags)
{
	if( (TopNest->Allow & AllowFlags) != AllowFlags )
	{
		if( TopNest->NestType==NEST_None )
			appThrowf( TEXT("%s is not allowed before the Class definition"), Thing );
		else
			appThrowf( TEXT("%s is not allowed here"), Thing );
	}
	if( AllowFlags & ALLOW_Cmd )
		TopNest->Allow &= ~(ALLOW_VarDecl | ALLOW_Function | ALLOW_Ignores);
}

UBOOL FScriptCompiler::GetConstInt( INT& Result, const TCHAR* Tag, UStruct* Scope)
{
	FToken Token;
	if ( GetToken(Token) ) 
	{
		if( Token.GetConstInt(Result) )
			return 1;
		else if ( Scope )
		{
			//Not used here
		}
		else
			UngetToken(Token);

		UngetToken(Token);
	}
	if ( Tag )
		appThrowf( TEXT("%s: Missing constant integer") );
	return 0;
}

UClass* FScriptCompiler::GetQualifiedClass( const TCHAR* Thing)
{
	UClass* Result = nullptr;
	TCHAR Buf[256]=TEXT("");
	while ( true )
	{
		FToken Token;
		if( !GetIdentifier(Token) )
			break;
		appStrncat( Buf, Token.Identifier, ARRAY_COUNT(Buf) );
		if( !MatchSymbol(TEXT(".")) )
			break;
		appStrncat( Buf, TEXT("."), ARRAY_COUNT(Buf) );
	}
	if( Buf[0] != '\0' )
	{
		Result = FindObject<UClass>( ANY_PACKAGE, Buf );
		if( !Result )
			appThrowf( TEXT("Class '%s' not found"), Buf);
	}
	else if( Thing )
		appThrowf( TEXT("%s: Missing class name"), Thing);
	return Result;
}*/
#endif