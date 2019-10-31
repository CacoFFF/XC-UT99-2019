/*============================================================================
	IPv6.h
	Author: Fernando Velázquez

	IPv6/IPv4 address container.
============================================================================*/

#ifndef IPV6_H
#define IPV6_H

extern UBOOL GIPv6;

//Data in host byte order!
class FIPv6Address
{
	union
	{
		uint8  Bytes[16];
		uint16 Words[8];
		uint32 DWords[4]; //xmmword
		uint64 QWords[2];
	};

public:
	static const FIPv6Address Any; //::
	static const FIPv6Address InternalLoopback; //::1
	static const FIPv6Address InternalLoopback_v4; //::127.0.0.1
	static const FIPv6Address LanBroadcast; //FF02::1
	static const FIPv6Address LanBroadcast_v4; //255.255.255.255

public:
	FIPv6Address()
	{}

	//IPv4 mapped address constructor (::FFFF:A.B.C.D)
	FIPv6Address( uint8 A, uint8 B, uint8 C, uint8 D)
	{
		Bytes[0]=D;  Bytes[1]=C;  Bytes[2]=B;  Bytes[3]=A;
		Words[2] = 0xFFFF;  Words[3] = 0x0000;
		QWords[1] = 0x0000000000000000;
	}

	//IPv6 address constructor
	FIPv6Address( uint16 A, uint16 B, uint16 C, uint16 D, uint16 E, uint16 F, uint16 G, uint16 H)
	{
		Words[0]=H;  Words[1]=G;
		Words[2]=F;  Words[3]=E;
		Words[4]=D;  Words[5]=C;
		Words[6]=B;  Words[7]=A;
	}

#ifdef AF_INET
	FIPv6Address( in_addr addr )
	{
		DWords[0] = ntohl(addr.s_addr);
		Words[2] = 0xFFFF;  Words[3] = 0x0000;
		QWords[1] = 0x0000000000000000;
	}

	inline sockaddr_in ToSockAddr() const
	{
		sockaddr_in Result;
		Result.sin_family = AF_INET;
		Result.sin_port = 0;
		Result.sin_addr.s_addr = htonl(DWords[0]);
		*(uint64*)Result.sin_zero = 0;
		return Result;
	}
#endif
#ifdef AF_INET6
	FIPv6Address( in6_addr addr )
	{
		//Windows and linux use different union/variable names!!!
		for ( int32 i=0 ; i<16 ; i++ )
			Bytes[i] = addr.s6_addr[15-i];
	}

	inline sockaddr_in6 ToSockAddr6() const
	{
		sockaddr_in6 Result;
		Result.sin6_family = AF_INET6;
		Result.sin6_port = 0;
		//Windows and linux use different union/variable names!!!
		for ( int32 i=0 ; i<16 ; i++ )
			Result.sin6_addr.s6_addr[i] = Bytes[15-i];
		Result.sin6_scope_id = 0;
		return Result;
	}
#endif


	bool operator==( const FIPv6Address& Other ) const
	{
		return QWords[0] == Other.QWords[0]
			&& QWords[1] == Other.QWords[1];
	}

	bool operator!=( const FIPv6Address& Other ) const
	{
		return QWords[0] != Other.QWords[0]
			|| QWords[1] != Other.QWords[1];
	}

	bool IsIPv4() const
	{
		return Words[2]==0xFFFF && Words[3]==0x0000 && QWords[1]==0x0000000000000000;
	}

	friend FArchive& operator<<( FArchive& Ar, FIPv6Address& IpAddr )
	{
		return Ar << (INT&)IpAddr.DWords[0] << (INT&)IpAddr.DWords[1] << (INT&)IpAddr.DWords[2] << (INT&)IpAddr.DWords[3];
	}

	FString String() const;

public:
	//Ip in array order: 3.2.1.0
	uint8 GetByte( int32 Index ) const
	{
		check((Index >= 0) && (Index < 16));
		return Bytes[Index];
	}

	uint32 GetWord( int32 Index=0 ) const
	{
		check((Index >= 0) && (Index < 8));
		return Words[Index];
	}

	uint32 GetDWord( int32 Index=0 ) const
	{
		check((Index >= 0) && (Index < 4));
		return DWords[Index];
	}
};



//Address:Port
class FIPv6Endpoint
{
public:
	FIPv6Address Address;
	uint16 Port;

public:
	FIPv6Endpoint() {}
	FIPv6Endpoint( const FIPv6Address& InAddress, uint16 InPort )
		: Address(InAddress)
		, Port(InPort)
	{}

	bool operator==( const FIPv6Endpoint& Other ) const
	{
		return ((Address == Other.Address) && (Port == Other.Port));
	}

	bool operator!=( const FIPv6Endpoint& Other ) const
	{
		return ((Address != Other.Address) || (Port != Other.Port));
	}

	FString String( ) const;

	friend FArchive& operator<<( FArchive& Ar, FIPv6Endpoint& Endpoint )
	{
		return Ar << Endpoint.Address << Endpoint.Port;
	}

#ifdef AF_INET
	inline FIPv6Endpoint( sockaddr_in addr )
		: Address( addr.sin_addr)
	{
		Port = ntohs(addr.sin_port);
	}

	inline sockaddr_in ToSockAddr() const
	{
		sockaddr_in Result = Address.ToSockAddr();
		Result.sin_port = htons(Port);
		return Result;
	}
#endif
#ifdef AF_INET6
	inline FIPv6Endpoint( sockaddr_in6 addr )
		: Address( addr.sin6_addr)
	{
		Port = ntohs(addr.sin6_port);
	}

	inline sockaddr_in6 ToSockAddr6() const
	{
		sockaddr_in6 Result = Address.ToSockAddr6();
		Result.sin6_port = htons(Port);
		return Result;
	}
#endif

};



#endif