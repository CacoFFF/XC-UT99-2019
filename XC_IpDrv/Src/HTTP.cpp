/*=============================================================================
	HTTP.cpp
	Author: Fernando Velazquez

	Implementation of asynchronous HTTP File Downloader
=============================================================================*/

#if 1

#include "HTTPDownload.h"
#include "Cacus/Atomics.h"


/*----------------------------------------------------------------------------
	HTTP Request utils.
----------------------------------------------------------------------------*/

FString HTTP_Request::String()
{
	FString Result = FString::Printf( TEXT("%s %s HTTP/1.1\r\nHost: %s\r\n")
		,*Method, *Path, *Hostname);
	for ( TMultiMap<FString,FString>::TIterator It(Headers) ; (UBOOL)It ; ++It )
		Result += FString::Printf( TEXT("%s: %s\r\n"), *It.Key(), *It.Value() );
	Result += TEXT("\r\n");
	return Result;
}

/*----------------------------------------------------------------------------
	HTTP Downloader.
----------------------------------------------------------------------------*/

void UXC_HTTPDownload::StaticConstructor()
{
	UClass* Class = GetClass();

	// Config.
	new(Class,TEXT("ProxyServerHost"),		RF_Public)UStrProperty(CPP_PROPERTY(ProxyServerHost		), TEXT("Settings"), CPF_Config );
	new(Class,TEXT("ProxyServerPort"),		RF_Public)UIntProperty(CPP_PROPERTY(ProxyServerPort		), TEXT("Settings"), CPF_Config );
	new(Class,TEXT("DownloadTimeout"),		RF_Public)UFloatProperty(CPP_PROPERTY(DownloadTimeout	), TEXT("Settings"), CPF_Config );
	new(Class,TEXT("RedirectToURL"),		RF_Public)UStrProperty(CPP_PROPERTY(DownloadParams		), TEXT("Settings"), CPF_Config );
	new(Class,TEXT("UseCompression"),		RF_Public)UBoolProperty(CPP_PROPERTY(UseCompression		), TEXT("Settings"), CPF_Config );

	//Defaults
	UXC_HTTPDownload* Defaults = (UXC_HTTPDownload*)Class->GetDefaultObject();
	Defaults->IsLZMA = 1;
	Defaults->IsCompressed = 1;
	Defaults->DownloadTimeout = 4.0f;
}

UXC_HTTPDownload::UXC_HTTPDownload()
{
	Socket.SetInvalid();
	IsCompressed = 0;
	IsLZMA = 0;
}

void UXC_HTTPDownload::Destroy()
{
	Socket.Close();
	Super::Destroy();
}

UBOOL UXC_HTTPDownload::TrySkipFile()
{
	if( Super::TrySkipFile() )
	{
		Connection->Logf( TEXT("SKIP GUID=%s"), *Info->Guid.String() );
		Socket.Close();
		Finished = 1;
		Error[0] = '\0';
		return 1;
	}
	return 0;
}

void UXC_HTTPDownload::ReceiveFile( UNetConnection* InConnection, INT InPackageIndex, const TCHAR* Params, UBOOL InCompression)
{
	guard(UXC_HTTPDownload::ReceiveFile);
	SaveConfig();
	UDownload::ReceiveFile( InConnection, InPackageIndex);

	if( !Params[0] )
	{
		IsInvalid = 1;
		Finished = 1;
		return;
	}

	FPackageInfo& Info = Connection->PackageMap->List( PackageIndex );
	DownloadURL = FDownloadURL( Params, *Info.URL);
	if ( !DownloadURL.bIsValid )
	{
		IsInvalid = 1;
		Finished = 1;
		return;
	}

	if ( InCompression )
		DownloadURL.Compression = LZMA_COMPRESSION;
	DownloadURL.ProxyHostname = ProxyServerHost;
	DownloadURL.ProxyPort = ProxyServerPort;
	CurrentURL = DownloadURL;

	Request = HTTP_Request();
	Request.Hostname = CurrentURL.StringHost();
	Request.Method = TEXT("GET");
	Request.Path = CurrentURL.StringGet();
	Request.Headers.Set( TEXT("User-Agent")  , TEXT("Unreal"));
	Request.Headers.Set( TEXT("Accept")      , TEXT("*/*"));
	Request.Headers.Set( TEXT("Connection")  , TEXT("close")); //Proxy with auth require [Proxy-Connection: close]
	Request.RedirectsLeft = 5;

	FString Msg1 = FString::Printf( (Info.PackageFlags&PKG_ClientOptional)?LocalizeProgress(TEXT("ReceiveOptionalFile"),TEXT("Engine")):LocalizeProgress(TEXT("ReceiveFile"),TEXT("Engine")), Info.Parent->GetName() );
	Connection->Driver->Notify->NotifyProgress( *Msg1, TEXT(""), 4.f );

	unguard;
}

void UXC_HTTPDownload::Tick()
{
	SavedLogs.Flush();
	Super::Tick();

	//*****************************
	// Async operations have 3 stages:
	// 1 -- Setup stage:
	//  Initialization of the async process, this blocks the main thread.
	// 2 -- Async stage:
	//  The async processor runs downloader independent tasks (mostly API stuff) while the
	//  main thead continues as normal, polling the downloader for updates on every tick.
	// 3 -- End stage:
	//  The async processor blocks destruction of downloaders and begins to update them.
	if ( !Finished && !AsyncAction )
	{
		//******************************
		//Setup hostname and update port
		FString NewHostname = CurrentURL.StringHost( 1 );
		if ( Request.Hostname != NewHostname )
		{
			RemoteEndpoint.Address = FIPv6Address::Any;
			Request.Hostname = NewHostname;
		}
		RemoteEndpoint.Port = CurrentURL.Port ? CurrentURL.Port : 80;

		//*****************************
		//Hostname needs to be resolved
		if ( RemoteEndpoint.Address == FIPv6Address::Any )
		{
			new FDownloadAsyncProcessor( [](FDownloadAsyncProcessor* Proc)
			{
				//STAGE 1, setup local environment.
				UXC_HTTPDownload* Download = (UXC_HTTPDownload*)Proc->Download;
				FString Hostname = Download->Request.Hostname;

				//STAGE 2, let main go (no longer safe to use Download from now on)
				Proc->Detach();
				FIPv6Address Address = ResolveHostname( *Hostname, NULL);

				//STAGE 3, validate downloader and lock
				CSpinLock SL(&UXC_Download::GlobalLock);
				if ( !Proc->DownloadActive() )
					return;
				if ( Address == FIPv6Address::Any )
				{
					Download->SavedLogs.Logf( NAME_DevNet, TEXT("Failed to resolve hostname %s"), *Hostname);
					Download->DownloadError( *FString::Printf( *UXC_Download::InvalidUrlError, *Download->Request.Hostname) );
				}
				else
					Download->SavedLogs.Logf( NAME_DevNet, TEXT("Resolved: %s >> %s"), *Hostname, *Address.String() );
				Download->RemoteEndpoint.Address = Address;
			}, this);
		}

		//************
		//Send request
		else if ( Request.Path.Len() )
		{
			Response = HTTP_Response();
			Request.Path = CurrentURL.StringGet();
			new FDownloadAsyncProcessor( [](FDownloadAsyncProcessor* Proc)
			{
				//STAGE 1, setup local environment.
				UXC_HTTPDownload* Download = (UXC_HTTPDownload*)Proc->Download;
				if ( !Download->AsyncLocalBind() )
					return;
				FSocket Socket               = Download->Socket;
				double Timeout               = Download->DownloadTimeout;
				FIPv6Endpoint RemoteEndpoint = Download->RemoteEndpoint;
				FString RequestHeader        = Download->Request.String();
				const TCHAR* ConnectError    = nullptr;

				//STAGE 2, let main go (no longer safe to use Download from now on)
				Proc->Detach();
				Socket.SetNonBlocking();
				appSleep( 0.2f); //Don't try to connect so quickly (a previous download's connection may not be closed)
				ESocketState State = SOCKET_MAX;
				if ( !Socket.Connect(RemoteEndpoint) && (Socket.LastError != FSocket::ENonBlocking) )
					ConnectError = TEXT("XC_HTTPDownload: connect() failed");
				else
				{
					State = Socket.CheckState( SOCKET_Writable, Timeout);
					if ( State == SOCKET_HasError )
						ConnectError = TEXT("XC_HTTPDownload: select() failed");
					else if ( State == SOCKET_Timeout )
						ConnectError = TEXT("XC_HTTPDownload: connection timed out");
				}

				if ( ConnectError )
				{
					//STAGE 3: Tell downloader (if still exists) of failure to connect to server
					CSleepLock SL(&UXC_Download::GlobalLock); 
					if ( Proc->DownloadActive() )
					{
						Download->SavedLogs.Log( NAME_DevNet, ConnectError);
						Download->DownloadError( *UXC_Download::ConnectionFailedError );
						Download->IsInvalid = (State == SOCKET_Timeout); //If the server timed out, do not try to connect again for another download
					}
					return;
				}
				else
				{
					//STAGE 3: Send request to server and unlock again (back to stage 2)
					CSleepLock SL(&UXC_Download::GlobalLock); 
					if ( !Proc->DownloadActive() )
						return;

					int32 Sent = 0;
					ANSICHAR* RequestHeaderAnsi = TCHAR_TO_ANSI_HEAP( *RequestHeader);
					bool bSent = Socket.Send( (const uint8*)RequestHeaderAnsi, RequestHeader.Len(), Sent) && (Sent >= RequestHeader.Len());
					appFree( RequestHeaderAnsi);
					if ( !bSent ) //Produce proper log!
					{
						Download->DownloadError( *UXC_Download::ConnectionFailedError );
						return;
					}
					Download->SavedLogs.Log( NAME_DevNetTraffic, TEXT("Connected..."));
				}

				double LastRecvTime = appSecondsNew();
				while ( true )
				{
					//STAGE 3: Receive data from server
					CSleepLock SL(&UXC_Download::GlobalLock); 
					if ( !Proc->DownloadActive() )
						return;
					int32 OldTransfered = Download->Transfered;
					if ( Download->AsyncReceive() )
					{
						if ( Download->Error[0] )
							Download->SavedLogs.Log( Download->Error);
						break;
					}
					if ( OldTransfered != Download->Transfered )
						LastRecvTime = appSecondsNew();
					else if ( appSecondsNew() - LastRecvTime > Timeout )
					{
						Download->SavedLogs.Log( NAME_DevNet, TEXT("XC_HTTPDownload: connection timed out") );
						Download->DownloadError( *UXC_Download::ConnectionFailedError );
						break;
					}
					appSleep( 0); //Poll aggresively to prevent bandwidth loss
				}
				CSleepLock SL(&UXC_Download::GlobalLock); 
				if ( Proc->DownloadActive() && Download->RecvFileAr )
				{
					delete Download->RecvFileAr;
					Download->RecvFileAr = nullptr;
				}
			}, this);
		}
	}
}

/*----------------------------------------------------------------------------
	Downloader utils.
----------------------------------------------------------------------------*/

void UXC_HTTPDownload::UpdateCurrentURL( const TCHAR* RelativeURI)
{
	//Merge modifiers into path
	if ( CurrentURL.RequestedPackage.Len() )
	{
		CurrentURL.Path += CurrentURL.RequestedPackage;
		CurrentURL.Path += CurrentURL.GetCompressedExt( CurrentURL.Compression);
		CurrentURL.RequestedPackage.Empty();
		CurrentURL.Compression = 0;
	}
	*(FURI*)&CurrentURL = FURI( CurrentURL, RelativeURI);
}

/*----------------------------------------------------------------------------
	Asynchronous functions.
	These are called by worker threads.
----------------------------------------------------------------------------*/

void UXC_HTTPDownload::ReceiveData( BYTE* Data, INT Count )
{
	if ( Count <= 0 )
		return;

	// Receiving spooled file data.
	if( !Transfered && !RecvFileAr )
	{
		// Open temporary file initially.
		SavedLogs.Logf( NAME_DevNet, TEXT("Receiving package '%s'"), Info->Parent->GetName() );
		if ( RealFileSize )
			SavedLogs.Logf( NAME_DevNet, TEXT("Compressed filesize: %i"), RealFileSize);

		GFileManager->MakeDirectory( *GSys->CachePath, 0 );
		GFileManager->MakeDirectory( TEXT("../DownloadTemp"), 0);
		FString Filename = FString::Printf( TEXT("../DownloadTemp/%s%s"), *DownloadURL.RequestedPackage, DownloadURL.GetCompressedExt(DownloadURL.Compression) );
		appStrncpy( TempFilename, *Filename, 255);
		RecvFileAr = GFileManager->CreateFileWriter( TempFilename );
		if ( Count >= 13 )
		{
			QWORD* LZMASize = (QWORD*)&Data[5];
			if ( Info->FileSize == *LZMASize )
			{
				IsCompressed = 1;
				IsLZMA = 1;
				SavedLogs.Logf( NAME_DevNet, TEXT("USES LZMA"));
			}
			INT* UzSignature = (INT*)&Data[0];
			if ( *UzSignature == 1234 || *UzSignature == 5678 )
			{
				IsCompressed = 1;
				SavedLogs.Logf( NAME_DevNet, TEXT("USES UZ: Signature %i"), *UzSignature);
			}
		}
	}

	// Receive.
	if( !RecvFileAr )
		DownloadError( *UXC_Download::NetOpenError );
	else
	{
		RecvFileAr->Serialize( Data, Count);
		if( RecvFileAr->IsError() )
			DownloadError( *FString::Printf( *UXC_Download::NetWriteError, TempFilename ) );
		else
			Transfered += Count;
	}	
}


bool UXC_HTTPDownload::AsyncReceive()
{
	uint8 Buf[4096];
	int32 Bytes = 0;
	int32 TotalBytes = 0;
	bool bShutdown = false;
	while ( Socket.Recv( Buf, sizeof(Buf), Bytes) )
	{
		if ( Bytes == 0 )
		{
			bShutdown = true;
			SavedLogs.Logf( NAME_DevNetTraffic, TEXT("Graceful shutdown"));
			break;
		}
		TotalBytes += Bytes;
		int32 Start = Response.ReceivedData.Add(Bytes);
		appMemcpy( &Response.ReceivedData(Start), Buf, Bytes);
		SavedLogs.Logf( NAME_DevNetTraffic, TEXT("Received %i bytes"), Bytes);
	}

	if ( (Socket.LastError != 0) && (Socket.LastError != FSocket::ENonBlocking) )
	{
		DownloadError( *FString::Printf( TEXT("Socket error: %s"), Socket.ErrorText( Socket.LastError)) );
		return true;
	}

	//Process received bytes
	if ( Response.Status == 0 ) //Header stage
	{
		for ( int32 i=0 ; i<Response.ReceivedData.Num()-1 ; i++ )
			if ( (Response.ReceivedData(i) == '\r') && (Response.ReceivedData(i+1) == '\n') )
			{
				Response.HeaderLines.AddZeroed();
				if ( i == 0 ) //Empty line: EOH
				{
					SavedLogs.Logf( NAME_DevNetTraffic, TEXT("HTTP Header END received") );
					Response.ReceivedData.Remove( 0, 2);
					break;
				}
				TArray<TCHAR>& NewLine = Response.HeaderLines.Last().GetCharArray();
				NewLine.Add( i+1);
				for ( int32 j=0 ; j<i ; j++ )
					NewLine(j) = (TCHAR)Response.ReceivedData(j);
				NewLine(i) = '\0';
				SavedLogs.Logf( NAME_DevNetTraffic, TEXT("HTTP Header received: %s"), *Response.HeaderLines.Last() );

				//Remove raw line and keep processing
				Response.ReceivedData.Remove( 0, i+2);
				i = -1;
			}

		//EOH included
		if ( Response.HeaderLines.Num() && (Response.HeaderLines.Last().Len() == 0) )
		{
			if ( Response.HeaderLines.Num() < 2 )
			{
				Response.Status = -1; //Internal error
				DownloadError( TEXT("Bad HTTP response") );
				return true;
			}
			const TCHAR* Line = *Response.HeaderLines(0);
			Response.Version = ParseToken( Line, 0);
			Response.Status = appAtoi( *ParseToken( Line, 0) );
			for ( int32 i=1 ; i<Response.HeaderLines.Num()-1 ; i++ )
			{
				Line = *Response.HeaderLines(i);
				FString Key = ParseToken( Line, 0);
				if ( Key[Key.Len()-1] == ':' )
					Key.GetCharArray().Remove( Key.Len()-1 );
				while ( *Line == ' ' )
					Line++;
				Response.Headers.Set( *Key, Line);
			}
			Response.HeaderLines.Empty();
		}
		else
			return bShutdown;

		AGAIN:
		if ( Response.Status == 200 ) //OK
		{
			//Cookie, only one supported for now
			FString* SetCookie = Response.Headers.Find( TEXT("Set-Cookie"));
			if ( SetCookie )
				Request.Headers.Set( TEXT("Cookie"), **SetCookie);
			//Get filesize
			FString* ContentLength = Response.Headers.Find( TEXT("Content-Length"));
			if ( ContentLength )
			{
				RealFileSize = appAtoi( **ContentLength);
				if ( RealFileSize == 0 )
				{
					Response.Status = 404;
					goto AGAIN;
				}
			}
		}
		else if ( Response.Status == 301 || Response.Status == 302 || Response.Status == 303 ||  Response.Status == 307 ) //Permanent redirect + Found + Temporary redirect
		{
			if ( Response.Status == 303 )
				Request.Method = TEXT("GET");
			FString* Location = Response.Headers.Find( TEXT("Location"));
			if ( Location )
				SavedLogs.Logf( NAME_DevNet, TEXT("Redirected (%i) to %s"), Response.Status, **Location);
			if ( !Location || !Location->Len() )
				DownloadError( TEXT("Bad redirection"));
			else if (Request.RedirectsLeft-- <= 0 )
				DownloadError( TEXT("Too many redirections") );
			else
				UpdateCurrentURL( **Location);
			return true;
		}
		else if ( Response.Status == 404 )
		{
			if ( DownloadURL.Compression > 0 ) //Change compression and retry
			{
				DownloadURL.Compression--;
				CurrentURL = DownloadURL;
			}
			else
				DownloadError( *FString::Printf( *UXC_Download::InvalidUrlError, *CurrentURL.String() ) );
			return true;
		}
		else
		{
			DownloadError( *FString::Printf( *UXC_Download::InvalidUrlError, *CurrentURL.String() ) );
			return true;
		}
	}
	
	if ( Response.Status == 200 )
	{
		int32 RealSize = RealFileSize ? RealFileSize : Info->FileSize;
		int32 Count = (Transfered + Response.ReceivedData.Num() > RealSize) ? RealSize - Transfered : Response.ReceivedData.Num();
		if ( Count > 0 )
			ReceiveData( &Response.ReceivedData(0), Count );
		Response.ReceivedData.Empty();
		if ( ((Transfered >= RealSize) || bShutdown) && RecvFileAr )
		{
			delete RecvFileAr;
			RecvFileAr = nullptr;
			bShutdown = true;
		}
	}

	return bShutdown;
}

bool UXC_HTTPDownload::AsyncLocalBind()
{
	Socket.Close();
	Socket = FSocket(true);
	if ( Socket.IsInvalid() )
	{
		DownloadError( *UXC_Download::ConnectionFailedError );
		return false;
	}
	Socket.SetReuseAddr();
	Socket.SetLinger();
	FIPv6Endpoint LocalAddress( GetLocalBindAddress(SavedLogs), 0);
	if( !Socket.BindPort( LocalAddress, 20) )
	{
		SavedLogs.Log( NAME_DevNet, TEXT("XC_HTTPDownload: bind() failed") );
		DownloadError( *UXC_Download::ConnectionFailedError );
		return false;
	}
	return true;
}

IMPLEMENT_CLASS(UXC_HTTPDownload)

#endif