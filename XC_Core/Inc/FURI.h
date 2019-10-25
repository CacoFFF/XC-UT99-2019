/*=============================================================================
	FURI.h

	Author: Fernando Velázquez
	//TODO: Update to RFC 3986
	// https://www.ietf.org/rfc/rfc3986.txt
=============================================================================*/

#ifndef USES_CACUS_URI
#define USES_CACUS_URI


/** RFC 2396 notes - https://www.ietf.org/rfc/rfc2396.txt
3.1 >> Schemes treat upper case letter as lower case
3.1 >> Relative URI's inherit scheme from base URI
3.2 >> Authority (auth@host:port) requires of "//" at start.
Cacus >> Interpreter can handle cases where leading "//" are missing
*/


class XC_CORE_API FURI
{
public:
	friend class PropertyURI;

	FURI();
	FURI( const TCHAR* Address);
	FURI( const FString* Address);
	FURI( const FURI& rhs);
	FURI( const FURI& BaseURI, const TCHAR* Address);
	FURI( FURI&& rhs); //Old C++ compilers don't support this
	~FURI();

	FURI& operator=( const FURI& Other) = default;
	UBOOL operator==( const FURI& rhs);
	UBOOL operator!=( const FURI& rhs);

	FString operator*() const; //Export URI to text

	FString Scheme;
	FString Auth;
	FString Hostname;
	INT     Port;
	FString Path;
	FString Query;
	FString Fragment;

	//Helpers, should be used instead of setting values directly
	void setScheme( const TCHAR* Val);
	void setPort( const TCHAR* Val);
	void setAuthority( const FString& Authority); //Auth, Hostname, Port

	static INT DefaultPort( const FString& Scheme);

	friend FArchive& operator<<( FArchive& Ar, FURI& U )
	{
		guard(FURI<<);
		Ar << U.Scheme<< U.Auth << U.Hostname << U.Port << U.Path << U.Query << U.Fragment;
		return Ar;
		unguard;
	}

protected:
	void Parse( const FString& Address);
};



inline FURI::FURI()
	: Port(0)
{
}

inline FURI::~FURI()
{
}


#endif