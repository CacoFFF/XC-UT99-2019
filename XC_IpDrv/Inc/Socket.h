/*============================================================================
	Socket.h
	Author: Fernando Velázquez

	Platform independant abstractions for Sockets
	Inspired in UE4's sockets.
============================================================================*/

#ifndef INC_SOCKET_H
#define INC_SOCKET_H

/*-----------------------------------------------------------------------------
	Definitions.
-----------------------------------------------------------------------------*/

#if _WINDOWS
	typedef uint32 socket_type;
#else
	typedef int32 socket_type;
#endif

/*----------------------------------------------------------------------------
	Unified socket system 
----------------------------------------------------------------------------*/

enum ESocketState
{
	SOCKET_Timeout, //Used for return values
	SOCKET_Readable,
	SOCKET_Writable,
	SOCKET_HasError,
	SOCKET_MAX
};

/*----------------------------------------------------------------------------
	FSocket abstraction (win32/unix).
----------------------------------------------------------------------------*/

class FSocketGeneric
{
protected:
	socket_type Socket; //Should not be doing this!
public:
	int32 LastError;
public:
	static const socket_type InvalidSocket;
	static const int32 Error;

	FSocketGeneric();
	FSocketGeneric( bool bTCP);

	static bool Init( FString& Error);
	static const TCHAR* ErrorText( int32 Code=-1)     {return TEXT("");}
	static int32 ErrorCode()                {return 0;}

	bool Close()                            {SetInvalid(); return false;}
	bool IsInvalid()                        {return Socket==InvalidSocket;}
	void SetInvalid()                       {Socket=InvalidSocket;}
	bool SetNonBlocking()                   {return true;}
	bool SetReuseAddr( bool bReUse=true)    {return true;}
	bool SetLinger()                        {return true;}
	bool SetRecvErr()                       {return false;}

	bool Connect( FIPv6Endpoint& RemoteAddress);
	bool Send( const uint8* Buffer, int32 BufferSize, int32& BytesSent);
	bool SendTo( const uint8* Buffer, int32 BufferSize, int32& BytesSent, const FIPv6Endpoint& Dest);
	bool Recv( uint8* Data, int32 BufferSize, int32& BytesRead); //Implement flags later
	bool RecvFrom( uint8* Data, int32 BufferSize, int32& BytesRead, FIPv6Endpoint& Source); //Implement flags later, add IPv6 type support
	bool EnableBroadcast( bool bEnable=1);
	void SetQueueSize( int32 RecvSize, int32 SendSize);
	uint16 BindPort( FIPv6Endpoint& LocalAddress, int NumTries=1, int Increment=1);
	ESocketState CheckState( ESocketState CheckFor, double WaitTime=0);
};

#ifdef _WINDOWS
class FSocketWindows : public FSocketGeneric
{
public:
	static const int32 ENonBlocking;
	static const int32 EPortUnreach;
	static const TCHAR* API;

	FSocketWindows() {}
	FSocketWindows( bool bTCP) : FSocketGeneric(bTCP) {}

	static bool Init( FString& Error);
	static const TCHAR* ErrorText( int32 Code=-1);
	static int32 ErrorCode();
	
	bool Close();
	bool SetNonBlocking();
	bool SetReuseAddr( bool bReUse=true);
	bool SetLinger();

	ESocketState CheckState( ESocketState CheckFor, double WaitTime=0);
};
typedef FSocketWindows FSocket;


#else
class FSocketBSD : public FSocketGeneric
{
public:
	static const int32 ENonBlocking;
	static const int32 EPortUnreach;
	static const TCHAR* API;

	FSocketBSD() {}
	FSocketBSD( bool bTCP) : FSocketGeneric(bTCP) {}

	static const TCHAR* ErrorText( int32 Code=-1);
	static int32 ErrorCode();

	bool Close();
	bool SetNonBlocking();
	bool SetReuseAddr( bool bReUse=true);
	bool SetLinger();
	bool SetRecvErr();
};
typedef FSocketBSD FSocket;


#endif

/*----------------------------------------------------------------------------
	Non-blocking resolver.
----------------------------------------------------------------------------*/

class FResolveInfo : public CThread
{
public:
	// Variables.
	FIPv6Address Addr;
	TCHAR Error[256];
	TCHAR HostName[256];

	// Functions.
	FResolveInfo( const TCHAR* InHostName );

	int32 Resolved();
	const TCHAR* GetError() const; //Returns nullptr in absence of error
	const TCHAR* GetHostName() const;
};


/*----------------------------------------------------------------------------
	Functions.
----------------------------------------------------------------------------*/

FIPv6Address ResolveHostname( const TCHAR* HostName, TCHAR* Error, UBOOL bOnlyParse=0);
FIPv6Address GetLocalBindAddress( FOutputDevice& Out);
FIPv6Address GetLocalHostAddress( FOutputDevice& Out, UBOOL& bCanBindAll);

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

#endif
