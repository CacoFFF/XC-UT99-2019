/*=============================================================================
	XC_Networking.cpp:
	UT version friendly implementation on networking extensions
=============================================================================*/

#include "XC_Core.h"
#include "Engine.h"
#include "FConfigCacheIni.h"
#include "UnNet.h"
#include "XC_Download.h"
#include "XC_LZMA.h"

#include "Cacus/CacusThread.h"
#include "Cacus/Atomics.h"


void UZDecompress( const TCHAR* SourceFilename, const TCHAR* DestFilename, TCHAR* Error);

/*----------------------------------------------------------------------------
	Asynchronous processor.
----------------------------------------------------------------------------*/

static volatile int32 DownloadTag = 0;
static volatile int32 RecordLock = 0;
struct DownloadRecord
{
	const UXC_Download* Download;
	int32 Tag;

	DownloadRecord( const UXC_Download* InDownload)
		: Download(InDownload), Tag(DownloadTag++ & 0x7FFFFFFF)
	{}
};
static TArray<DownloadRecord> Downloads;

static int32 GetDownloadTag( const UXC_Download* Download)
{
	CSpinLock SL(&RecordLock);
	for ( int32 i=0 ; i<Downloads.Num() ; i++ )
		if ( Downloads(i).Download == Download )
			return Downloads(i).Tag;
	Downloads.AddItem( DownloadRecord(Download) );
	return Downloads.Last().Tag;
}

static void RemoveDownload( const UXC_Download* Download)
{
	CSpinLock SL(&RecordLock);
	for ( int32 i=0 ; i<Downloads.Num() ; i++ )
		if ( Downloads(i).Download == Download )
		{
			Downloads.Remove(i);
			break;
		}
}

static uint32 AsyncProc( void* Arg, CThread* Handler)
{
	FDownloadAsyncProcessor* Processor = (FDownloadAsyncProcessor*)Handler;
	do
	{	while ( Processor->Download->AsyncAction); //Additional TEST operation before WRITE prevents scalability issues
	} while ( FPlatformAtomics::InterlockedCompareExchange(&Processor->Download->AsyncAction, 1, 0) );

	try	{(Processor->Proc)(Processor);}	catch(...){}
	FPlatformAtomics::InterlockedExchange( &UXC_Download::GlobalLock, 0); //Hacky, but saves us from exception

	if ( Processor->DownloadActive() )
		FPlatformAtomics::InterlockedExchange( &Processor->Download->AsyncAction, 0);
	Processor->Detach();
	appSleep( 0.2f ); //Don't immediately kill the processor
	delete Processor;
	return THREAD_END_OK;
}

FDownloadAsyncProcessor::FDownloadAsyncProcessor( const ASYNC_PROC InProc, UXC_Download* InDownload)
	: CThread()
	, Download(InDownload)
	, DownloadTag( GetDownloadTag(InDownload) )
	, Proc(InProc)
{
	Run(&AsyncProc);
	while ( !IsEnded() ) //Don't let main go until thread is detached
		appSleep(0.f);
}

bool FDownloadAsyncProcessor::DownloadActive()
{
	CSpinLock SL(&RecordLock);
	for ( int32 i=0 ; i<Downloads.Num() ; i++ )
		if ( Downloads(i).Tag == DownloadTag )
			return true;
	return false;
}




/*----------------------------------------------------------------------------
	XC_Download.
----------------------------------------------------------------------------*/

volatile int32 UXC_Download::GlobalLock = 0;
FString UXC_Download::NetSizeError;
FString UXC_Download::NetOpenError;
FString UXC_Download::NetWriteError;
FString UXC_Download::NetMoveError;
FString UXC_Download::InvalidUrlError;
FString UXC_Download::ConnectionFailedError;


UXC_Download::UXC_Download()
{
	//Preload thread safe error strings
#define PRELOAD_ERROR(errorname,maxlen) \
	if ( !errorname##Error.Len() ) \
	{ \
		errorname##Error = LocalizeError(TEXT(#errorname),TEXT("Engine")); \
		if ( errorname##Error.Len() > maxlen ) \
			errorname##Error = errorname##Error.Left( maxlen ); \
	}

	PRELOAD_ERROR(NetSize,255);
	PRELOAD_ERROR(NetOpen,255);
	PRELOAD_ERROR(NetWrite,190);
	PRELOAD_ERROR(NetMove,255);
	PRELOAD_ERROR(NetOpen,255);
	PRELOAD_ERROR(InvalidUrl,140);
	PRELOAD_ERROR(ConnectionFailed,255);
}

void UXC_Download::Destroy()
{
	guard(UXC_Download::Destroy);
	CSpinLock SL(&UXC_Download::GlobalLock);
	RemoveDownload(this);
	Super::Destroy();
	unguard;
}

void UXC_Download::Tick()
{
	guard(UXC_Download::Tick);

	if ( Error[0] )
		Finished = 1;

	int32 DownloadSize = RealFileSize ? RealFileSize : Info->FileSize;
	if ( !Finished && (OldTransfered != Transfered) )
	{
		int32 FileCount = 0;
		for ( int32 i=0; i<Connection->PackageMap->List.Num(); i++)
			if (Connection->PackageMap->List(i).PackageFlags & PKG_Need)
				FileCount++;
		
		FString Msg1 = FString::Printf( (Info->PackageFlags&PKG_ClientOptional)?LocalizeProgress(TEXT("ReceiveOptionalFile"),TEXT("Engine")):LocalizeProgress(TEXT("ReceiveFile"),TEXT("Engine")), Info->Parent->GetName() );
		FString Msg2 = FString::Printf( LocalizeProgress(TEXT("ReceiveSize"),TEXT("Engine")), DownloadSize/1024, 100.f*Transfered/DownloadSize, Transfered/1024, FileCount-1 );
		Connection->Driver->Notify->NotifyProgress( *Msg1, *Msg2, 4.f );
		OldTransfered = Transfered;
	}
	if ( !Finished && !AsyncAction )
	{
		//*******************************************************
		//File has been downloaded, and receiver has been closed.
		if ( (Transfered >= DownloadSize) && !RecvFileAr )
		{
			if ( IsCompressed )
			{
				FString Msg1 = FString::Printf( LocalizeProgress(TEXT("DecompressFile"),TEXT("XC_Core")), Info->Parent->GetName() );
				FString Msg2 = FString::Printf( TEXT("%s: %iK > %iK"), (IsLZMA ? TEXT("LZMA") : TEXT("UZ")), Transfered/1024, Info->FileSize/1024 );
				Connection->Driver->Notify->NotifyProgress( *Msg1, *Msg2, 4.f );
			}
			new FDownloadAsyncProcessor( [](FDownloadAsyncProcessor* Proc)
			{
				//STAGE 1, setup local environment.
				BYTE IsCompressed = Proc->Download->IsCompressed;
				BYTE IsLZMA = Proc->Download->IsLZMA;
				FString Guid = Proc->Download->Info->Guid.String();
				FString TempFilename = Proc->Download->TempFilename;
				FString DestFilename = ((GSys->CachePath + PATH_SEPARATOR) + Guid) + GSys->CacheExt;
				TCHAR Error[256] = TEXT("");

				//STAGE 2, let main go (no longer safe to use Download from now on)
				Proc->Detach();
				if ( !GFileManager->FileSize( *TempFilename ) )
					appStrcpy( Error, *UXC_Download::NetOpenError );
				else if ( IsCompressed )
				{
					if ( IsLZMA ) LzmaDecompress( *TempFilename, *DestFilename, Error);
					else          UZDecompress( *TempFilename, *DestFilename, Error);
				}
				else if ( !GFileManager->Move( *DestFilename, *TempFilename, 1) )
					appStrcpy( Error, *UXC_Download::NetMoveError);

				//STAGE 3, validate downloader and lock
				CSpinLock SL(&UXC_Download::GlobalLock);
				if ( !Proc->DownloadActive() )
					return;
				appStrcpy( Proc->Download->Error, Error);
				Proc->Download->Finished = 1;
			}, this);
		}
		else if ( (Transfered > 0) && (Transfered < DownloadSize) && !RecvFileAr )
		{
			DownloadError( *NetSizeError );
			Finished = 1;
		}

	}

	if ( Finished )
	{
		if ( Error[0] )
			debugf( TEXT("Download finished with error: %s"), Error);
		if ( !Error[0] ) //Finished without errors
		{
			FString IniName = GSys->CachePath + PATH_SEPARATOR + TEXT("cache.ini");
			FConfigCacheIni CacheIni;
			CacheIni.SetString( TEXT("Cache"), Info->Guid.String(), *(*Info->URL) ? *Info->URL : Info->Parent->GetName(), *IniName );

			FString Msg = FString::Printf( TEXT("Received '%s'"), Info->Parent->GetName() );
			Connection->Driver->Notify->NotifyProgress( TEXT("Success"), *Msg, 4.f );
		}
		GFileManager->Delete( TempFilename);
		Connection->Driver->Notify->NotifyReceivedFile( Connection, PackageIndex, Error, 0);
	}
	unguard;
}


/*=============================================================================
XC_Core extended download protocols
=============================================================================*/


void UXC_Download::StaticConstructor()
{
	EnableLZMA = 1;
	UseCompression = 1;
}



void UXC_Download::ReceiveData( BYTE* Data, INT Count )
{
	guard( UXC_Download:ReceiveData);
	// Receiving spooled file data.
	if( Transfered==0 && !RecvFileAr )
	{
		debugf( NAME_DevNet, TEXT("Receiving package '%s'"), Info->Parent->GetName() );
		FString PackageName = Info->URL;
		int32 DirSeparator = Max( PackageName.InStr(TEXT("/"),1), PackageName.InStr( TEXT("\\"),1) );
		if ( DirSeparator >= 0 )
			PackageName = PackageName.Mid( DirSeparator+1);

		if ( Count >= 13 )
		{
			QWORD* LZMASize = (QWORD*)&Data[5];
			if ( Info->FileSize == *LZMASize )
			{
				IsCompressed = 1;
				IsLZMA = 1;
				PackageName += TEXT(".lzma");
				debugf( NAME_DevNet, TEXT("USES LZMA"));
			}
			INT* UzSignature = (INT*)&Data[0];
			if ( *UzSignature == 1234 || *UzSignature == 5678 )
			{
				IsCompressed = 1;
				PackageName += TEXT(".uz");
				debugf( NAME_DevNet, TEXT("USES UZ: Signature %i"), *UzSignature);
			}
		}

		// Open temporary file after figuring out the type of compression.
		GFileManager->MakeDirectory( *GSys->CachePath, 0 );
		GFileManager->MakeDirectory( TEXT("../DownloadTemp"), 0);
		FString Filename = FString::Printf( TEXT("../DownloadTemp/%s"), *PackageName );
		appStrncpy( TempFilename, *Filename, 255);
		RecvFileAr = GFileManager->CreateFileWriter( TempFilename );

	}

	// Receive.
	if( !RecvFileAr )
	{
		// Opening file failed.
		DownloadError( LocalizeError(TEXT("NetOpen"),TEXT("Engine")) );
	}
	else
	{
		if( Count > 0 )
			((FArchive*)RecvFileAr)->Serialize( Data, Count);
		if( RecvFileAr->IsError() )
		{
			// Write failed.
			DownloadError( *FString::Printf( LocalizeError(TEXT("NetWrite"),TEXT("Engine")), TempFilename ) );
		}
		else
		{
			// Successful.
			Transfered += Count;
		}
	}	
	unguard;
}

void UXC_Download::DownloadDone()
{
	guard( UXC_Download::DownloadDone);
	if( RecvFileAr )
	{
		guard( DeleteFile );
		delete RecvFileAr;
		RecvFileAr = nullptr;
		unguard;
	}
	if( SkippedFile )
	{
		guard( Skip );
		debugf( TEXT("Skipped download of '%s'"), Info->Parent->GetName() );
		GFileManager->Delete( TempFilename );
		TCHAR Msg[256];
		appSprintf( Msg, TEXT("Skipped '%s'"), Info->Parent->GetName() );
		Connection->Driver->Notify->NotifyProgress( TEXT("Success"), Msg, 4.f );
		Connection->Driver->Notify->NotifyReceivedFile( Connection, PackageIndex, TEXT(""), 1 );
		unguard;
	}
	else
	{
		if ( !Connection->Channels[0] || Connection->Channels[0]->Closing )
			return;
		if( !Error[0] && Transfered==0 )
			DownloadError( *FString::Printf( LocalizeError(TEXT("NetRefused"),TEXT("Engine")), Info->Parent->GetName() ) );
		else if( !Error[0] )
		{
			Transfered = RealFileSize ? RealFileSize : Info->FileSize; //Fix to start async decompressor
			if ( IsA(UXC_ChannelDownload::StaticClass()) ) //Detach download from channel
				((UXC_ChannelDownload*)this)->Ch->Download = NULL; 
		}
	}
	unguard;
}
IMPLEMENT_CLASS(UXC_Download)



void UXC_ChannelDownload::StaticConstructor()
{
	DownloadParams = TEXT("Enabled");
	UChannel::ChannelClasses[7] = UXC_FileChannel::StaticClass();
	new( GetClass(),TEXT("Ch"), RF_Public) UObjectProperty( CPP_PROPERTY(Ch), TEXT("Download"), CPF_Edit|CPF_EditConst|CPF_Const, UFileChannel::StaticClass() );
}

void UXC_ChannelDownload::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
//	*((FArchive*)&Ar) << Ch;
}
UBOOL UXC_ChannelDownload::TrySkipFile()
{
	if( Ch && Super::TrySkipFile() )
	{
		FOutBunch Bunch( Ch, 1 );
		FString Cmd = TEXT("SKIP");
		Bunch << Cmd;
		Bunch.bReliable = 1;
		Ch->SendBunch( &Bunch, 0 );
		return 1;
	}
	return 0;
}
void UXC_ChannelDownload::ReceiveFile( UNetConnection* InConnection, INT InPackageIndex, const TCHAR *Params, UBOOL InCompression )
{
	UXC_Download::ReceiveFile( InConnection, InPackageIndex, Params, InCompression );

	// Create channel.
	Ch = (UFileChannel *)Connection->CreateChannel( (EChannelType)7, 1 );

	if( !Ch )
	{
		DownloadError( LocalizeError(TEXT("ChAllocate"),TEXT("Engine")) );
		DownloadDone();
		return;
	}

	// Set channel properties.
	Ch->Download = (UChannelDownload*)this; //THIS IS A HACK!!!!
	Ch->PackageIndex = PackageIndex;

	// Send file request.
	FOutBunch Bunch( Ch, 0 );
	Bunch.ChType = 7;
	Bunch << Info->Guid;
	Bunch.bReliable = 1;
	check(!Bunch.IsError());
	Ch->SendBunch( &Bunch, 0 );
}

void UXC_ChannelDownload::Destroy()
{
	guard(UXC_ChannelDownload::Destroy);
	if( Ch && Ch->Download == (UChannelDownload*)this )
		Ch->Download = nullptr;
	Ch = nullptr;
	Super::Destroy();
	unguard;
}
IMPLEMENT_CLASS(UXC_ChannelDownload)


UXC_FileChannel::UXC_FileChannel()
{
	Download = NULL;
}

void UXC_FileChannel::Init( UNetConnection* InConnection, INT InChannelIndex, INT InOpenedLocally )
{
	Super::Init( InConnection, InChannelIndex, InOpenedLocally );
	if ( InConnection && InConnection->Driver && !InConnection->Driver->ServerConnection ) //This is not a client
		ChType = CHTYPE_File; //Avoid ULevel->NotifyAcceptingChannel from deleting this channel
}

void UXC_FileChannel::ReceivedBunch( FInBunch& Bunch )
{
//	UNREAL ADV: BUG REPORTED BY LUIGI AURIEMMA
//	check(!Closing);
	guard( UXC_FileChannel:ReceivedBunch);
	if ( Closing )
		return;
	if( OpenedLocally )
	{
		// Receiving a file sent from the other side.  If Bunch.GetNumBytes()==0, it means the server refused to send the file.
		Download->ReceiveData( Bunch.GetData(), Bunch.GetNumBytes() );
	}
	else
	{
		if( !Connection->Driver->AllowDownloads )
		{
			// Refuse the download by sending a 0 bunch.
			debugf( NAME_DevNet, LocalizeError(TEXT("NetInvalid"),TEXT("Engine")) );
			FOutBunch Bunch( this, 1 );
			SendBunch( &Bunch, 0 );
			return;
		}
		if( SendFileAr )
		{
			FString Cmd;
			Bunch << Cmd;
			if( !Bunch.IsError() && Cmd==TEXT("SKIP") )
			{
				// User cancelled optional file download.
				// Remove it from the package map
				debugf( TEXT("User skipped download of '%s'"), SrcFilename );
				Connection->PackageMap->List.Remove( PackageIndex );
				return;
			}
		}
		else
		{
			// Request to send a file.
			FGuid Guid;
			Bunch << Guid;
			if( !Bunch.IsError() )
			{
				for( INT i=0; i<Connection->PackageMap->List.Num(); i++ )
				{
					FPackageInfo& Info = Connection->PackageMap->List(i);
					if( Info.Guid==Guid && Info.URL!=TEXT("") )
					{
						if( Connection->Driver->MaxDownloadSize>0 && GFileManager->FileSize(*Info.URL) > Connection->Driver->MaxDownloadSize )
							break;							
						appStrncpy( SrcFilename, *Info.URL, ARRAY_COUNT(SrcFilename) );
						if( Connection->Driver->Notify->NotifySendingFile( Connection, Guid ) )
						{
							check(Info.Linker);
							SendFileAr = NULL;
							FString FileToSend( SrcFilename);
							FileToSend += TEXT(".lzma");
							SendFileAr = GFileManager->CreateFileReader( *FileToSend);
							if ( !SendFileAr )
							{
								FileToSend = SrcFilename;
								FileToSend += TEXT(".uz");
								SendFileAr = GFileManager->CreateFileReader( *FileToSend);
							}
							if ( !SendFileAr )
								SendFileAr = GFileManager->CreateFileReader( SrcFilename );
							if( SendFileAr )
							{
								// Accepted! Now initiate file sending.
								debugf( NAME_DevNet, LocalizeProgress(TEXT("NetSend"),TEXT("Engine")), *FileToSend );
								PackageIndex = i;
								return;
							}
						}
					}
				}
			}
		}

		// Illegal request; refuse it by closing the channel.
		debugf( NAME_DevNet, LocalizeError(TEXT("NetInvalid"),TEXT("Engine")) );
		
		FOutBunch Bunch( this, 1 );
		SendBunch( &Bunch, 0 );
	}
	unguard;
}

static UBOOL LanPlay = -1; //Prevent static init inside function, newer compilers try to do it thread-safe when not necessary
void UXC_FileChannel::Tick()
{
	UChannel::Tick();
	Connection->TimeSensitive = 1;
	INT Size;

	//TIM: IsNetReady(1) causes the client's bandwidth to be saturated. Good for clients, very bad
	// for bandwidth-limited servers. IsNetReady(0) caps the clients bandwidth.
	if ( LanPlay == -1 )
		LanPlay = ParseParam(appCmdLine(),TEXT("lanplay"));
	while( !OpenedLocally && SendFileAr && IsNetReady(LanPlay) && (Size=MaxSendBytes())!=0 )
	{
		// Sending.
		INT Remaining = ((FArchive*)SendFileAr)->TotalSize() - SentData;
		Size = Min( Size, Remaining );
		//Never send less than 13 bytes, we ensure LZMA header is sent in one chunk
		if ( (SentData == 0) && (Size <= 13) )
			return;
		FOutBunch Bunch( this, Size>=Remaining );

		//Slow method, otherwise we depend on Alloca
		TArray<BYTE> Buffer(Size);
//		BYTE* Buffer = (BYTE*)appAlloca( Size );
		((FArchive*)SendFileAr)->Serialize( Buffer.GetData(), Size ); //Linux v440 net crashfix
		if( SendFileAr->IsError() )
		{
			//HANDLE THIS!!
		}
		SentData += Size;
		Bunch.Serialize( Buffer.GetData(), Size );
		Bunch.bReliable = 1;
		check(!Bunch.IsError());
		SendBunch( &Bunch, 0 );
		Connection->FlushNet();
		if ( Bunch.bClose ) //Finished
		{
			delete SendFileAr;
			SendFileAr = nullptr;
		}
	}
}

void UXC_FileChannel::Destroy()
{
	check(Connection);
	if( RouteDestroy() )
		return;

	check( Connection->Channels[ChIndex] == this);

	// Close the file.
	if( SendFileAr )
	{
		delete SendFileAr;
		SendFileAr = nullptr;
	}

	// Notify that the receive succeeded or failed.
	if( OpenedLocally && Download )
	{
		Download->DownloadDone();
		if ( Download ) //Detachable download may not want to be deleted yet
			delete Download;
	}
	UChannel::Destroy();
}
IMPLEMENT_CLASS(UXC_FileChannel)



/*----------------------------------------------------------------------------
	UZ decompressor.
----------------------------------------------------------------------------*/

#undef guard
#undef unguard
#define guard(n) {
#define unguard }

#include "FCodec.h"

void UZDecompress( const TCHAR* SourceFilename, const TCHAR* DestFilename, TCHAR* Error)
{
	FArchive* SrcFileAr = nullptr;
	FArchive* DestFileAr = nullptr;

	try
	{
		SrcFileAr = GFileManager->CreateFileReader( SourceFilename );
		if ( SrcFileAr )
		{
			DestFileAr = GFileManager->CreateFileWriter( DestFilename );
			if ( DestFileAr )
			{
				int32 Signature;
				*SrcFileAr << Signature;
				if( (Signature != 5678) && (Signature != 1234) )
					appStrcpy( Error, *UXC_Download::NetSizeError);
				else
				{
					FString OrigFilename;
					*SrcFileAr << OrigFilename;
					FCodecFull Codec;
					Codec.AddCodec(new FCodecRLE);
					Codec.AddCodec(new FCodecBWT);
					Codec.AddCodec(new FCodecMTF);
					if ( Signature == 5678 ) //UZ2 Support
						Codec.AddCodec(new FCodecRLE);
					Codec.AddCodec(new FCodecHuffman);
					Codec.Decode( *SrcFileAr, *DestFileAr );
				}
				delete DestFileAr;
				DestFileAr = nullptr;
			}
			delete SrcFileAr;
			SrcFileAr = nullptr;
			GFileManager->Delete( SourceFilename );
		}
		else
			appStrcpy( Error, *UXC_Download::NetOpenError);
	}
	catch ( const TCHAR* C )
	{
		if ( *C )
			appStrncpy( Error, C, 255);
	}
	catch (...)
	{
		if ( !Error[0] )
			appStrcpy( Error, TEXT("Unhandled exception in UZ decompressor"));
	}

	if ( SrcFileAr ) delete SrcFileAr;
	if ( DestFileAr) delete DestFileAr;

	if ( Error[0] )
	{
		if ( GFileManager->FileSize( DestFilename) )
			GFileManager->Delete( DestFilename );
	}
}