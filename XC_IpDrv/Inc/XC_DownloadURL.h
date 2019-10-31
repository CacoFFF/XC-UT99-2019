/*=============================================================================
	UnDownloadURL.cpp
	Author: Fernando Velázquez

	Specialized URL type designed for better HTTP handling
=============================================================================*/

#ifndef INC_DOWNLOADURL_H
#define INC_DOWNLOADURL_H

#include "FURI.h"

#define NO_COMPRESSION 0
#define UZ_COMPRESSION 1
#define LZMA_COMPRESSION 2


class FDownloadURL : public FURI
{
public:
	UBOOL bIsValid;
	int32 Compression;
	FString RequestedPackage;
	FString ProxyHostname;
	int32 ProxyPort;

public:
	FDownloadURL();
	FDownloadURL( const TCHAR* FromText, const TCHAR* PackageFileName);
	FDownloadURL( const FDownloadURL& Other) = default;
	FDownloadURL( FDownloadURL&& Other);

	FDownloadURL& operator=( const FDownloadURL& Other) = default;

	int32 GetPort();
	FString String() const;
	FString StringGet() const; //Must contain trailing '/'
	FString StringHost( UBOOL bNoPort=0 ) const;

	static const TCHAR* GetCompressedExt( int32 Compression);

	friend FArchive& operator<<( FArchive& Ar, FDownloadURL& U )
	{
		guard(FDownloadURL<<);
		Ar << (FURI&)U << U.bIsValid << U.Compression << U.RequestedPackage;
		return Ar;
		unguard;
	}
};

#endif

