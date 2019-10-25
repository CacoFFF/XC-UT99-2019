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
#include "FCodec.h"

#include "Cacus/CacusThread.h"


/*=============================================================================
XC_Core extended download protocols
=============================================================================*/

struct FThreadDecompressor : public CThread
{
	UXC_Download* Download;
	TCHAR* TempFilename;
	TCHAR* Error;
	
	FThreadDecompressor( UXC_Download* DL)
	: CThread()
	, Download(DL)
	, TempFilename(DL->TempFilename)
	, Error(DL->Error)	{}
};


void UXC_Download::StaticConstructor()
{
	EnableLZMA = 1;
	UseCompression = 1;
}

void UXC_Download::Tick()
{
	guard(UXC_Download::Tick);
	if ( !IsDecompressing && Decompressor ) //Compression finished?
	{
		delete Decompressor;
		Decompressor = NULL;

		if( Error[0] )
		{
			GFileManager->Delete( TempFilename );
//HIGOR: Control channel is being closed, do not notify level of file failure (it will restart download using a different method!)
			Connection->Driver->Notify->NotifyReceivedFile( Connection, PackageIndex, Error, 0 );
		}
		else
		{
			// Success.
			TCHAR Msg[256];
			FString IniName = GSys->CachePath + PATH_SEPARATOR + TEXT("cache.ini");
			FConfigCacheIni CacheIni;
			CacheIni.SetString( TEXT("Cache"), Info->Guid.String(), *(*Info->URL) ? *Info->URL : Info->Parent->GetName(), *IniName );

			appSprintf( Msg, TEXT("Received '%s'"), Info->Parent->GetName() );
			Connection->Driver->Notify->NotifyProgress( TEXT("Success"), Msg, 4.f );
			Connection->Driver->Notify->NotifyReceivedFile( Connection, PackageIndex, Error, 0 );
		}
	
	}
	unguard;
}

void UXC_Download::ReceiveData( BYTE* Data, INT Count )
{
	guard( UXC_Download:ReceiveData);
	// Receiving spooled file data.
	if( Transfered==0 && !RecvFileAr )
	{
		// Open temporary file initially.
		debugf( NAME_DevNet, TEXT("Receiving package '%s'"), Info->Parent->GetName() );
		appCreateTempFilename( *GSys->CachePath, TempFilename );
		GFileManager->MakeDirectory( *GSys->CachePath, 0 );
		RecvFileAr = GFileManager->CreateFileWriter( TempFilename );
		if ( Count >= 13 )
		{
			QWORD* LZMASize = (QWORD*)&Data[5];
			if ( Info->FileSize == *LZMASize )
			{
				IsCompressed = 1;
				IsLZMA = 1;
				debugf( NAME_DevNet, TEXT("USES LZMA"));
			}
			INT* UzSignature = (INT*)&Data[0];
			if ( *UzSignature == 1234 || *UzSignature == 5678 )
			{
				IsCompressed = 1;
				debugf( NAME_DevNet, TEXT("USES UZ: Signature %i"), *UzSignature);
			}
		}
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
			INT RealSize = RealFileSize ? RealFileSize : Info->FileSize;
			FString Msg1 = FString::Printf( (Info->PackageFlags&PKG_ClientOptional)?LocalizeProgress(TEXT("ReceiveOptionalFile"),TEXT("Engine")):LocalizeProgress(TEXT("ReceiveFile"),TEXT("Engine")), Info->Parent->GetName() );
			FString Msg2 = FString::Printf( LocalizeProgress(TEXT("ReceiveSize"),TEXT("Engine")), RealSize/1024, 100.f*Transfered/RealSize );
			Connection->Driver->Notify->NotifyProgress( *Msg1, *Msg2, 4.f );
		}
	}	
	unguard;
}

void UXC_Download::DownloadDone()
{
	guard( UXC_Download::DownloadDone);
	
	if ( Decompressor ) //Prevent XC_IpDrv reentrancy
		return;	
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
		if( !Error[0] && IsCompressed )
		{
			
			if ( IsA(UXC_ChannelDownload::StaticClass()) )
				((UXC_ChannelDownload*)this)->Ch->Download = NULL; //Detach download from channel
			StartDecompressor();
			return;
/*			TCHAR CFilename[256];
			appStrcpy( CFilename, TempFilename );
			appCreateTempFilename( *GSys->CachePath, TempFilename );
			FArchive* CFileAr = GFileManager->CreateFileReader( CFilename );
			FArchive* CFilePx = (FArchive*)CFileAr;

			FArchive* UFileAr = NULL; //Don't open yet
			if ( CFileAr && !IsLZMA )
				UFileAr = GFileManager->CreateFileWriter( TempFilename );

			if( !CFileAr || (!IsLZMA && !UFileAr) )
				DownloadError( LocalizeError(TEXT("NetOpen"),TEXT("Engine")) );
			else if ( IsLZMA )
			{
				if ( LzmaDecompress( CFileAr, *Dest, Error) )
					debugf( NAME_DevNet, TEXT("LZMA Decompress: %s"), TempFilename);
			}
			else
			{
				INT Signature;
				FString OrigFilename;
				CFilePx->Serialize( &Signature, sizeof(INT) );
				if( (Signature != 5678) && (Signature != 1234) )
					DownloadError( LocalizeError(TEXT("NetSize"),TEXT("Engine")) );
				else
				{
					*CFilePx << OrigFilename;
					FCodecFull Codec;
					Codec.AddCodec(new FCodecRLE);
					Codec.AddCodec(new FCodecBWT);
					Codec.AddCodec(new FCodecMTF);
					if ( Signature == 5678 ) //UZ2 Support
						Codec.AddCodec(new FCodecRLE);
					Codec.AddCodec(new FCodecHuffman);
					Codec.Decode( *CFileAr, *UFileAr );
				}
			}
			if( CFileAr )
			{
				ARCHIVE_DELETE( CFileAr);
				GFileManager->Delete( CFilename );
			}
			if( UFileAr )
				ARCHIVE_DELETE( UFileAr);*/
		}
		if( !Error[0] && !IsCompressed && GFileManager->FileSize(TempFilename)!=Info->FileSize ) //Compression screws up filesize, ignore
			DownloadError( LocalizeError(TEXT("NetSize"),TEXT("Engine")) );
		TCHAR Dest[256];
		DestFilename( Dest);
		if( !Error[0] && !IsLZMA && !GFileManager->Move( Dest, TempFilename, 1 ) ) //LZMA already performs this step
			DownloadError( LocalizeError(TEXT("NetMove"),TEXT("Engine")) );
		if( Error[0] )
		{
			GFileManager->Delete( TempFilename );
//HIGOR: Control channel is being closed, do not notify level of file failure (it will restart download using a different method!)
			Connection->Driver->Notify->NotifyReceivedFile( Connection, PackageIndex, Error, 0 );
		}
		else
		{
			// Success.
			TCHAR Msg[256];
			FString IniName = GSys->CachePath + PATH_SEPARATOR + TEXT("cache.ini");
			FConfigCacheIni CacheIni;
			CacheIni.SetString( TEXT("Cache"), Info->Guid.String(), *(*Info->URL) ? *Info->URL : Info->Parent->GetName(), *IniName );

			appSprintf( Msg, TEXT("Received '%s'"), Info->Parent->GetName() );
			Connection->Driver->Notify->NotifyProgress( TEXT("Success"), Msg, 4.f );
			Connection->Driver->Notify->NotifyReceivedFile( Connection, PackageIndex, Error, 0 );
		}
	}
	unguard;
}



static unsigned long LZMADecompress( void* arg)
{
	//Setup environment
	FThreadDecompressor* TInfo = (FThreadDecompressor*)arg;
	
	//Setup decompression
	TCHAR Dest[256];
	TCHAR Error[256];
	TInfo->Download->DestFilename( Dest);
	LzmaDecompress( TInfo->TempFilename, Dest, Error);

	if ( Error[0] )
	{
		appStrcpy( TInfo->Error, Error);
		if ( GFileManager->FileSize( Dest) )
			GFileManager->Delete( Dest);
	}
	TInfo->Download->IsDecompressing = 0;
	return THREAD_END_OK;
}

static unsigned long UZDecompress( void* arg)
{
	//Setup environment
	FThreadDecompressor* TInfo = (FThreadDecompressor*)arg;
	
	//Setup decompression
	TCHAR DecompressProgress[256];
	
	appCreateTempFilename( *GSys->CachePath, DecompressProgress );
	FArchive* CFileAr = GFileManager->CreateFileReader( TInfo->Download->TempFilename );
	if ( CFileAr )
	{
		FArchive* UFileAr = GFileManager->CreateFileWriter( DecompressProgress );
		if ( UFileAr )
		{
			INT Signature;
			FString OrigFilename;
			CFileAr->Serialize( &Signature, sizeof(INT) );
			if( (Signature != 5678) && (Signature != 1234) )
				TInfo->Download->DownloadError( LocalizeError(TEXT("NetSize"),TEXT("Engine")) );
			else
			{
				*CFileAr << OrigFilename;
				FCodecFull Codec;
				Codec.AddCodec(new FCodecRLE);
				Codec.AddCodec(new FCodecBWT);
				Codec.AddCodec(new FCodecMTF);
				if ( Signature == 5678 ) //UZ2 Support
					Codec.AddCodec(new FCodecRLE);
				Codec.AddCodec(new FCodecHuffman);
				Codec.Decode( *CFileAr, *UFileAr );
			}
			delete UFileAr;
			if ( !TInfo->Download->Error[0] )
			{
				TCHAR Dest[256];
				TInfo->Download->DestFilename( Dest);
				if( !GFileManager->Move( Dest, DecompressProgress, 1 ) )
					TInfo->Download->DownloadError( LocalizeError(TEXT("NetMove"),TEXT("Engine")) );
				if ( GFileManager->FileSize( DecompressProgress) )
					GFileManager->Delete( DecompressProgress );
			}
		}
		delete CFileAr;
		GFileManager->Delete( TInfo->Download->TempFilename );
	}
	else
		TInfo->Download->DownloadError( LocalizeError(TEXT("NetOpen"),TEXT("Engine")) );
	
	//Check that environment is still active (download could have been cancelled)
	TInfo->Download->IsDecompressing = 0;
	return THREAD_END_OK;
}

void UXC_Download::StartDecompressor()
{
	if ( IsDecompressing ) //XC_IpDrv makes reentrant calls
		return;
	IsDecompressing = 1;
	Decompressor = new( TEXT("Decompressor Thread")) FThreadDecompressor(this);
	if ( IsLZMA )
		Decompressor->Run( &LZMADecompress, Decompressor);
	else
		Decompressor->Run( &UZDecompress, Decompressor);

	TCHAR Prg[128];
	appStrcpy( Prg, TEXT("%s: %iK > %iK"));
	FString Msg1 = FString::Printf( LocalizeProgress(TEXT("DecompressFile"),TEXT("XC_Core")), Info->Parent->GetName() );
	FString Msg2 = FString::Printf( Prg, (IsLZMA ? TEXT("LZMA") : TEXT("UZ")), Transfered/1024, Info->FileSize/1024 );
	Connection->Driver->Notify->NotifyProgress( *Msg1, *Msg2, 4.f );
}

void UXC_Download::DestFilename( TCHAR* T)
{
	T[0] = 0;
	appStrcat( T, *(GSys->CachePath));
	appStrcat( T, PATH_SEPARATOR);
	appStrcat( T, Info->Guid.String());
	appStrcat( T, *(GSys->CacheExt));
}
IMPLEMENT_CLASS(UXC_Download)



void UXC_ChannelDownload::StaticConstructor()
{
	DownloadParams = TEXT("Enabled");
	UChannel::ChannelClasses[7] = UXC_FileChannel::StaticClass();
	new( GetClass(),TEXT("Ch"), RF_Public) UObjectProperty( CPP_PROPERTY(Ch), TEXT("Download"), CPF_Edit|CPF_EditConst|CPF_Const, UFileChannel::StaticClass() );
}

void UXC_Download::Destroy()
{
	if ( Decompressor )
		delete Decompressor; //Blocking operation
	Super::Destroy();
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
	if( Ch && Ch->Download == (UChannelDownload*)this )
		Ch->Download = NULL;
	Ch = NULL;
	Super::Destroy();
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

