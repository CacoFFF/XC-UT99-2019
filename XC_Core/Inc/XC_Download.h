/*=============================================================================
	XC_Core extended download protocols
=============================================================================*/

#ifndef _INC_XC_DL
#define _INC_XC_DL

#include "Cacus/CacusThread.h"

class XC_CORE_API UXC_Download : public UDownload
{
	DECLARE_ABSTRACT_CLASS(UXC_Download,UDownload,CLASS_Transient|CLASS_Config,XC_Core);

	UBOOL EnableLZMA;

	BYTE IsLZMA;
	BYTE IsInvalid; //Remove from download list ASAP

	int32 OldTransfered;
	volatile int32 AsyncAction; //Worker thread in progress
	volatile int32 Finished; //Destroy this channel on main thread
	static volatile int32 GlobalLock; //Prevents destruction of download managers

	static FString NetSizeError;
	static FString NetOpenError;
	static FString NetWriteError;
	static FString NetMoveError;
	static FString InvalidUrlError;
	static FString ConnectionFailedError;

	// Constructors.
	void StaticConstructor();
	UXC_Download();

	// UObject interface.
	void Destroy();

	// UDownload interface
	void Tick();
	void DownloadDone();
	void ReceiveData( BYTE* Data, INT Count );
};

class XC_CORE_API UXC_ChannelDownload : public UXC_Download
{
	DECLARE_CLASS(UXC_ChannelDownload,UXC_Download,CLASS_Transient|CLASS_Config,XC_Core);
    NO_DEFAULT_CONSTRUCTOR(UXC_ChannelDownload);
	
	// Variables.
	UFileChannel* Ch;

	// Constructors.
	void StaticConstructor();

	// UObject interface.
	void Destroy();
	void Serialize( FArchive& Ar );

	// UDownload Interface.
	void ReceiveFile( UNetConnection* InConnection, INT PackageIndex, const TCHAR *Params=NULL, UBOOL InCompression=0 );
	UBOOL TrySkipFile();
};

//
// A channel for exchanging binary files.
//
class XC_CORE_API UXC_FileChannel : public UFileChannel
{
	DECLARE_CLASS(UXC_FileChannel,UFileChannel,CLASS_Transient,XC_Core);

	// Receive Variables.
/*	UChannelDownload*	Download;		 // UDownload when receiving.

	// Send Variables.
	FArchive*			SendFileAr;		 // File being sent.
	TCHAR				SrcFilename[256];// Filename being sent.
	INT					PackageIndex;	 // Index of package in map.
	INT					SentData;		 // Number of bytes sent.
*/

	// Constructor.
	void StaticConstructor()
	{
		UChannel::ChannelClasses[7] = GetClass();
		GetDefault<UXC_FileChannel>()->ChType = (EChannelType)7;
	}
	UXC_FileChannel();
	void Init( UNetConnection* InConnection, INT InChIndex, UBOOL InOpenedLocally );
	void Destroy();

	// UChannel interface.
	void ReceivedBunch( FInBunch& Bunch );

	// UFileChannel interface.
//	FString Describe();
	void Tick();
};


class XC_CORE_API FDownloadAsyncProcessor : public CThread
{
public:
	typedef void (*ASYNC_PROC)(FDownloadAsyncProcessor*);

	UXC_Download* Download;
	int32 DownloadTag;
	ASYNC_PROC Proc;

	FDownloadAsyncProcessor( const ASYNC_PROC InProc, UXC_Download* InDownload);

	bool DownloadActive();
};



#endif
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
