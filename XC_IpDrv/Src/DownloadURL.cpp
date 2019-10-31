/*=============================================================================
	UnDownloadURL.cpp
	Author: Fernando Velázquez

	Definitions of this specialized URL type.
=============================================================================*/

#include "XC_IpDrv.h"

static const TCHAR* CompressionArray[3] = { TEXT(""), TEXT(".uz"), TEXT(".lzma") };

FDownloadURL::FDownloadURL()
	: FURI( TEXT("http://127.0.0.1/"))
	, bIsValid(1)
	, Compression(0)
	, RequestedPackage()
{}

FDownloadURL::FDownloadURL( const TCHAR* FromText, const TCHAR* PackageFileName)
	: FURI( FromText)
	, Compression(0)
	, RequestedPackage(PackageFileName)
{
	if ( !Scheme.Len() ) //No scheme was introduced! Reparse as HTTP
	{
		setScheme( TEXT("http"));
		Port = DefaultPort( Scheme );
		if ( !Hostname.Len() ) //Fix hostname after re-setting scheme
			Parse( **this);
	}
	bIsValid = Hostname.Len() && Path.Len();
}

FDownloadURL::FDownloadURL( FDownloadURL&& Other)
{
	appMemcpy( this, &Other, sizeof(FDownloadURL) );
	appMemzero( &Other, sizeof(FDownloadURL) );
}

int32 FDownloadURL::GetPort()
{
	int32 Result = (ProxyHostname.Len() > 0) ? ProxyPort : Port;
	if ( Result <= 0 || Result > 65535 )
		return DefaultPort( Scheme);
	return Result;
}

FString FDownloadURL::String() const
{
	FString Result = **this;
	if ( !Query.Len() && Result.Right(1) != TEXT("/") )
		Result += TEXT("/");
	Result += RequestedPackage;
	Result += GetCompressedExt( Compression);
	return Result;
}

FString FDownloadURL::StringGet() const
{
	if ( Auth.Len() || ProxyHostname.Len() )
		return String();
	FString Result = Path;
	Result += RequestedPackage;
	Result += GetCompressedExt( Compression);
	return Result;
}

FString FDownloadURL::StringHost( UBOOL bNoPort ) const
{
	bool bUseProxy = ProxyHostname.Len() > 0;
	FString Result = bUseProxy ? ProxyHostname : Hostname;
	int32 ExtraPort = bUseProxy ? ProxyPort : Port;

	if ( (ExtraPort != DefaultPort(Scheme)) && !bNoPort )
		Result += FString::Printf( TEXT(":%i"), Port);

	return Result;
}

const TCHAR* FDownloadURL::GetCompressedExt(int32 Compression)
{
	if ( Compression < 0 || Compression >= ARRAY_COUNT(CompressionArray) )
		Compression = 0;
	return CompressionArray[Compression];
}
