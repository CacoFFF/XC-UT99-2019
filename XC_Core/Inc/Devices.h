/*=============================================================================
	Devices.h:

	Revision history:
		* Created by Fernando Velázquez (Higor)
=============================================================================*/

#ifndef XC_DEVICES
#define XC_DEVICES

#include "FOutputDeviceFile.h"

#define OLD_LINES 16
class XC_CORE_API FLogLine
{
public:
	EName Event;
	FString Msg;
	
	FLogLine();
	FLogLine( EName InEvent, const TCHAR* InData);
	bool operator==( const FLogLine& O);
};
class XC_CORE_API FOutputDeviceInterceptor : public FOutputDevice
{
public:
	FOutputDevice* Next;
	FOutputDeviceFile* CriticalOut;
	volatile UBOOL ProcessLock;
	volatile UBOOL SerializeLock; //Serialize being called

	EName Repeater;
	DWORD RepeatCount;
	TArray<FLogLine> MessageHistory; //Newer=Lower, up to OLD_LINES
	DWORD MultiLineCount;
	DWORD MultiLineCur;

	//Constructor
	FOutputDeviceInterceptor( FOutputDevice* InNext=NULL );
	~FOutputDeviceInterceptor();

	//FOutputDevice interface
	void Serialize( const TCHAR* Msg, EName Event );

	//FOutputDeviceInterceptor
	void SetRepeaterText( const TCHAR* Text);
	void ProcessMessage( FLogLine& Line);
	void FlushRepeater();
	void SerializeNext( const TCHAR* Text, EName Event );
};

class XC_CORE_API FOutputDeviceAsyncStorage : public FOutputDevice
{
public:
	volatile int32 Lock;
	TArray<FLogLine> Store;

	//Constructor
	FOutputDeviceAsyncStorage();
	~FOutputDeviceAsyncStorage();

	//FOutputDevice interface
	void Serialize( const TCHAR* Msg, EName Event );

	//FOutputDeviceAsyncStorage
	void Flush();
	void Discard();
};


#endif
