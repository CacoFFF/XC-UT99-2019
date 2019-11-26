/*=============================================================================
	XC_CoreObj.h:
	Generic objects for XC_Core
=============================================================================*/

#ifndef _INC_XC_COREOBJ
#define _INC_XC_COREOBJ

//==============================================
// Generic systems
//==============================================

class XC_CORE_API FGenericSystem : public FExec
{
public:
	//FExec interface
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar );

	//FGenericSystem interface
	virtual UBOOL Init()
	{
		return true;
	}
	virtual INT Tick( FLOAT DeltaSeconds=0.f)
	{
		return 0;
	} 
	virtual void Exit() { }
	virtual ~FGenericSystem() { }

	virtual UBOOL IsTyped( const TCHAR* Type)
	{
		return 0; //return appStricmp( Type, TEXT("SomeName")) == 0;
	}
};

class XC_CORE_API FGenericSystemDispatcher : public FGenericSystem
{
public:
	//FExec interface
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar );

	//FGenericSystem interface
	UBOOL Init();
	INT Tick( FLOAT DeltaSeconds=0.f);
	void Exit();
	UBOOL IsTyped( const TCHAR* Type);

	//FGenericSystemDispatcher interface
	FGenericSystemDispatcher( UBOOL InMultiExec=0)
	:	GenSystems()
	,	MultiExec( InMultiExec)
	{}
	FGenericSystem* FindByType( const TCHAR* Type); //Uses FGenericSystem::IsTyped to find a system of a specific type
	virtual ~FGenericSystemDispatcher();

	TArray<FGenericSystem*> GenSystems;
	UBOOL MultiExec;
};



//==============================================
// Class cache
//==============================================
//A standard cached property
struct XC_CORE_API FPropertyCache
{
	FPropertyCache* Next;
	UProperty* Property;
	FPropertyCache() {};
	FPropertyCache( UProperty* InProperty, FPropertyCache* InNext)
	:	Next( InNext)
	,	Property( InProperty)
	{};
};

//Base type of Class cache, subdefine for different property caching types
//The first object to be added should be the highest intended class
template <class T> struct TClassPropertyCacheHolder;
struct XC_CORE_API FClassPropertyCache
{
	typedef struct TClassPropertyCacheHolder<FClassPropertyCache> Holder;

	FClassPropertyCache* Parent;
	FClassPropertyCache* Next;
	UClass* Class;
	UBOOL bProps;
	FPropertyCache* Properties;

	//Master constructor, create the top class cache here
	FClassPropertyCache( UClass* MasterClass=UObject::StaticClass() );

	//Child constructor, create subclasses cache here
	FClassPropertyCache( FClassPropertyCache* InNext, UClass* InClass);


	//Chained list lookup
	FClassPropertyCache* GetCache( UClass* Other);

	//Property lookup
	UProperty* GetNamedProperty( const TCHAR* PropertyName);

	inline UBOOL IsCached( UClass* Other)
	{
		return GetCache( Other) != NULL;
	}
	
	virtual UBOOL AcceptProperty( UProperty* Property)
	{
		return true;
	}

	void GrabProperties( FMemStack& Mem);
};

//Templated holder
template <class T> struct TClassPropertyCacheHolder
{
	UClass* TopClass;
	FMemStack* Mem;
	FMemMark Mark;
	T* List;

	TClassPropertyCacheHolder( UClass* RootClass, FMemStack* MemStack)
	{
		TopClass = RootClass;
		Mem = MemStack;
		Mark = FMemMark(*Mem);
		List = new(*Mem) T(RootClass);
		List->GrabProperties(*Mem);
	}

	~TClassPropertyCacheHolder()
	{
		Mark.Pop();
	}

	T* GetCache( UClass* Class)
	{
		if ( Class && Class->IsChildOf(TopClass) )
			return SetupCache(Class);
		return nullptr;
	}

private:
	//Top class check passed
	T* SetupCache( UClass* Class)
	{
		T* Found = (T*)List->GetCache( Class);
		if ( !Found )
		{
			T* Parent = SetupCache( Class->GetSuperClass() );
			check(Parent);
			List = new(*Mem) T( List, Class);
			List->GrabProperties(*Mem);
			Found = List;
		}
		return Found;
	}

private:
	TClassPropertyCacheHolder();
};

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
