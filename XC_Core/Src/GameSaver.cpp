
#include "XC_Core.h"
#include "Engine.h"
#include "UnLinker.h"
#include "XC_GameSaver.h"

#define CURRENT_FILE_VERSION 1
static int32 FileVersion = CURRENT_FILE_VERSION;
static FName FNAME_None( NAME_None);
static struct FSaveFile* GSaveFile;

static FNameEntry* GetNameEntry( FName& N)
{
	if ( N.IsValid() )
		return FName::GetEntry( N.GetIndex() );
	return nullptr;
}

struct FArchiveSetError : public FArchive
{
	static void Error( FArchive& Ar)
	{
		((FArchiveSetError&)Ar).ArIsError = 1;
	}
};

struct FPropertySaver
{
	UObject* Object;
	int32 Offset;
	TArray<uint8> Data;

	FPropertySaver( UObject* InObject, int32 InOffset, int32 InSize)
		: Object(InObject), Offset(InOffset), Data(InSize)
	{
		appMemcpy( Data.GetData(), (void*)((PTRINT)Object + Offset), InSize);
	}

	~FPropertySaver()
	{
		appMemcpy( (void*)((PTRINT)Object + Offset), Data.GetData(), Data.Num() );
	}
};
#define OFFSET_AND_SIZE(STR,PROP) STRUCT_OFFSET(STR,PROP), sizeof(STR::PROP)

static void FixStruct( UStruct* Struct)
{
	for ( UProperty* Prop=Struct->PropertyLink ; Prop ; Prop=Prop->PropertyLinkNext )
	{
		if ( Prop->Offset == 0 )
		{
			UStruct* Container = Cast<UStruct>( Prop->GetOuter() );
			if ( Container && Container->GetSuperStruct() )
			{
				if ( !Container->GetSuperStruct()->PropertiesSize )
					FixStruct( Container->GetSuperStruct() );
				if ( Container->GetSuperStruct()->PropertiesSize )
				{
					debugf( TEXT("ZERO PROP %s >>> %s (%s)"), Prop->GetPathName(), *FObjectPathName(Prop->PropertyLinkNext), Container->GetPathName() );
					FArchive ArDummy;
					Struct->Link( ArDummy, 1);
				}
			}
		}
		if ( Cast<UStructProperty>(Prop) )
			FixStruct( ((UStructProperty*)Prop)->Struct );
	}
}


/*-----------------------------------------------------------------------------
	Save file private containers
-----------------------------------------------------------------------------*/

struct FIndexedNameArray : protected TArray<FName>
{
	using FArray::Num;
	using TArray<FName>::operator();
	using FArray::IsValidIndex;

	int32 GetIndex( FName Name);

	friend FArchive& operator<<( FArchive& Ar, FIndexedNameArray& A);
};

struct FSaveGameElement
{
	UObject* Object;
	FName Name;

	DWORD ObjectFlags;
	int32 NameIndex;
	int32 ClassIndex;
	int32 WithinIndex;

	int8 DataQueued;
	int8 IsImport;
	TArray<uint8> Data;

	FSaveGameElement() {}
	FSaveGameElement( UObject* InObject, class FArchiveGameSaverTag& GameSaverTag);

	friend FArchive& operator<<( FArchive& Ar, FSaveGameElement& S);
};

struct FCompactReachSpec
{
	INT distance, Start, End, CollisionRadius, CollisionHeight, reachFlags; 
	BYTE  bPruned;

	friend FArchive& operator<<(FArchive &Ar, FCompactReachSpec &R );

	void From( const FReachSpec& Spec, struct FSaveFile& SaveFile);
	void To( FReachSpec& Spec, struct FSaveFile& SaveFile) const;
};


struct FSaveFile
{
	FSaveSummary             Summary;
	TMap<FString,FString>    TravelInfo;
	FIndexedNameArray        Names;
	TArray<FSaveGameElement> Elements;
	TArray<int32>            ActorList;
	TArray<FCompactReachSpec> ReachSpecs;

	ULevel* Level;

	int32 GetSavedIndex( UObject* Object);
	UObject* GetSavedObject( int32 SavedIndex);

	friend FArchive& operator<<( FArchive& Ar, FSaveFile& S);
};



/*-----------------------------------------------------------------------------
	Save file I/O
-----------------------------------------------------------------------------*/

static void SaveName( FArchive& Ar, const TCHAR* String)
{
	int32 i = 0;
	uint8 Data[NAME_SIZE];
	while ( *String )
	{
		Data[i] = (uint8)((*String++) & 0xFF);
		if ( !Data[i] )
			Data[i] = '_'; //Hack fix just in case for chars
		i++;
	}
	Data[i++] = '\0';
	Ar.Serialize( Data, i);
}

static bool LoadName( FArchive& Ar, TCHAR* String)
{
	int32 i = 0;
	uint8 Data[NAME_SIZE];
	while ( i < NAME_SIZE )
	{
		Ar.Serialize( &Data[i], 1);
		if ( Data[i] == '\0' )
		{
			for ( int32 j=0 ; j<=i ; j++ )
				String[j] = (TCHAR)Data[j];
			return true;
		}
		i++;
	}
	FArchiveSetError::Error(Ar);
	return false;
}

FArchive& operator<<( FArchive& Ar, FIndexedNameArray& A)
{
	guard(FIndexedNameArray::<<);
	int32 NameCount = A.Num();
	Ar << AR_INDEX(NameCount);
	if ( Ar.IsLoading() )
	{
		A.Empty();
		A.Add(NameCount);
		TCHAR SerializedName[NAME_SIZE];
		for ( int32 i=0 ; i<NameCount ; i++ )
		{
			if ( !LoadName(Ar,SerializedName) )
				break;
			A(i) = FName( SerializedName );
		}
	}
	else if ( Ar.IsSaving() )
	{
		for ( int32 i=0 ; i<NameCount ; i++ )
			SaveName( Ar, *A(i) );
	}
	else
	{
		for ( int32 i=0 ; i<NameCount ; i++ )
			Ar << A(i);
	}
	return Ar;
	unguard;
}

FArchive& operator<<( FArchive& Ar, FSaveGameElement& S)
{
	return Ar << S.ObjectFlags << AR_INDEX(S.NameIndex) << AR_INDEX(S.ClassIndex) << AR_INDEX(S.WithinIndex) << S.IsImport << S.Data;
}

FArchive& operator<<( FArchive& Ar, FSaveSummary& S)
{
	FileVersion = CURRENT_FILE_VERSION;
	uint8 Header[] = { 'U', 'S', 'X', CURRENT_FILE_VERSION };
	Ar.Serialize( Header, sizeof(Header));
	if ( Ar.IsLoading() )
	{
		if ( Header[0]!='U' || Header[1]!='S' || Header[2]!='X' || Header[3]==0 || Header[3]>CURRENT_FILE_VERSION )
			Ar.Close();
		FileVersion = Header[3];
	}
	return Ar << S.Players << S.LevelTitle << S.Notes << S.URL << S.GUID;
}

FArchive& operator<<(FArchive &Ar, FCompactReachSpec &R )
{
	Ar << AR_INDEX(R.distance) << AR_INDEX(R.Start) << AR_INDEX(R.End) << AR_INDEX(R.CollisionRadius) << AR_INDEX(R.CollisionHeight);
	Ar << AR_INDEX(R.reachFlags) << R.bPruned;
	return Ar;
};

FArchive& operator<<( FArchive& Ar, FSaveFile& S)
{
	Ar << S.Summary;
	if ( !Ar.IsError() )
		Ar << S.TravelInfo << S.Names << S.Elements << *(TArray<FCompactIndex>*)&S.ActorList << *(TArray<FCompactReachSpec>*)&S.ReachSpecs;
	return Ar;
}

/*-----------------------------------------------------------------------------
	Save file data logic
-----------------------------------------------------------------------------*/

UBOOL FSaveSummary::IsValid()
{
	return URL.Map != TEXT("");
}

//
// Returns name index, addes new one
//
int32 FIndexedNameArray::GetIndex( FName N)
{
	guard(FIndexedNameArray::GetIndex);
	FNameEntry* Entry = GetNameEntry(N);
	if ( !Entry )
		return 0;
	if ( Entry->Flags & RF_TagExp )
	{
		int32 Index = FindItemIndex(N);
		check(Index != INDEX_NONE);
		return Index;
	}
	Entry->Flags |= RF_TagExp;
	return AddItem(N);
	unguard;
}

//
// ReachSpec treatment
//
void FCompactReachSpec::From( const FReachSpec& Spec, FSaveFile& SaveFile)
{
	distance = Spec.distance;
	Start = SaveFile.GetSavedIndex(Spec.Start);
	End = SaveFile.GetSavedIndex(Spec.End);
	CollisionRadius = Spec.CollisionRadius;
	CollisionHeight = Spec.CollisionHeight;
	reachFlags = Spec.reachFlags; 
	bPruned = Spec.bPruned;
}
void FCompactReachSpec::To( FReachSpec& Spec, FSaveFile& SaveFile) const
{
	Spec.distance = distance;
	Spec.Start = Start ? (AActor*)SaveFile.Elements(Start-1).Object : nullptr;
	Spec.End = End ? (AActor*)SaveFile.Elements(End-1).Object : nullptr;
	Spec.CollisionRadius = CollisionRadius;
	Spec.CollisionHeight = CollisionHeight;
	Spec.reachFlags = reachFlags; 
	Spec.bPruned = bPruned;
}

//
// Returns import index
//
int32 FSaveFile::GetSavedIndex( UObject* Object)
{
	if ( Object && (Object->GetFlags() & RF_TagExp) )
	{
		for ( int32 i=0 ; i<Elements.Num() ; i++ )
			if ( Elements(i).Object == Object )
				return i+1;
	}
	return 0;
}

UObject* FSaveFile::GetSavedObject( int32 SavedIndex)
{
	guard(FSaveFile::GetSavedObject);
	if ( !SavedIndex )
		return nullptr;
	check(Elements.IsValidIndex(SavedIndex-1));
	return Elements(SavedIndex-1).Object;
	unguard;
}

/*-----------------------------------------------------------------------------
	Serializers
-----------------------------------------------------------------------------*/

//
// Soft Linker, uses the Linker's loader to grab data without modifying the UObject system
//
class FArchiveLinkerSerializer : public FArchive
{
public:
	ULinkerLoad* Linker;
	UObject* SerializeInto;
	int32 Pos;
	TArray<BYTE> Buffer;

	UObject* RealObject;
	UBOOL bLog;

	FArchiveLinkerSerializer( UObject* RealObject, UObject* FakeDefaults)
	{
		ArIsLoading = 1;
		ArIsPersistent = 1;

		this->RealObject = RealObject; //Kept for logging
		bLog = /*RealObject->IsA(ALevelInfo::StaticClass())*/0;

		Linker = RealObject->GetLinker();
		check(Linker);
		check(Linker->Loader);
		int32 SavedPos = Linker->Loader->Tell();
		FObjectExport& Export = Linker->ExportMap( RealObject->GetLinkerIndex() );
		Linker->Loader->Seek( Export.SerialOffset );
		Linker->Loader->Precache( Export.SerialSize );
		Pos = 0;
		Buffer.AddZeroed( Export.SerialSize + 32); //Extra size can't hurt
		Linker->Loader->Serialize( Buffer.GetData(), Export.SerialSize);
		Linker->Loader->Seek( SavedPos );

		SerializeInto = FakeDefaults;
		*(PTRINT*)SerializeInto = *(PTRINT*)RealObject; //Copy vtable
		SerializeInto->SetClass( RealObject->GetClass() );
		SerializeInto->SetFlags( RF_NeedLoad | RF_Preloading | (Export.ObjectFlags & RF_HasStack) );
	}


	void Start()
	{
		check(RealObject);

		guard(SerializeImportFromLevel);
		SerializeInto->Serialize( *this );
		SerializeInto->ConditionalPostLoad();
		if ( SerializeInto->GetStateFrame() )
			delete SerializeInto->GetStateFrame();
		unguardf( (TEXT("%s"), RealObject->GetPathName() ) );

	}

	void Serialize( void* V, INT Length ) override
	{
		if ( bLog )	debugf( TEXT("Writing %i to buffer (%i)"), Length, Pos );
		appMemcpy( V, &Buffer(Pos), Length);
		Pos += Length;
	}

	FString GetImportPath( FObjectImport& Import)
	{
		FString TopPath = *Import.ObjectName;
		int32 PackageIndex = -Import.PackageIndex - 1;
		if ( PackageIndex >= 0 )
			TopPath = GetImportPath( Linker->ImportMap(PackageIndex) ) + TEXT(".") + TopPath;
		return TopPath;
	}

	FArchive& operator<<( UObject*& Object )
	{
		INT Index;
		*this << AR_INDEX(Index);
		Object = nullptr;
		if ( Index > 0 ) //This is one of the level's exports
		{
			Index--;
			Object = Linker->ExportMap( Index )._Object;
			if ( !Object || Object->IsPendingKill() )
			{
				Object = GSys; //HACK TO FORCE A DELTA!
//				debugf( TEXT("Forcing deleted delta in for export on %s! (%s)"), RealObject->GetName(), *Linker->ExportMap( Index ).ObjectName );
			}
		}
		else if ( Index < 0 ) //This is one of the level's imports
		{
			Index = -Index - 1;
			FObjectImport& Import = Linker->ImportMap( Index );
			if ( Import.SourceLinker )
			{
				FString ClassPath = FString::Printf( TEXT("%s.%s"), *Import.ClassPackage, *Import.ClassName);
				UClass* Class = (UClass*)UObject::StaticFindObject( UClass::StaticClass(), nullptr,*ClassPath, 1);
				FString TopPath = GetImportPath( Import );
				Object = UObject::StaticFindObject( Class, nullptr, *TopPath, Class != nullptr);
				if ( !Object )
					debugf( TEXT("Failed to locate object %s"), *TopPath);
			}
			if ( !Object )
			{
				debugf( TEXT("Failed to locate import %s (%s.%s) %i"), *Import.ObjectName, *Import.ClassPackage, *Import.ClassName, Import.PackageIndex);
				Object = GSys; //HACK TO FORCE A DELTA!
			}
		}
		return *this;
	}
	FArchive& operator<<( FName& Name )
	{
		if ( bLog )	debugf( TEXT("... Getting indexed name %s"), *Name);
		NAME_INDEX NameIndex;
		*this << AR_INDEX(NameIndex);
		if( !Linker->NameMap.IsValidIndex(NameIndex) )
			appErrorf( TEXT("Bad name index %i/%i"), NameIndex, Linker->NameMap.Num() );	
		Name = Linker->NameMap( NameIndex );
		return *this;
	}

};

//
// Game loader
//
class FArchiveGameLoader : public FArchive
{
public:
	FSaveFile* SaveData;
	TArray<uint8>* Buffer;
	int32 Pos;
	UBOOL bLog;

	FArchiveGameLoader( FSaveFile* InSaveData, TArray<uint8>* InBuffer)
		: SaveData(InSaveData), Buffer(InBuffer), Pos(0), bLog(0)
	{
		ArIsLoading = 1;
		ArIsPersistent = 1;
	}

	virtual void Serialize( void* V, INT Length)
	{
		guard(FArchiveGameLoader::Serialize);
		if ( Pos + Length > Buffer->Num() )
		{
			if ( Pos <= Buffer->Num() ) //Only log once
				GWarn->Logf( TEXT("FArchiveGameLoader going out of bounds %i -> %i / %i"), Pos, Pos+Length, Buffer->Num() );
			appMemzero( V, Length);
		}
		else
		{
			if ( bLog )
				debugf( TEXT("...Loading %i (%i)"), Length, Pos);
			uint8* Data = (uint8*)Buffer->GetData() + (PTRINT)Pos;
			appMemcpy( V, Data, Length);
		}
		Pos += Length;
		unguard;
	}

	virtual FArchive& operator<<( class FName& N )
	{
		guard(FArchiveGameLoader::Name<<);
		int32 Index;
		*this << AR_INDEX( Index );
		check( SaveData->Names.IsValidIndex(Index) );
		N = SaveData->Names(Index); //TODO: NEEDS ERROR HANDLING
		if ( bLog ) debugf( TEXT(".... Name %s (%i)"), *N, Index);
		return *this;
		unguard;
	}

	virtual FArchive& operator<<( UObject*& Object )
	{
		guard(FArchiveGameLoader::Object<<);
		int32 Index;
		*this << AR_INDEX( Index );
		Object = SaveData->GetSavedObject(Index);
		if ( bLog )
			debugf( TEXT(".... Object %s (%i)"), *FObjectPathName(Object), Index);
		return *this;
		unguard;
	}
};



//
// Common interface of FArchiveGameSaver
// Creates name entries and converts tagged objects to indices
//
class FArchiveGameSaver : public FArchive
{
public:
	FSaveFile* SaveData;
	UBOOL bLog;

	FArchiveGameSaver( FSaveFile* InSaveData)
		: SaveData(InSaveData), bLog(0)
	{
		ArIsSaving = 1;
		ArIsPersistent = 1;
	}

	virtual FArchive& operator<<( class FName& N )
	{
		if ( bLog )
			debugf( TEXT("... Getting indexed name %s"), *N);
		int32 Index = SaveData->Names.GetIndex(N);
		return *this << AR_INDEX( Index);
	}

	virtual FArchive& operator<<( UObject*& Object )
	{
		if ( bLog )
			debugf( TEXT("... Getting indexed object %s"), *FObjectPathName(Object) );
		int32 Index = SaveData->GetSavedIndex( Object );
		return *this << AR_INDEX( Index);
	}

	virtual INT MapObject( UObject* Object )
	{
		if ( bLog )
			debugf( TEXT("... Getting mapped object %s"), *FObjectPathName(Object) );
		return (Object && (Object->GetFlags() & (RF_TagExp))) ? SaveData->GetSavedIndex(Object) : 0;
	}
};

//
// Saves data to a local buffer
//
class FArchiveGameSaverBuffer : public FArchiveGameSaver
{
public:
	TArray<uint8> Buffer;

	FArchiveGameSaverBuffer( FSaveFile* InSaveData)
		: FArchiveGameSaver(InSaveData)
	{}

	virtual void Serialize( void* V, INT Length )
	{
		if ( bLog )
			debugf( TEXT("Writing %i to buffer (%i)"), Length, Buffer.Num() );
		int32 Offset = Buffer.Add( Length );
		appMemcpy( &Buffer(Offset), V, Length);
	}
};


//
// Tags objects and names and generates their save headers
//
class FArchiveGameSaverTag : public FArchiveGameSaver
{
public:
	using FArchiveGameSaver::FArchiveGameSaver;

	virtual FArchive& operator<<( UObject*& Object )
	{
		guard(FArchiveGameSaverTag::<<);
		if ( Object && !(Object->GetFlags() & RF_TagExp) && !Object->IsPendingKill() )
		{
			Object->SetFlags(RF_TagExp);
			if ( !Object->IsIn(UObject::GetTransientPackage()) )
			{
				if ( !(Object->GetFlags() & RF_Transient) || Object->IsA(UField::StaticClass()) )
				{
					//Automatically adds super imports
					FSaveGameElement Element( Object, *this); 
					int32 Index = SaveData->Elements.AddZeroed();
					ExchangeRaw( SaveData->Elements(Index), Element);

					//Continue tagging via recursion
					if ( Object->IsIn(SaveData->Level->GetOuter()) )
						Object->Serialize( *this );
				}
			}
		}
		return *this;
		unguardf(( TEXT("(%s)"), Object->GetFullName() ));
	}
};

//
// Saves tagged objects' data and queues more objects for saving.
//
class FArchiveGameSaverProperties : public FArchiveGameSaverBuffer
{
public:
	int32 i;
	TArray<int32> ObjectIndices;

	FArchiveGameSaverProperties( FSaveFile* InSaveData)
		: FArchiveGameSaverBuffer(InSaveData), i(0)
	{}

	virtual FArchive& operator<<( UObject*& Object)
	{
		if ( bLog )	debugf( TEXT("... Getting indexed object %s"), *FObjectPathName(Object) );
		int32 Index = SaveData->GetSavedIndex(Object);
		if ( Index )
			AddObject( Index );
		return *this << AR_INDEX( Index);
	}

	bool AddObject( int32 Index)
	{
		Index--;
		check( SaveData->Elements.IsValidIndex(Index) );
		FSaveGameElement& Element = SaveData->Elements(Index);
		if ( !Element.Object || Element.DataQueued || !Element.Object->IsIn(SaveData->Level->GetOuter())
			|| Element.Object->IsA(UClass::StaticClass() )
			|| Element.Object->IsA(UPrimitive::StaticClass() )
			|| Element.Object->IsA(UBitmap::StaticClass()) ) //May break scripted textures!!
			return false;

		Element.DataQueued = 1;
		ObjectIndices.AddItem(Index);
		return true;
	}

	void ProcessObjects()
	{
		guard(FArchiveGameSaverProperties::ProcessObjects)
		for ( ; i<ObjectIndices.Num() ; i++ )
		{
			check( SaveData->Elements.IsValidIndex(ObjectIndices(i)) );
			TArray<uint8> SavedDefaults;
			FSaveGameElement& Element = SaveData->Elements( ObjectIndices(i) );
			UObject* Object = Element.Object;
			check(Object);
			if ( bLog ) debugf( TEXT("Process object: %s"), Object->GetPathName() );

			//Temporarily swap defaults so we obtain a delta of the Import instead of class defaults.
			Element.IsImport = Object->GetLinker() && (Object->GetLinkerIndex() != INDEX_NONE);
			if ( Element.IsImport )
			{
				SavedDefaults.AddZeroed( Object->GetClass()->PropertiesSize );
				for ( TFieldIterator<UProperty>It(Object->GetClass()) ; It ; ++It )
					if ( !(It->PropertyFlags & CPF_Transient) && (It->Offset >= sizeof(UObject)) )
						It->CopyCompleteValue( &SavedDefaults(It->Offset), &Object->GetClass()->Defaults(It->Offset));
				FArchiveLinkerSerializer ArLinker( Object, (UObject*)&SavedDefaults(0) );
				ArLinker.Start();
				ExchangeRaw( Object->GetClass()->Defaults, SavedDefaults);
			}
			
			Object->Serialize( *this );
			ExchangeRaw( Buffer, Element.Data);
			Buffer.Empty();
			if ( bLog )	debugf( TEXT("Serialized %i bytes of %s"), Element.Data.Num(), Object->GetPathName() );

			if ( Element.IsImport )
			{
				ExchangeRaw( Object->GetClass()->Defaults, SavedDefaults);
				UObject::ExitProperties( &SavedDefaults(0), Object->GetClass() );
			}
		}
		unguard;
	}
	
};

/*-----------------------------------------------------------------------------
	Save game element
-----------------------------------------------------------------------------*/

FSaveGameElement::FSaveGameElement( UObject* InObject, FArchiveGameSaverTag& GameSaverTag)
	: Object(InObject)
	, Name(InObject->GetFName())
	, ObjectFlags(InObject->GetFlags() & RF_Load)
	, DataQueued(0)
	, IsImport(2)
{
	// Tag the name (manually)
	NameIndex = GameSaverTag.SaveData->Names.GetIndex( Name );

	// Tag the class
	UClass* Class = Object->GetClass();
	GameSaverTag << Class;
	ClassIndex = GameSaverTag.SaveData->GetSavedIndex( Class );

	// Tag the outer
	UObject* Outer = Object->GetOuter();
	if ( Outer )
		GameSaverTag << Outer;
	WithinIndex = GameSaverTag.SaveData->GetSavedIndex( Outer );
}


/*-----------------------------------------------------------------------------
	Main interfaces
-----------------------------------------------------------------------------*/

namespace XC
{

XC_CORE_API void GetSaveGameList( TArray<FSaveSummary>& SaveList)
{
	FString Path = GSys->SavePath;
	Path += PATH_SEPARATOR TEXT("*.usx");
	TArray<FString> FileList = GFileManager->FindFiles( *Path, 1, 0);

	SaveList.Empty();
	for ( int32 i=0 ; i<FileList.Num() ; i++ )
	{
		FSaveSummary Summary;
		if ( LoadSaveGameSummary( Summary, *FileList(i)) )
		{
			int32 Index = SaveList.AddZeroed();
			ExchangeRaw( SaveList(Index), Summary);
		}
	}
}

XC_CORE_API UBOOL LoadSaveGameSummary( FSaveSummary& SaveSummary, const TCHAR* FileName)
{
	guard(XC::LoadSaveGameSummary);
	UBOOL bSuccess = 0;
	FArchive* Ar = GFileManager->CreateFileReader( FileName, 0);
	if ( Ar && Ar->TotalSize() && !Ar->IsError() )
	{
		FSaveSummary Summary;
		*Ar << Summary;
		if ( !Ar->IsError() && Summary.IsValid() )
		{
			bSuccess = 1;
			ExchangeRaw( SaveSummary, Summary);
		}
	}
	delete Ar;
	return bSuccess;
	unguard;
}



XC_CORE_API UBOOL SaveGame( ULevel* Level, const TCHAR* FileName, uint32 SaveFlags)
{
	guard(XC::SaveGame);

	if ( !Level->GetLinker() )
	{
		debugf( TEXT("No linker!"));
		return 0;
	}

/*	TArray<FPropertySaver> PropertyBackups;
	new(PropertyBackups) FPropertySaver( Level->GetLevelInfo(), OFFSET_AND_SIZE(ALevelInfo,Summary));
	Level->GetLevelInfo()->Summary = nullptr;*/

	guard(UntagInitial);
	TArray<APlayerPawn*> Players;
	TArray<AGameInfo*> Games;
	for( FObjectIterator It; It; ++It )
	{
		if ( It->IsA(AStatLog::StaticClass()) || //Do not export these classes
			It->IsA(ULevelSummary::StaticClass()) ) 
			It->SetFlags( RF_TagExp );
		//Don't save Game and derivates
		else if ( (SaveFlags & SAVE_NoGame) && It->IsA(AGameInfo::StaticClass()) )
			Games.AddItem( (AGameInfo*)*It);
		//Don't save Players and derivates
		else if ( (SaveFlags & SAVE_NoGame) && It->IsA(APlayerPawn::StaticClass()) )
			Players.AddItem( (APlayerPawn*)*It);
		//Don't save mutators (except embedded)
		else if ( (SaveFlags & SAVE_NoGame) && It->IsA(AMutator::StaticClass()) && !It->GetLinker() && (It->GetLinkerIndex() == INDEX_NONE) )
			It->SetFlags( RF_TagExp );
		else
			It->ClearFlags( RF_TagExp );
	}
	for ( int32 i=0 ; i<Players.Num() ; i++ )
	{
		Players(i)->SetFlags( RF_TagExp );
		if ( Players(i)->myHUD ) Players(i)->myHUD->SetFlags( RF_TagExp );
		if ( Players(i)->Scoring ) ((UObject*)Players(i)->Scoring)->SetFlags( RF_TagExp );
		if ( Players(i)->PlayerReplicationInfo ) Players(i)->PlayerReplicationInfo->SetFlags( RF_TagExp );
		for ( AInventory* Inv=Players(i)->Inventory ; Inv ; Inv=Inv->Inventory )
			Inv->SetFlags( RF_TagExp );
	}
	for ( int32 i=0 ; i<Games.Num() ; i++ )
	{
		Games(i)->SetFlags( RF_TagExp );
		if ( Games(i)->GameReplicationInfo )
			Games(i)->GameReplicationInfo->SetFlags( RF_TagExp );
	}

	for( int32 i=0; i<FName::GetMaxNames(); i++ )
		if( FName::GetEntry(i) )
			FName::GetEntry(i)->Flags &= ~(RF_TagExp);
	unguard;

	FSaveFile SaveFile;
	GSaveFile = &SaveFile;
	SaveFile.Level = Level;
//	SaveFile.Summary.Players;
	SaveFile.Summary.LevelTitle = Level->GetLevelInfo()->Title;
//	SaveFile.Summary.Notes;

	//Treat the game's URL
	SaveFile.Summary.URL = Level->URL;
	SaveFile.Summary.URL.Host = TEXT("");
	SaveFile.Summary.URL.Port = 0;
	for ( int32 i=SaveFile.Summary.URL.Op.Num()-1 ; i>=0 ; i-- )
	{
		if ( !appStrnicmp(*SaveFile.Summary.URL.Op(i),TEXT("name=")) ||
			!appStrnicmp(*SaveFile.Summary.URL.Op(i),TEXT("listen")) ||
			!appStrnicmp(*SaveFile.Summary.URL.Op(i),TEXT("checksum=")) )
			SaveFile.Summary.URL.Op.Remove(i);
		if ( ( SaveFlags & SAVE_NoGame ) &&
			(	!appStrnicmp(*SaveFile.Summary.URL.Op(i),TEXT("class=")) ||
				!appStrnicmp(*SaveFile.Summary.URL.Op(i),TEXT("skin=")) ||
				!appStrnicmp(*SaveFile.Summary.URL.Op(i),TEXT("face=")) ||
				!appStrnicmp(*SaveFile.Summary.URL.Op(i),TEXT("overrideclass=")) ||
				!appStrnicmp(*SaveFile.Summary.URL.Op(i),TEXT("voice=")) ||
				!appStrnicmp(*SaveFile.Summary.URL.Op(i),TEXT("team=")) ))
			SaveFile.Summary.URL.Op.Remove(i);
	}
	SaveFile.Summary.GUID = ((ULinker*)Level->GetLinker())->Summary.Guid;
	SaveFile.TravelInfo = Level->TravelInfo;

	guard(Tag);
	FArchiveGameSaverTag ArTag( &SaveFile );
	(FArchive&)ArTag << FNAME_None; //MSVC being dumb
	ArTag << Level;
	unguard;

	guard(Process);
	FArchiveGameSaverProperties ArProp( &SaveFile );
	for ( int32 i=0 ; i<Level->Actors.Num() ; i++ )
	{
		int32 Index = SaveFile.GetSavedIndex(Level->Actors(i));
		SaveFile.ActorList.AddItem(Index);
		if ( Index && ArProp.AddObject(Index) )
			ArProp.ProcessObjects();
	}
	unguard;

	guard(ReachSpecs)
	int32 SpecCount = Level->ReachSpecs.Num();
	SaveFile.ReachSpecs.SetSize( SpecCount );
	for ( int32 i=0 ; i<SpecCount ; i++ )
		SaveFile.ReachSpecs(i).From( Level->ReachSpecs(i), SaveFile);
	unguard;

	guard(SaveToFile);
	GFileManager->MakeDirectory( *GSys->SavePath, 1);
	FArchive* Saver = GFileManager->CreateFileWriter( FileName, FILEWRITE_EvenIfReadOnly);
	*Saver << SaveFile;
	UBOOL IsError = Saver->IsError();
	delete Saver;
	return !IsError;
	unguard;

	unguard;
}

XC_CORE_API UBOOL LoadGame( ULevel* Level, const TCHAR* FileName)
{
	if ( !FileName )
		return false;

	FSaveFile SaveFile;
	GSaveFile = &SaveFile;
	guard(LoadFromFile);
	FArchive* Loader = GFileManager->CreateFileReader( FileName, FILEWRITE_EvenIfReadOnly);
	if ( !Loader || !Loader->TotalSize() || Loader->IsError() )
	{
		debugf( /*NAME_DevLoad,*/ TEXT("LoadGame error: failed loading %s"), FileName);
		return false;
	}
	*Loader << SaveFile;
	delete Loader;
	if ( !SaveFile.Summary.IsValid() || (SaveFile.ActorList.Num() <= 0) )
	{
		debugf( /*NAME_DevLoad,*/ TEXT("LoadGame error: save file %s has bad data"), FileName);
		return false;
	}
	unguard;

	guard(SetupGeneral);
	Level->URL = SaveFile.Summary.URL;
	Level->TravelInfo = SaveFile.TravelInfo;
	unguard;

	//Names are already autogenerated during load process
	guard(SetupImports);
	UBOOL bFoundUPackage = 0;
	for ( int32 i=0 ; i<SaveFile.Elements.Num() ; i++ )
	{
		//Setup name
		FSaveGameElement& Element = SaveFile.Elements(i);
		int32 NameIndex = Element.NameIndex;
		if ( NameIndex < 0 || NameIndex >= SaveFile.Names.Num() )
		{
			GWarn->Logf( TEXT("LoadGame error: found object with invalid name index %i/%i"), NameIndex, SaveFile.Names.Num() );
			return false;
		}
		Element.Name = SaveFile.Names( NameIndex);

		//Hardcode package
		if ( (Element.Name == NAME_Package) && !Element.WithinIndex && !Element.ClassIndex && !bFoundUPackage )
		{
			bFoundUPackage++;
			Element.Object = UPackage::StaticClass();
			continue;
		}

		//Setup outer
		FString PathName;
		FSaveGameElement* ElementPtr = &Element;
		while ( ElementPtr && (ElementPtr->Name != NAME_None) )
		{
			if ( PathName.Len() )
				PathName = FString(TEXT(".")) + PathName;
			PathName = FString(*ElementPtr->Name) + PathName;

			int32 WithinIndex = ElementPtr->WithinIndex;

			//Hack fix, objects and classes in level should already be preloaded
			if ( !WithinIndex && (Element.IsImport == 2) && !appStricmp( *ElementPtr->Name, Level->GetOuter()->GetName()) )
				Element.IsImport = 1;

			ElementPtr = nullptr;
			if ( WithinIndex > SaveFile.Elements.Num() )
			{
				GWarn->Logf( TEXT("LoadGame error: found object with invalid outer index %i/%i"), WithinIndex-1, SaveFile.Elements.Num() );
				return 0;
			}
			else if ( WithinIndex > 0 )
				ElementPtr = &SaveFile.Elements( WithinIndex-1 );
		}

		UClass* Class = nullptr;
		if ( Element.ClassIndex > 0 )
		{
			Class = (UClass*)SaveFile.Elements( Element.ClassIndex-1 ).Object;
			if ( Class && (Class->GetClass() != UClass::StaticClass()) )
			{
				GWarn->Logf( TEXT("LoadGame error: referred class is not a class %s"), Class->GetPathName() );
				return 0;
			}
		}
		else if ( !Element.WithinIndex ) //This is package
			Class = UPackage::StaticClass();

		guard(AssociateObject);
		Element.Object = nullptr;
		UObject* Outer = SaveFile.GetSavedObject(Element.WithinIndex);
		//Attempt exact match
		if ( Outer )
			Element.Object = UObject::StaticFindObject( Class, Outer, *Element.Name, Class != nullptr);
		//Attempt path match
		if ( !Element.Object )
			Element.Object = UObject::StaticFindObject( Class, nullptr, *PathName, Class != nullptr);
		if ( Element.IsImport == 2 )
		{
			if ( !Element.Object && (Class == UPackage::StaticClass()) && !Element.WithinIndex )
				Element.Object = UObject::LoadPackage( nullptr, *PathName, LOAD_Quiet);
			if ( !Element.Object && Class )
				Element.Object = UObject::StaticLoadObject( Class, nullptr, *PathName, nullptr, LOAD_Quiet, nullptr);
			if ( !Element.Object )
				GWarn->Logf( TEXT("LoadGame: failed to load %s (%s) %i"), *PathName, *FObjectPathName(Class), Element.ClassIndex );
		}
		else if ( Element.IsImport == 1 )
		{
			if ( !Element.Object ) //Failed imports don't count
				GWarn->Logf( TEXT("LoadGame: failed import %s (%s)"), *PathName, *FObjectPathName(Class));
		}
		else
		{
			if ( Element.Object )
			{
				if ( Element.Object->GetClass() == Class ) //Attempt to replace
					Element.Object = UObject::StaticConstructObject( Class, Element.Object->GetOuter(), Element.Name, Element.Object->GetFlags() );
				else
				{
					GWarn->Logf( TEXT("LoadGame: unable to overwrite mismatching %s"), *PathName);
					Element.Object = nullptr;
				}
			}
			else if ( Class && Element.WithinIndex && SaveFile.Elements( Element.WithinIndex-1).Object )
			{
				Element.Object = UObject::StaticConstructObject( Class, SaveFile.Elements( Element.WithinIndex-1).Object, Element.Name);
				if ( Class->IsChildOf(AActor::StaticClass()) )
					Element.Object->SetFlags( RF_Transactional );
			}
			else
				GWarn->Logf( TEXT("LoadGame: failed to create object %s"), *PathName);

			//Hardcode XLevel
			if ( Cast<AActor>(Element.Object) && (Element.Object->GetOuter() == Level->GetOuter()) )
				((AActor*)Element.Object)->XLevel = Level;

			//Hardcode flags (risky!)
			if ( Element.Object )
			{
				Element.Object->ClearFlags( RF_Load );
				Element.Object->SetFlags( Element.ObjectFlags & RF_Load );
			}
		}

		if ( appStricmp(*PathName, *FObjectPathName(Element.Object)) )
			debugf( TEXT("Bad object relink: %s to %s (import type %i)"), *PathName, *FObjectPathName(Element.Object), (int32)Element.IsImport );

		unguard;
//		debugf( TEXT("Setting up element %s (%s) as %s"), *PathName, ObjectPathName(Class), ObjectPathName(Element.Object) );
	}
	unguard;

	guard(SetupProperties);
	for ( int32 i=0 ; i<SaveFile.Elements.Num() ; i++ )
	{
		FSaveGameElement& Element = SaveFile.Elements(i);
		if ( Element.Object && Element.Data.Num() ) //TODO: CHECK IF IN LEVEL?
		{
			guard(Element);
			if ( !Element.Object->IsA(UField::StaticClass()) ) //Non-fields should not be serialized (broken script text may actually be serialized!!)
			{
				FixStruct( Element.Object->GetClass() );
//				debugf( TEXT("Loading properties for %s (%i)"), ObjectPathName(Element.Object), Element.Data.Num() );
				Element.Object->ClearFlags( RF_HasStack );
				Element.Object->SetFlags( Element.ObjectFlags & RF_HasStack );
				FArchiveGameLoader ArLoader( &SaveFile, &Element.Data);
				Element.Object->Serialize( ArLoader );
			}
			else if ( Element.Object->GetLinker() )
				Element.Object->GetLinker()->Preload( Element.Object);
			unguardf( (TEXT("%s"), Element.Object->GetPathName()) );
		}
	}
	unguard;

	guard(SetupActorList);
	debugf( TEXT("Setting up Actor list %i -> %i"), Level->Actors.Num(), SaveFile.ActorList.Num());
	int32 ActorCount = SaveFile.ActorList.Num();
	Level->Actors.SetSize( ActorCount );
	for ( int32 i=0 ; i<ActorCount ; i++ )
		Level->Actors(i) = Cast<AActor>(SaveFile.GetSavedObject(SaveFile.ActorList(i))); //TODO: FAIL IF NOT ACTOR
	unguard;

	guard(SetupReachSpecs);
	int32 ReachSpecCount = SaveFile.ReachSpecs.Num();
	Level->ReachSpecs.SetSize( ReachSpecCount );
	for ( int32 i=0 ; i<ReachSpecCount ; i++ )
		SaveFile.ReachSpecs(i).To( Level->ReachSpecs(i), SaveFile);
	unguard;

	debugf( TEXT("LoadGame success"));

	return 1;
}



};

