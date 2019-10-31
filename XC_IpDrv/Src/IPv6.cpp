/*=============================================================================
	IPv6.cpp
	Author: Fernando Velazquez

	IPv6/IPv4 address container implementation.
=============================================================================*/

#include "Core.h"
#include "Cacus/CacusPlatform.h"
#include "IPv6.h"

UBOOL GIPv6 = 0;

const FIPv6Address FIPv6Address::Any(0,0,0,0,0,0,0,0);
const FIPv6Address FIPv6Address::InternalLoopback(0,0,0,0,0,0,0,1);
const FIPv6Address FIPv6Address::InternalLoopback_v4(127,0,0,1);
const FIPv6Address FIPv6Address::LanBroadcast(0xFF02,0,0,0,0,0,0,1);
const FIPv6Address FIPv6Address::LanBroadcast_v4(224,0,0,1);


FString FIPv6Address::String() const
{
	//This is a IPv4 address
	if ( IsIPv4() )
		return FString::Printf( TEXT("%i.%i.%i.%i"), (int32)Bytes[3], (int32)Bytes[2], (int32)Bytes[1], (int32)Bytes[0] );


	int32 ZLow, ZHigh;
	for ( ZHigh=7    ; ZHigh>=0 && Words[ZHigh] ; ZHigh-- ); //Find first zero (left to right)
	for ( ZLow=ZHigh ; ZLow>0 && !Words[ZLow-1] ; ZLow-- ); //Find last zero after first

	int32 i = 7;
	FString Result;
	while ( i > ZHigh )
	{
		Result += FString::Printf( TEXT("%x"), (int32)Words[i]);
		if ( i > 0 )
			Result += TEXT(":");
		i--;
	}
	if ( (ZLow >= 0) && (i >= ZLow) )
	{
		if ( Result == TEXT("") )
			Result += TEXT("::");
		else
			Result += TEXT(":");
		i = ZLow - 1;
	}
	while ( i >= 0 )
	{
		Result += FString::Printf( TEXT("%x"), (int32)Words[i]);
		if ( i > 0 )
			Result += TEXT(":");
		i--;
	}
	return Result;
}

FString FIPv6Endpoint::String() const
{
	FString AddressString = Address.String();
	if ( AddressString.InStr( TEXT(":")) == INDEX_NONE )
		return FString::Printf( TEXT("%s:%i"), *AddressString, (int32)Port); //IPv4 address
	else
		return FString::Printf( TEXT("[%s]:%i"), *AddressString, (int32)Port); //IPv6 address
}
