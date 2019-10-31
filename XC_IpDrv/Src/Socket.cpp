/*============================================================================
	Socket.cpp
	Author: Fernando Velázquez

	Definitions for platform independant abstractions for Sockets
============================================================================*/


#if _WINDOWS
// WinSock includes.
	#define __WINSOCK__ 1
	#pragma comment(lib,"ws2_32.lib") //Use Winsock2
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <conio.h>
	#define MSG_NOSIGNAL		0
#else
// BSD socket includes.
	#define __BSD_SOCKETS__ 1
	#include <stdio.h>
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netinet/ip.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <errno.h>
	#include <fcntl.h>

	#ifndef MSG_NOSIGNAL
		#define MSG_NOSIGNAL 0x4000
	#endif
#endif


// Provide WinSock definitions for BSD sockets.
#if __LINUX_X86__
	#define INVALID_SOCKET      -1
	#define SOCKET_ERROR        -1

/*	#define ECONNREFUSED        111
	#define EAGAIN              11*/
#endif

#include "XC_IpDrv.h"

/*----------------------------------------------------------------------------
	Resolve.
----------------------------------------------------------------------------*/

// Resolution thread entrypoint.
unsigned long ResolveThreadEntry( void* Arg, CThread* Handler)
{
	FResolveInfo* Info = (FResolveInfo*)Arg;
	Info->Addr = ResolveHostname( Info->HostName, Info->Error);
	return THREAD_END_OK;
}



/*----------------------------------------------------------------------------
	Generic socket.
----------------------------------------------------------------------------*/

const socket_type FSocketGeneric::InvalidSocket = INVALID_SOCKET;
const int32 FSocketGeneric::Error = SOCKET_ERROR;


FSocketGeneric::FSocketGeneric()
	: Socket( INVALID_SOCKET )
	, LastError(0)
{}

static int32 FirstSocket = 0;
FSocketGeneric::FSocketGeneric( bool bTCP)
{
	FString Error;
	FSocketGeneric::Init( Error);
	Socket = socket
			(
				GIPv6 ? PF_INET6 : PF_INET, //How to open multisocket?
				bTCP ? SOCK_STREAM : SOCK_DGRAM,
				bTCP ? IPPROTO_TCP : IPPROTO_UDP
			);
	LastError = ErrorCode();

	//TODO: Check if opened

	if ( GIPv6 ) //TODO: Check socket status and enable dual-stack
	{
		int Zero = 0;
		LastError = setsockopt( Socket, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&Zero, sizeof(Zero));
	}
}

bool FSocketGeneric::Init( FString& Error)
{
	if ( !FirstSocket )
	{
		FirstSocket = 1;
		GIPv6 = ((UXC_TcpNetDriver*)UXC_TcpNetDriver::StaticClass()->GetDefaultObject())->UseIPv6;
	}
	return true;
}

bool FSocketGeneric::Connect( FIPv6Endpoint& RemoteAddress)
{
	if ( GIPv6 )
	{
		sockaddr_in6 addr = RemoteAddress.ToSockAddr6();
		LastError = connect( Socket, (sockaddr*)&addr, sizeof(addr));
	}
	else
	{
		sockaddr_in addr = RemoteAddress.ToSockAddr();
		LastError = connect( Socket, (sockaddr*)&addr, sizeof(addr));
	}
	if ( LastError )
		LastError = FSocket::ErrorCode();
	return LastError == 0;
}

bool FSocketGeneric::Send( const uint8* Buffer, int32 BufferSize, int32& BytesSent)
{
	BytesSent = send( Socket, (const char*)Buffer, BufferSize, 0);
	LastError = (BytesSent < 0) ? FSocket::ErrorCode() : 0;
	return BytesSent >= 0;
}

bool FSocketGeneric::SendTo( const uint8* Buffer, int32 BufferSize, int32& BytesSent, const FIPv6Endpoint& Dest)
{
	if ( GIPv6 )
	{
		sockaddr_in6 addr = Dest.ToSockAddr6();
		BytesSent = sendto( Socket, (const char*)Buffer, BufferSize, 0, (sockaddr*)&addr, sizeof(addr) );
	}
	else
	{
		sockaddr_in addr = Dest.ToSockAddr();
		BytesSent = sendto( Socket, (const char*)Buffer, BufferSize, 0, (sockaddr*)&addr, sizeof(addr) );
	}
	LastError = (BytesSent < 0) ? FSocket::ErrorCode() : 0;
	return BytesSent >= 0;
}

bool FSocketGeneric::Recv( uint8* Data, int32 BufferSize, int32& BytesRead)
{
	BytesRead = recv( Socket, (char*)Data, BufferSize, 0);
	LastError = (BytesRead < 0) ? FSocket::ErrorCode() : 0;
	return BytesRead >= 0;
}

bool FSocketGeneric::RecvFrom( uint8* Data, int32 BufferSize, int32& BytesRead, FIPv6Endpoint& Source)
{
	uint8 addrbuf[28]; //Size of sockaddr_6 is 28, this should be safe for both kinds of sockets
	int32 addrsize = sizeof(addrbuf);

	BytesRead = recvfrom( Socket, (char*)Data, BufferSize, 0, (sockaddr*)addrbuf, (socklen_t*)&addrsize);
	if ( GIPv6 )
		Source = *(sockaddr_in6*)addrbuf;
	else
		Source = *(sockaddr_in*)addrbuf;
	LastError = (BytesRead < 0) ? FSocket::ErrorCode() : 0;
	return BytesRead >= 0;
}

bool FSocketGeneric::EnableBroadcast( bool bEnable)
{
	int32 Enable = bEnable ? 1 : 0;
	LastError = setsockopt( Socket, SOL_SOCKET, SO_BROADCAST, (char*)&Enable, sizeof(Enable));
	return LastError == 0;
}

void FSocketGeneric::SetQueueSize( int32 RecvSize, int32 SendSize)
{
	socklen_t BufSize = sizeof(RecvSize);
	setsockopt( Socket, SOL_SOCKET, SO_RCVBUF, (char*)&RecvSize, BufSize );
	getsockopt( Socket, SOL_SOCKET, SO_RCVBUF, (char*)&RecvSize, &BufSize );
	setsockopt( Socket, SOL_SOCKET, SO_SNDBUF, (char*)&SendSize, BufSize );
	getsockopt( Socket, SOL_SOCKET, SO_SNDBUF, (char*)&SendSize, &BufSize );
	debugf( NAME_Init, TEXT("%s: Socket queue %i / %i"), FSocket::API, RecvSize, SendSize );
}

uint16 FSocketGeneric::BindPort( FIPv6Endpoint& LocalAddress, int NumTries, int Increment)
{
	for( int32 i=0 ; i<NumTries ; i++ )
	{
		if ( GIPv6 )
		{
			sockaddr_in6 addr = LocalAddress.ToSockAddr6();
			LastError = bind( Socket, (sockaddr*)&addr, sizeof(addr));
			if( !LastError ) //Zero ret = success
			{
				if ( LocalAddress.Port == 0 ) //A random client port was requested, get it
				{
					sockaddr_in6 bound;
					int32 size = sizeof(bound);
					LastError = getsockname( Socket, (sockaddr*)(&bound), (socklen_t*)&size);
					LocalAddress.Port = ntohs(bound.sin6_port);
				}
				return LocalAddress.Port;
			}
		}
		else
		{
			sockaddr_in addr = LocalAddress.ToSockAddr();
			LastError = bind( Socket, (sockaddr*)&addr, sizeof(addr));
			if( !LastError ) //Zero ret = success
			{
				if ( LocalAddress.Port == 0 ) //A random client port was requested, get it
				{
					sockaddr_in bound;
					int32 size = sizeof(bound);
					LastError = getsockname( Socket, (sockaddr*)(&bound), (socklen_t*)&size);
					LocalAddress.Port = ntohs(bound.sin_port);
				}
				return LocalAddress.Port;
			}
		}

		if( LocalAddress.Port == 0 ) //Random binding failed/went full circle in port range
			break;
		LocalAddress.Port += Increment;
	}
	return 0;
}

ESocketState FSocketGeneric::CheckState( ESocketState CheckFor, double WaitTime)
{
	fd_set SocketSet;
	timeval Time;

	Time.tv_sec = appFloor(WaitTime);
	Time.tv_usec = appFloor((WaitTime - (double)Time.tv_sec) * 1000.0 * 1000.0);
	FD_ZERO(&SocketSet);
	FD_SET(Socket, &SocketSet);

	int Status = 0;
	if      ( CheckFor == SOCKET_Readable ) Status = select(Socket + 1, &SocketSet, nullptr, nullptr, &Time);
	else if ( CheckFor == SOCKET_Writable ) Status = select(Socket + 1, nullptr, &SocketSet, nullptr, &Time);
	else if ( CheckFor == SOCKET_HasError ) Status = select(Socket + 1, nullptr, nullptr, &SocketSet, &Time);
	LastError = Status;

	if ( Status == Error )
		return SOCKET_HasError;
	else if ( Status == 0 )
		return SOCKET_Timeout;
	return CheckFor;
}

/*----------------------------------------------------------------------------
	Windows socket.
----------------------------------------------------------------------------*/
#ifdef __WINSOCK__

const int32 FSocketWindows::ENonBlocking = WSAEWOULDBLOCK;
const int32 FSocketWindows::EPortUnreach = WSAECONNRESET;
const TCHAR* FSocketWindows::API = TEXT("WinSock");

bool FSocketWindows::Init( FString& ErrorString )
{
	FSocketGeneric::Init( ErrorString);

	// Init WSA.
	static uint32 Tried = 0;
	if( !Tried )
	{
		Tried = 1;
		WSADATA WSAData;
		int32 Code = WSAStartup( MAKEWORD(2,2), &WSAData );
		if( !Code )
		{
			debugf( NAME_Init, TEXT("WinSock: version %i.%i (%i.%i), MaxSocks=%i, MaxUdp=%i"),
				WSAData.wVersion>>8,WSAData.wVersion&255,
				WSAData.wHighVersion>>8,WSAData.wHighVersion&255,
				WSAData.iMaxSockets,WSAData.iMaxUdpDg
			);
		}
		else
			ErrorString = FString::Printf( TEXT("WSAStartup failed (%s)"), ErrorText(Code) );
	}
	return true;
}

bool FSocketWindows::Close()
{
	if ( Socket != INVALID_SOCKET )
	{
		LastError = closesocket( Socket);
		Socket = INVALID_SOCKET;
		return LastError == 0;
	}
	return false;
}

// This connection will not block the thread, must poll repeatedly to see if it's properly established
bool FSocketWindows::SetNonBlocking()
{
	uint32 NoBlock = 1;
	LastError = ioctlsocket( Socket, FIONBIO, &NoBlock );
	return LastError == 0;
}

// Reopen connection if a packet arrived after being closed? (apt for servers)
bool FSocketWindows::SetReuseAddr( bool bReUse )
{
	char optval = bReUse ? 1 : 0;
	LastError = setsockopt( Socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	bool bSuccess = (LastError == 0);
	if ( !bSuccess )
		debugf(TEXT("setsockopt with SO_REUSEADDR failed"));
	return bSuccess;
}

// This connection will not gracefully shutdown, and will discard all pending data when closed
bool FSocketWindows::SetLinger()
{
	linger ling;
	ling.l_onoff  = 1;	// linger on
	ling.l_linger = 0;	// timeout in seconds
	LastError = setsockopt( Socket, SOL_SOCKET, SO_LINGER, (char*)&ling, sizeof(ling));
	return LastError == 0;
}

const TCHAR* FSocketWindows::ErrorText( int32 Code)
{
	if( Code == -1 )
		Code = WSAGetLastError();
	switch( Code )
	{
	#define CASE(n) case n: return TEXT(#n);
		CASE(WSAEINTR)
		CASE(WSAEBADF)
		CASE(WSAEACCES)
		CASE(WSAEFAULT)
		CASE(WSAEINVAL)
		CASE(WSAEMFILE)
		CASE(WSAEWOULDBLOCK)
		CASE(WSAEINPROGRESS)
		CASE(WSAEALREADY)
		CASE(WSAENOTSOCK)
		CASE(WSAEDESTADDRREQ)
		CASE(WSAEMSGSIZE)
		CASE(WSAEPROTOTYPE)
		CASE(WSAENOPROTOOPT)
		CASE(WSAEPROTONOSUPPORT)
		CASE(WSAESOCKTNOSUPPORT)
		CASE(WSAEOPNOTSUPP)
		CASE(WSAEPFNOSUPPORT)
		CASE(WSAEAFNOSUPPORT)
		CASE(WSAEADDRINUSE)
		CASE(WSAEADDRNOTAVAIL)
		CASE(WSAENETDOWN)
		CASE(WSAENETUNREACH)
		CASE(WSAENETRESET)
		CASE(WSAECONNABORTED)
		CASE(WSAECONNRESET)
		CASE(WSAENOBUFS)
		CASE(WSAEISCONN)
		CASE(WSAENOTCONN)
		CASE(WSAESHUTDOWN)
		CASE(WSAETOOMANYREFS)
		CASE(WSAETIMEDOUT)
		CASE(WSAECONNREFUSED)
		CASE(WSAELOOP)
		CASE(WSAENAMETOOLONG)
		CASE(WSAEHOSTDOWN)
		CASE(WSAEHOSTUNREACH)
		CASE(WSAENOTEMPTY)
		CASE(WSAEPROCLIM)
		CASE(WSAEUSERS)
		CASE(WSAEDQUOT)
		CASE(WSAESTALE)
		CASE(WSAEREMOTE)
		CASE(WSAEDISCON)
		CASE(WSASYSNOTREADY)
		CASE(WSAVERNOTSUPPORTED)
		CASE(WSANOTINITIALISED)
		CASE(WSAHOST_NOT_FOUND)
		CASE(WSATRY_AGAIN)
		CASE(WSANO_RECOVERY)
		CASE(WSANO_DATA)
		case 0:						return TEXT("WSANO_ERROR");
		default:					return TEXT("WSA_Unknown");
	#undef CASE
	}
}

int32 FSocketWindows::ErrorCode()
{
	return WSAGetLastError();
}


ESocketState FSocketWindows::CheckState( ESocketState CheckFor, double WaitTime)
{
	fd_set SocketSet;
	timeval Time;

	int Status = 0;
	while ( (WaitTime >= 0) && (Status == 0) )
	{
		double StartTime = appSecondsNew();
		Time.tv_sec = appFloor(WaitTime);
		Time.tv_usec = appFloor((WaitTime - (double)Time.tv_sec) * 1000.0 * 1000.0);
		FD_ZERO(&SocketSet);
		FD_SET(Socket, &SocketSet);

		if      ( CheckFor == SOCKET_Readable ) Status = select(Socket + 1, &SocketSet, nullptr, nullptr, &Time);
		else if ( CheckFor == SOCKET_Writable ) Status = select(Socket + 1, nullptr, &SocketSet, nullptr, &Time);
		else if ( CheckFor == SOCKET_HasError ) Status = select(Socket + 1, nullptr, nullptr, &SocketSet, &Time);
		LastError = Status;

		if ( Status == Error )
			return SOCKET_HasError;
		else if ( Status == 0 )
		{
			if ( WaitTime > 0 ) //Do not spin
				appSleep( 0.001f);
			WaitTime -= appSecondsNew() - StartTime;
			if ( WaitTime <= 0 ) //Timed out
				return SOCKET_Timeout;
		}
	}
	return CheckFor;
}

#endif
/*----------------------------------------------------------------------------
	Unix socket.
----------------------------------------------------------------------------*/
#ifdef __BSD_SOCKETS__

const int32 FSocketBSD::ENonBlocking = EAGAIN;
const int32 FSocketBSD::EPortUnreach = ECONNREFUSED;
const TCHAR* FSocketBSD::API = TEXT("Sockets");

bool FSocketBSD::Close()
{
	if ( Socket != INVALID_SOCKET )
	{
		LastError = close( Socket);
		Socket = INVALID_SOCKET;
		return LastError == 0;
	}
	return false;
}

// This connection will not block the thread, must poll repeatedly to see if it's properly established
bool FSocketBSD::SetNonBlocking()
{
	int32 pd_flags;
	pd_flags = fcntl( Socket, F_GETFL, 0 );
	pd_flags |= O_NONBLOCK;
	LastError = fcntl( Socket, F_SETFL, pd_flags );
	return LastError == 0;
}

// Reopen connection if a packet arrived after being closed? (apt for servers)
bool FSocketBSD::SetReuseAddr( bool bReUse )
{
	int32 optval = bReUse ? 1 : 0;
	LastError = setsockopt( Socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	return LastError == 0;
}

// This connection will not gracefully shutdown, and will discard all pending data when closed
bool FSocketBSD::SetLinger()
{
	linger ling;
	ling.l_onoff  = 1;	// linger on
	ling.l_linger = 0;	// timeout in seconds
	LastError = setsockopt( Socket, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
	return LastError == 0;
}

bool FSocketBSD::SetRecvErr()
{
	int32 on = 1;
	LastError = setsockopt(Socket, SOL_IP, IP_RECVERR, &on, sizeof(on));
	bool bSuccess = (LastError == 0);
	if ( !bSuccess )
		debugf(TEXT("setsockopt with IP_RECVERR failed"));
	return bSuccess;
}

const TCHAR* FSocketBSD::ErrorText( int32 Code)
{
	if( Code == -1 )
		Code = errno;
	switch( Code )
	{
	case EINTR:					return TEXT("EINTR");
	case EBADF:					return TEXT("EBADF");
	case EACCES:				return TEXT("EACCES");
	case EFAULT:				return TEXT("EFAULT");
	case EINVAL:				return TEXT("EINVAL");
	case EMFILE:				return TEXT("EMFILE");
	case EWOULDBLOCK:			return TEXT("EWOULDBLOCK");
	case EINPROGRESS:			return TEXT("EINPROGRESS");
	case EALREADY:				return TEXT("EALREADY");
	case ENOTSOCK:				return TEXT("ENOTSOCK");
	case EDESTADDRREQ:			return TEXT("EDESTADDRREQ");
	case EMSGSIZE:				return TEXT("EMSGSIZE");
	case EPROTOTYPE:			return TEXT("EPROTOTYPE");
	case ENOPROTOOPT:			return TEXT("ENOPROTOOPT");
	case EPROTONOSUPPORT:		return TEXT("EPROTONOSUPPORT");
	case ESOCKTNOSUPPORT:		return TEXT("ESOCKTNOSUPPORT");
	case EOPNOTSUPP:			return TEXT("EOPNOTSUPP");
	case EPFNOSUPPORT:			return TEXT("EPFNOSUPPORT");
	case EAFNOSUPPORT:			return TEXT("EAFNOSUPPORT");
	case EADDRINUSE:			return TEXT("EADDRINUSE");
	case EADDRNOTAVAIL:			return TEXT("EADDRNOTAVAIL");
	case ENETDOWN:				return TEXT("ENETDOWN");
	case ENETUNREACH:			return TEXT("ENETUNREACH");
	case ENETRESET:				return TEXT("ENETRESET");
	case ECONNABORTED:			return TEXT("ECONNABORTED");
	case ECONNRESET:			return TEXT("ECONNRESET");
	case ENOBUFS:				return TEXT("ENOBUFS");
	case EISCONN:				return TEXT("EISCONN");
	case ENOTCONN:				return TEXT("ENOTCONN");
	case ESHUTDOWN:				return TEXT("ESHUTDOWN");
	case ETOOMANYREFS:			return TEXT("ETOOMANYREFS");
	case ETIMEDOUT:				return TEXT("ETIMEDOUT");
	case ECONNREFUSED:			return TEXT("ECONNREFUSED");
	case ELOOP:					return TEXT("ELOOP");
	case ENAMETOOLONG:			return TEXT("ENAMETOOLONG");
	case EHOSTDOWN:				return TEXT("EHOSTDOWN");
	case EHOSTUNREACH:			return TEXT("EHOSTUNREACH");
	case ENOTEMPTY:				return TEXT("ENOTEMPTY");
	case EUSERS:				return TEXT("EUSERS");
	case EDQUOT:				return TEXT("EDQUOT");
	case ESTALE:				return TEXT("ESTALE");
	case EREMOTE:				return TEXT("EREMOTE");
	case HOST_NOT_FOUND:		return TEXT("HOST_NOT_FOUND");
	case TRY_AGAIN:				return TEXT("TRY_AGAIN");
	case NO_RECOVERY:			return TEXT("NO_RECOVERY");
	case 0:						return TEXT("NO_ERROR");
	default:					return TEXT("Unknown");
	}
}

int32 FSocketBSD::ErrorCode()
{
	return errno;
}

#endif

/*----------------------------------------------------------------------------
	Non-blocking resolver.
----------------------------------------------------------------------------*/

FResolveInfo::FResolveInfo( const TCHAR* InHostName )
	: CThread()
{
	debugf( TEXT("[XC] Resolving %s..."), InHostName );

	appStrncpy( HostName, InHostName, 255);
	Error[0] = '\0';

	Run( &ResolveThreadEntry, this);
}

int32 FResolveInfo::Resolved()
{
	return WaitFinish( 0.001f);
}
const TCHAR* FResolveInfo::GetError() const
{
	return *Error ? Error : NULL;
}
const TCHAR* FResolveInfo::GetHostName() const
{
	return HostName;
}


/*----------------------------------------------------------------------------
	Other.
----------------------------------------------------------------------------*/

FIPv6Address ResolveHostname( const TCHAR* HostName, TCHAR* Error, UBOOL bOnlyParse)
{
	addrinfo Hint, Hint4, *Result;
	appMemzero( &Hint, sizeof(Hint) );
	Hint.ai_family = GIPv6 ? AF_INET6 : AF_INET; //Get IPv4 or IPv6 addresses
	if ( GIPv6 )
		Hint.ai_flags |= AI_V4MAPPED | AI_ADDRCONFIG;
	if ( bOnlyParse )
		Hint.ai_flags |= AI_NUMERICHOST;

	if ( GIPv6 && bOnlyParse ) //IPv4 cannot be properly parsed with AF_INET6
	{
		appMemzero( &Hint4, sizeof(Hint4) );
		Hint4.ai_family = AF_INET;
		Hint4.ai_flags = AI_NUMERICHOST;
		Hint.ai_next = &Hint4;
	}

	FIPv6Address Address = FIPv6Address::Any;
	int32 ErrorCode;

	//Purposely failing to pass HostName will trigger 'gethostname' (useful for init)
	if ( *HostName == '\0' )
	{
		ANSICHAR AnsiHostName[256] = "";
		if ( gethostname( AnsiHostName, 256) )
		{
			appSprintf( Error, TEXT("gethostname failed (%s)"), FSocket::ErrorText() );
			return Address;
		}
		else
		{
			TCHAR* HostNameTmp = ANSI_TO_TCHAR(AnsiHostName);
			int32 i;
			for ( i=0 ; i<255 && HostNameTmp[i] ; i++ )
				((TCHAR*)HostName)[i] = HostNameTmp[i];
			((TCHAR*)HostName)[i] = '\0';
		}
		ErrorCode = getaddrinfo( AnsiHostName, NULL, &Hint, &Result);
	}
	else
	{
		ANSICHAR* AnsiHostName = TCHAR_TO_ANSI_HEAP( HostName);
		ErrorCode = getaddrinfo( AnsiHostName, NULL, &Hint, &Result);
		appFree( AnsiHostName);
	}

	if ( ErrorCode != 0 )
	{
		if ( Error )
			appSprintf( Error, TEXT("getaddrinfo failed %s: %s"), HostName, gai_strerror(ErrorCode));
	}
	else
	{
		if ( GIPv6 ) //Prioritize IPv6 result
		{
			for ( addrinfo* Link=Result ; Link ; Link=Link->ai_next )
				if ( Link->ai_family==AF_INET6 && Link->ai_addr )
				{
					in6_addr* addr = &((sockaddr_in6*)Link->ai_addr)->sin6_addr; //IPv6 struct
					if ( IN6_IS_ADDR_LINKLOCAL( addr) )
						continue;
					Address = *addr; 
					if ( Address != FIPv6Address::Any )
						return Address;
				}
		}

		for ( addrinfo* Link=Result ; Link ; Link=Link->ai_next )
			if ( (Link->ai_family==AF_INET) && Link->ai_addr )
			{
				Address = ((sockaddr_in*)Link->ai_addr)->sin_addr; //IPv4 struct
				if ( Address != FIPv6Address::Any )
					return Address;
			}

		if ( Error )
			appSprintf( Error, TEXT("Unable to find host %s"), HostName);
	}
	return Address;
}


// Get local IP to bind to
FIPv6Address GetLocalBindAddress( FOutputDevice& Out)
{
	UBOOL bCanBindAll;

	FIPv6Address HostAddr = GetLocalHostAddress( Out, bCanBindAll);
	if ( bCanBindAll )
		HostAddr = FIPv6Address::Any;
	return HostAddr;
}

//Should export as C
FIPv6Address GetLocalHostAddress( FOutputDevice& Out, UBOOL& bCanBindAll)
{
	guard(GetLocalHostAddress);
	FIPv6Address HostAddr = FIPv6Address::Any;
	TCHAR Home[256] = TEXT("");
	TCHAR Error[256] = TEXT("");

	bCanBindAll = false;

	if ( Parse( appCmdLine(), TEXT("MULTIHOME="), Home, ARRAY_COUNT(Home)) )
	{
		HostAddr = ResolveHostname( Home, Error/*, 1*/);
		if ( *Error || HostAddr==FIPv6Address::Any )
			Out.Logf( TEXT("Invalid multihome IP address %s"), Home);
//		else
//			Out.Logf( NAME_Init, TEXT("%s: Multihome %s resolved to (%s)"), FSocket::API, Home, *HostAddr.String() );
	}
	else
	{
		HostAddr = ResolveHostname( Home, Error); //get local host name
		if ( HostAddr != FIPv6Address::Any )
		{
			if( !ParseParam(appCmdLine(),TEXT("PRIMARYNET")) )
				bCanBindAll = true;
			static uint32 First = 0;
			if( !First )
			{
				First = 1;
				Out.Logf( NAME_Init, TEXT("%s: I am %s (%s)"), FSocket::API, Home, *HostAddr.String() );
			}
		}
	}
	if ( *Error )
		Out.Log( Error);
	return HostAddr;
	unguard;
}

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

