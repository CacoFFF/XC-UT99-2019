/*=============================================================================
	HTTPDownload.h
	Author: Fernando Velázquez

	Asynchronous HTTP File Downloader
=============================================================================*/

#ifndef UXC_HTTPDownload_H
#define UXC_HTTPDownload_H

#pragma once

#ifndef XC_CORE_API
	#define XC_CORE_API DLL_IMPORT
#endif

#include "XC_IpDrv.h"
#include "XC_Download.h"
#include "Devices.h"


class HTTP_Request
{
public:
	FString Hostname;
	FString Method;
	FString Path;
	FString Body;
	int32 RedirectsLeft;
	TMultiMap<FString,FString> Headers;

	FString String();
};

class HTTP_Response
{
public:
	FString Version;
	int32 Status;
	TMultiMap<FString,FString> Headers;

	TArray<FString> HeaderLines;
	TArray<BYTE> ReceivedData; // EOH is an empty line!

	HTTP_Response()
		: Status(0) {}
};


//
// Simple asynchronous HTTP_Downloader
//
class UXC_HTTPDownload : public UXC_Download
{
	DECLARE_CLASS(UXC_HTTPDownload,UXC_Download,CLASS_Transient|CLASS_Config,XC_IpDrv);

public:
	FStringNoInit ProxyServerHost;
	int32         ProxyServerPort;
	float         DownloadTimeout;

	FDownloadURL DownloadURL;
	FDownloadURL CurrentURL; //Does not use RequestedPackage/Compression fields
	HTTP_Request Request;
	HTTP_Response Response;
	FSocket Socket;
	FIPv6Endpoint RemoteEndpoint;
	volatile int32 LogLock;
	FOutputDeviceAsyncStorage SavedLogs;

public:
	void StaticConstructor();
	UXC_HTTPDownload();

	//UObject interface
	void Destroy();

	//UDownload interface
	UBOOL TrySkipFile() override;
	void ReceiveFile( UNetConnection* InConnection, INT PackageIndex, const TCHAR *Params=nullptr, UBOOL InCompression=0 ) override;
	void ReceiveData( BYTE* Data, INT Count) override;
	void Tick() override;

private:
	void UpdateCurrentURL( const TCHAR* RelativeURI);

	bool AsyncReceive();
	bool AsyncLocalBind();
};



#endif