/*=============================================================================
	TcpNetDriver.cpp

	Unreal TCP/IP driver.
=============================================================================*/

#include "XC_IpDrv.h"

/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

// Size of a UDP header.
#define IP_HEADER_SIZE     (20)
#define UDP_HEADER_SIZE    (IP_HEADER_SIZE+8)
#define SLIP_HEADER_SIZE   (UDP_HEADER_SIZE+4)
#define WINSOCK_MAX_PACKET (512)
#define NETWORK_MAX_PACKET (576)

/*-----------------------------------------------------------------------------
	UXC_TcpipConnection.
-----------------------------------------------------------------------------*/

//
// Windows socket class.
//
// Constructors and destructors.
UXC_TcpipConnection::UXC_TcpipConnection( FSocket InSocket, UNetDriver* InDriver, FIPv6Endpoint InRemoteAddress, EConnectionState InState, UBOOL InOpenedLocally, const FURL& InURL )
:	UNetConnection	( InDriver, InURL )
,	Socket			( InSocket )
,	RemoteAddress	( InRemoteAddress )
,	OpenedLocally	( InOpenedLocally )
{
	// Init the connection.
	State          = InState;
	MaxPacket      = WINSOCK_MAX_PACKET;
	PacketOverhead = SLIP_HEADER_SIZE;
	InitOut();

	// In connecting, figure out IP address.
	if( InOpenedLocally )
	{
		RemoteAddress.Address = ResolveHostname( *InURL.Host, NULL, 1);
		if ( RemoteAddress.Address == FIPv6Address::Any )
			ResolveInfo = new FResolveInfo( *InURL.Host);
		RemoteAddress.Port = InURL.Port ? InURL.Port : 7777;
	}
}

void UXC_TcpipConnection::LowLevelSend( void* Data, int32 Count )
{
	if ( ResolveInfo )
	{
		if( !ResolveInfo->Resolved() )
			return;
		else if( ResolveInfo->GetError() )
		{
			// Host name resolution just now failed.
			debugf( NAME_Log, ResolveInfo->GetError() );
			Driver->ServerConnection->State = USOCK_Closed;
			delete ResolveInfo;
			ResolveInfo = NULL;
			return;
		}
		else
		{
			// Host name resolution just now succeeded.
			RemoteAddress.Address = ResolveInfo->Addr;
			debugf( TEXT("Resolved %s (%s)"), ResolveInfo->GetHostName(), *RemoteAddress.Address.String() );
			delete ResolveInfo;
			ResolveInfo = NULL;
		}
	}
		// Send to remote.
	clockFast(Driver->SendCycles);
	int32 Sent;
	Socket.SendTo( (uint8*)Data, Count, Sent, RemoteAddress); //Should evaluate Sent?
	unclockFast(Driver->SendCycles);
}

FString UXC_TcpipConnection::LowLevelGetRemoteAddress()
{
	return RemoteAddress.String();
}

FString UXC_TcpipConnection::LowLevelDescribe()
{
	return FString::Printf
	(
		TEXT("%s %s state: %s"), *URL.Host, *RemoteAddress.String(),
			State==USOCK_Pending	?	TEXT("Pending")
		:	State==USOCK_Open		?	TEXT("Open")
		:	State==USOCK_Closed		?	TEXT("Closed")
		:								TEXT("Invalid")
	);
}

IMPLEMENT_CLASS(UXC_TcpipConnection);

/*-----------------------------------------------------------------------------
	UXC_TcpNetDriver.
-----------------------------------------------------------------------------*/

//
// Windows sockets network driver.
//
UBOOL UXC_TcpNetDriver::InitConnect( FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error )
{
	if( !Super::InitConnect( InNotify, ConnectURL, Error ) )
		return 0;
	if( !InitBase( 1, InNotify, ConnectURL, Error ) )
		return 0;

	// Create new connection.
	FIPv6Endpoint Endpoint = FIPv6Endpoint( FIPv6Address::Any, ConnectURL.Port);
	ServerConnection = new UXC_TcpipConnection( Socket, this, Endpoint, USOCK_Pending, 1, ConnectURL );
	debugf( NAME_DevNet, TEXT("Game client on port %i, rate %i"), LocalAddress.Port, ServerConnection->CurrentNetSpeed );

	// Create control channel (channel zero).
	GetServerConnection()->CreateChannel( CHTYPE_Control, 1, 0 );

	return 1;
}

UBOOL UXC_TcpNetDriver::InitListen( FNetworkNotify* InNotify, FURL& LocalURL, FString& Error )
{
	if( !Super::InitListen( InNotify, LocalURL, Error ) )
		return 0;
	if( !InitBase( 0, InNotify, LocalURL, Error ) )
		return 0;

	// Update result URL.
	LocalURL.Host = LocalAddress.Address.String();
	LocalURL.Port = LocalAddress.Port;
	debugf( NAME_DevNet, TEXT("TcpNetDriver on port %i%s"), LocalURL.Port, GIPv6 ? TEXT(", uses IPv6") : TEXT("") );

	return 1;
}

void UXC_TcpNetDriver::TickDispatch( float DeltaTime )
{
	if ( DeltaTime > 0 ) //Avoid unnecessary iterations, this is caused by connection handler doing extra polls
		Super::TickDispatch( DeltaTime );

	// Process all incoming packets.
	uint8 Data[NETWORK_MAX_PACKET];

#ifdef __LINUX_X86__
	INT LoopMax = (1+ClientConnections.Num()) * 500; //See what's up in linux
#endif

	for( ; ; )
	{
		// Get data, if any.
		clockFast(RecvCycles);
		int32 Size;
		FIPv6Endpoint Endpoint;
		Socket.RecvFrom( Data, sizeof(Data), Size, Endpoint);
		unclockFast(RecvCycles);
		

#ifdef __LINUX_X86__
		if ( LoopMax-- <= 0 )
			break;
#endif
		// Handle result.
		if( Size == FSocket::Error )
		{
			int32 Error = FSocket::ErrorCode();
			if ( Error == FSocket::ENonBlocking )
				break; // No data
			else if ( Error != FSocket::EPortUnreach )
			{
				static UBOOL FirstError=1;
				if ( FirstError )
					debugf( TEXT("UDP recvfrom error: %i from %s"), FSocket::ErrorText(Error), *Endpoint.String() );
				FirstError = 0;
				break;
			}
		}
		// Figure out which socket the received data came from.
		UXC_TcpipConnection* Connection = NULL;
		if( GetServerConnection() && (GetServerConnection()->RemoteAddress == Endpoint) )
			Connection = GetServerConnection();
		for( int32 i=0; i<ClientConnections.Num() && !Connection; i++ )
			if( ((UXC_TcpipConnection*)ClientConnections(i))->RemoteAddress == Endpoint )
				Connection = (UXC_TcpipConnection*)ClientConnections(i);

		if( Size == FSocket::Error )
		{
			if( Connection )
			{
				if( Connection != GetServerConnection() )
				{
					// We received an ICMP port unreachable from the client, meaning the client is no longer running the game
					// (or someone is trying to perform a DoS attack on the client)

					// rcg08182002 Some buggy firewalls get occasional ICMP port
					// unreachable messages from legitimate players. Still, this code
					// will drop them unceremoniously, so there's an option in the .INI
					// file for servers with such flakey connections to let these
					// players slide...which means if the client's game crashes, they
					// might get flooded to some degree with packets until they timeout.
					// Either way, this should close up the usual DoS attacks.
					if ((Connection->State != USOCK_Open) || (!AllowPlayerPortUnreach))
					{
						if ( LogPortUnreach )
							debugf( TEXT("Received ICMP port unreachable from client %s.  Disconnecting."), *Endpoint.String() );
						delete Connection;
					}
				}
			}
			else
			{
				if ( LogPortUnreach )
					debugf( TEXT("Received ICMP port unreachable from %s.  No matching connection found."), *Endpoint.String() );
			}
		}
		else
		{
			// If we didn't find a client connection, maybe create a new one.
			if( !Connection && Notify->NotifyAcceptingConnection()==ACCEPTC_Accept )
			{
				if ( ClientConnections.Num() >= ConnectionLimit )
				{
					//Run bulk disconnect on bad/empty connections
					guard( XC_IpDrv_DiscardConnections);
					for ( int32 i=0 ; i<ClientConnections.Num() ; i++ )
						if ( ClientConnections(i) && ClientConnections(i)->Channels[0] )
							delete ClientConnections(i--);
					unguard;
				}

				if ( ClientConnections.Num() < ConnectionLimit )
				{
					if ( UXC_TcpipConnection::StaticClass()->ClassUnique > (ClientConnections.Num() + ConnectionLimit) )
						UXC_TcpipConnection::StaticClass()->ClassUnique = 0;
					Connection = new UXC_TcpipConnection( Socket, this, Endpoint, USOCK_Open, 0, FURL() );
					Connection->URL.Host = Endpoint.Address.String();
					Notify->NotifyAcceptedConnection( Connection );
					ClientConnections.AddItem( Connection );
				}
			}

			// Send the packet to the connection for processing.
			if( Connection )
				Connection->ReceivedRawPacket( Data, Size );
		}
	}
}

FString UXC_TcpNetDriver::LowLevelGetNetworkNumber()
{
	return LocalAddress.Address.String();
}

void UXC_TcpNetDriver::LowLevelDestroy()
{
	// Close the socket.
	if ( !Socket.IsInvalid() )
	{
		if( !Socket.Close() )
			debugf( NAME_Exit, TEXT("WinSock closesocket error (%i)"), FSocket::ErrorCode() );
		debugf( NAME_Exit, TEXT("WinSock shut down") );
	}
}

// UXC_TcpNetDriver interface.
UBOOL UXC_TcpNetDriver::InitBase( UBOOL Connect, FNetworkNotify* InNotify, FURL& URL, FString& Error )
{
	static INT FirstInit = 1;
	if ( FirstInit ) //Running driver for the first time?
	{
		if ( InitialConnectTimeout == 0.f )
		{
			InitialConnectTimeout = 25.f;
			if ( !DownloadManagers.Num() )
			{
				DownloadManagers.AddZeroed(3);
				DownloadManagers(0) = TEXT("XC_IpDrv.XC_HTTPDownload");
				DownloadManagers(1) = TEXT("IpDrv.HTTPDownload");
				DownloadManagers(2) = TEXT("XC_Core.XC_ChannelDownload");
				DownloadManagers(3) = TEXT("Engine.ChannelDownload");
			}
			SaveConfig();
		}
	}
	// Init WSA.
	if( !Socket.Init( Error ) )
		return 0;

	// Create UDP socket and enable broadcasting.
	Socket = FSocket( false);
	if( Socket.IsInvalid() )
	{
		Socket = 0;
		Error = FString::Printf( TEXT("WinSock: socket failed (%i)"), FSocket::ErrorText() );
		return 0;
	}
	if ( !Socket.EnableBroadcast() )
	{
		Error = FString::Printf( TEXT("%s: setsockopt SO_BROADCAST failed (%i)"), FSocket::API, FSocket::ErrorText() );
		return 0;
	}

	Socket.SetReuseAddr();
	Socket.SetRecvErr();

    // Increase socket queue size, because we are polling rather than threading
	// and thus we rely on Windows Sockets to buffer a lot of data on the server.
	INT QueueSize = Connect ? 0x8000 : 0x25000; //was 0x20000
	Socket.SetQueueSize( QueueSize, QueueSize);

	// Bind socket to our port.
	LocalAddress.Address = GetLocalBindAddress(*GLog);
	UBOOL HardcodedPort = 0;
	if( !Connect )
	{
		// Init as a server.
		HardcodedPort = Parse( appCmdLine(), TEXT("PORT="), URL.Port );
		LocalAddress.Port = URL.Port;
	}
	int32 AttemptPort = LocalAddress.Port;
	int32 BoundPort = Socket.BindPort( LocalAddress, HardcodedPort ? 1 : 20);
	if ( BoundPort == 0 )
	{
		Error = FString::Printf( TEXT("%s: binding to port %i failed (%s)"), FSocket::API, AttemptPort, FSocket::ErrorText() );
		return 0;
	}
	if( !Socket.SetNonBlocking() )
	{
		Error = FString::Printf( TEXT("%s: SetNonBlocking failed (%s)"), FSocket::API, FSocket::ErrorText() );
		return 0;
	}

	// Success.
	return 1;
}

UXC_TcpipConnection* UXC_TcpNetDriver::GetServerConnection() 
{
	return (UXC_TcpipConnection*)ServerConnection;
}

void UXC_TcpNetDriver::StaticConstructor()
{
	new(GetClass(),TEXT("AllowPlayerPortUnreach"),	RF_Public)UBoolProperty (CPP_PROPERTY(AllowPlayerPortUnreach), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("LogPortUnreach"),			RF_Public)UBoolProperty (CPP_PROPERTY(LogPortUnreach        ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("ConnectionLimit"),			RF_Public)UIntProperty  (CPP_PROPERTY(ConnectionLimit       ), TEXT("Settings"), CPF_Config );
	new(GetClass(),TEXT("UseIPv6"),                 RF_Public)UBoolProperty (CPP_PROPERTY(UseIPv6               ), TEXT("Settings"), CPF_Config );

	
	UXC_TcpNetDriver* DefObject = GetDefault<UXC_TcpNetDriver>();
//	DefObject->InitialConnectTimeout = 15.0f;
	DefObject->ConnectionTimeout = 10.0f;
	DefObject->RelevantTimeout = 6.0f;
	DefObject->SpawnPrioritySeconds = 1.0f;
	DefObject->ServerTravelPause = 4.0f;
	DefObject->MaxClientRate = 25000;
	DefObject->NetServerMaxTickRate = 30;
	DefObject->LanServerMaxTickRate = 50;
	DefObject->AllowDownloads = 1;
	DefObject->RedirectRate = 50000;
	DefObject->RedirectPort = 7782;
	DefObject->ConnectionLimit = 128;
}

void UXC_TcpNetDriver::PostEditChange()
{
	guard(UXC_TcpNetDriver::PostEditChange);
	InitialConnectTimeout = Clamp( InitialConnectTimeout, 2.f, 200.f);
	ConnectionTimeout	= Clamp( ConnectionTimeout, 1.f, 200.f);
	RelevantTimeout		= Clamp( RelevantTimeout, 1.f, 200.f);
	ServerTravelPause	= Clamp( ServerTravelPause, 0.f, 60.f);
	MaxClientRate		= Clamp( MaxClientRate, 5000, 1000001); //1gbps
	NetServerMaxTickRate = Clamp( NetServerMaxTickRate, 0, 120);
	LanServerMaxTickRate = Clamp( LanServerMaxTickRate, 0, 120);
	RedirectPort = Clamp( RedirectPort, 1, 65535);
	RedirectRate = Clamp( RedirectRate, 5000, 5000000); //5gbps
	ConnectionLimit = Clamp( ConnectionLimit, 2, 1000); //Umm... lol

	Super::PostEditChange();
	SaveConfig();
	unguard;
}


IMPLEMENT_CLASS(UXC_TcpNetDriver);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

