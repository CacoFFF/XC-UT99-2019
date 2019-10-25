
#include "XC_Core.h"
#include "FURI.h"

#include "Cacus/CacusString.h"

//Specialized lightweight helpers
template<class T> inline void ExchangeRaw( T& A, T& B )
{
	register INT Tmp;
	for ( INT i=0 ; i<sizeof(T)/sizeof(Tmp) ; i++ )
	{
		Tmp = ((INT*)&A)[i];
		((INT*)&A)[i] = ((INT*)&B)[i];
		((INT*)&B)[i] = Tmp;
	}
}

template < INT cmpsize > inline INT appStrncmp( const TCHAR* S1, const TCHAR(&S2)[cmpsize])
{
	return appStrncmp( S1, S2, cmpsize-1);
}

//========= FStringBetween - begin ==========//
//
// Creates a FString with the character between Start and EndNull
// EndNull will become the null terminator.
//
// This is a potentially unsafe operation and should be used with care.
//
static FString FStringBetween( const TCHAR* Start, const TCHAR* EndNull)
{
	if ( EndNull <= Start )
		return FString();

	TArray<TCHAR> Result( ((DWORD)EndNull - (DWORD)Start) / sizeof(TCHAR) + 1 );
	TCHAR* Copying = &Result(0);
	while ( Start < EndNull )
		*Copying++ = *Start++;
	*Copying = '\0';
	return *(FString*)&Result;
}
//========= FStringBetween - end ==========//



//*****************************************************
//*********************** URI *************************
//*****************************************************


/** Global parsing notes
For relative cases
- "//" forces authority parsing
- "/", "." force path parsing
- "?" forces query parsing
- "#" forces fragment parsing
*/


//========= ParseScheme - begin ==========//
//
// RFC 3986 Section 3.1 states that:
//   scheme = alpha *( alpha | digit | "+" | "-" | "." )
//
static FString ParseScheme( const TCHAR*& Pos)
{
	FString Scheme;
	const TCHAR* Cur = Pos;
	if ( CGetCharType(*Cur) & CHTYPE_Alpha )
	{
		while ( (CGetCharType(*Cur) & CHTYPE_AlNum) || appStrchr(TEXT("+-."),*Cur) )
			Cur++;
		if ( *Cur == ':' )
		{
			Scheme = FStringBetween( Pos, Cur);
			TransformLowerCase( (TCHAR*)*Scheme);
			Pos = Cur+1;
		}
	}
	return Scheme; //Failure
}
//========= ParseScheme - end ==========//


//========= ParseAuthority - begin ==========//
//
//    RFC 3986 Section 3.2 states that:
// The authority component is preceded by a double slash ("//") and is
// terminated by the next slash ("/"), question mark ("?"), or number
// sign ("#") character, or by the end of the URI.
//
static FString ParseAuthority( const TCHAR*& Pos)
{
	FString Authority;
	if ( !appStrncmp( Pos, TEXT("//")) )
	{
		const TCHAR* Cur = Pos+2;
		const TCHAR* Start = Cur;
		while ( *Cur && !appStrchr(TEXT("/?#"), *Cur) )
			Cur++;
		Authority = FStringBetween( Start, Cur);
		Pos = Cur;
	}
	return Authority;
}
//========= ParseAuthority - end ==========//


//========= ParsePath - begin ==========//
//
//    RFC 3986 Section 3.3 states that:
// The path component is terminated by the first question mark ("?")
// or number sign ("#") character, or by the end of the URI.
//
static FString ParsePath( const TCHAR*& Pos)
{
	const TCHAR* Start = Pos;
	const TCHAR* End = Start;
	AdvanceTo( End, TEXT("?#")); //CACUSLIB
	Pos = End;
	return FStringBetween( Start, End);
}
//========= ParsePath - end ==========//


//========= ParseQuery - begin ==========//
//
//    RFC 3986 Section 3.4 states that:
// The query component is indicated by the first question mark ("?")
// character and terminated by a number sign ("#") character or by
// the end of the URI.
//
static FString ParseQuery( const TCHAR*& Pos, UBOOL& bDiscardBaseQuery)
{
	FString Query;
	if ( *Pos == '?' ) //Is a query string
	{
		bDiscardBaseQuery = 1;
		const TCHAR* Start = Pos+1;
		const TCHAR* End = Start;
		AdvanceTo( End, (TCHAR)'#'); //CACUSLIB
		Query = FStringBetween( Start, End);
		Pos = End;
	}
	return Query;
}
//========= ParseQuery - end ==========//


//========= ParseFragment - begin ==========//
//
//    RFC 3986 Section 3.5 states that:
// The fragment identifier component is indicated by the presence of
// a number sign ("#") character and terminated by the end of the URI
//
static FString ParseFragment( const TCHAR*& Pos, UBOOL& bDiscardBaseFragment)
{
	FString Fragment;
	if ( *Pos == '#' ) //Is a fragment identifier
	{
		bDiscardBaseFragment = 1;
		const TCHAR* Start = Pos+1;
		const TCHAR* End = Start;
		while ( *End ) End++;
		Fragment = FStringBetween( Start, End);
		Pos = End;
	}
	return Fragment;
}
//========= ParseFragment - end ==========//



//========= RemoveDotSegments - begin ==========//
//
//    RFC 3986 Section 5.2.4
//
static void RemoveLastSegment( FString& Output)
{
	INT OutLen = Output.Len();
	if ( OutLen <= 2 )
		Output = FString();
	else
	{
		INT Separator = Output.InStr( TEXT("/"), 1);
		Output = Output.Left( Max(Separator,0) );
	}
}

static void RemoveDotSegments( FString& Path)
{
	if ( !Path.Len() )
		return;

	FString Buffer = Path;
	TCHAR* Input = (TCHAR*)*Buffer;
	FString Output;

	while ( Input && *Input )
	{
		if      ( !appStrncmp(Input,TEXT("./")) )
			Input += _len("./"); //Remove
		else if ( !appStrncmp(Input,TEXT("../")) )
			Input += _len("../"); //Remove
		else if ( !appStrncmp (Input,TEXT("/.")) )
			Input[1] = '\0'; //Remove dot
		else if ( !appStrncmp(Input,TEXT("/./")) )
			Input += _len("/."); //Remove one slash and dot
		else if ( !appStrncmp( Input,TEXT("/..")) )
		{
			Input[1] = '\0'; //Remove dots, leave /
			RemoveLastSegment( Output);
		}
		else if ( !appStrncmp(Input,TEXT("/../")) )
		{
			Input += _len("/.."); //Remove one slash and two dots
			RemoveLastSegment( Output);
		}
		else if ( !appStrncmp(Input,TEXT(".")) || !appStrncmp(Input,TEXT("..")) )
			Input[0] = '\0'; //Clear
		else
		{
			TCHAR* Slash = appStrchr( Input+1, '/');
			if ( Slash )
				Output += FStringBetween( Input, Slash);
			else
				Output += Input;
			Input = Slash;
		}
	}
	ExchangeRaw( Path, Output); //Let original Path be deleted via out-of scope (move semantics)
}
//========= RemoveDotSegments - begin ==========//




FURI::FURI( const TCHAR* InURI)
	: FURI()
{
	Parse( InURI);
}

FURI::FURI(const FString * Address)
	: FURI()
{
	Parse( *Address);
}

FURI::FURI( const FURI& rhs)
	: Scheme(rhs.Scheme)
	, Auth(rhs.Auth)
	, Hostname(rhs.Hostname)
	, Port(rhs.Port)
	, Path(rhs.Path)
	, Query(rhs.Query)
	, Fragment(rhs.Fragment)
{
}


FURI::FURI( const FURI& rhs, const TCHAR* Address)
	: FURI(rhs)
{
	if ( Address )
		Parse( Address);
}

FURI::FURI( FURI&& rhs)
	: FURI()
{
	ExchangeRaw( *this, rhs);
}


UBOOL FURI::operator==( const FURI& rhs)
{
	return Scheme == rhs.Scheme
		&& !appStrcmp( *Auth, *rhs.Auth)
		&& Hostname == rhs.Hostname
		&& Port == rhs.Port
		&& Path == rhs.Path
		&& Query == rhs.Query
		&& Fragment == rhs.Fragment;
}

UBOOL FURI::operator!=( const FURI& rhs)
{
	return !((*this) == rhs);
}

//========= URI::operator* - begin ==========//
//
// Exports URI as plain text contained in the string buffer 
//
FString FURI::operator*() const //TODO: NOT TESTED
{
	FString Result;

	if ( **Scheme )
	{
		Result += Scheme;
		Result += TEXT(":");
	}

	if ( **Auth || **Hostname )
		Result += TEXT("//");

	if ( **Auth )
	{
		Result += Auth;
		Result += TEXT("@");
	}

	if ( **Hostname )
		Result += Hostname;

	if ( Port )
		Result += FString::Printf( TEXT(":%i"), Port);

	if ( **Path )
	{
		if ( **Path != '/' )
			Result += TEXT("/");
		Result += Path;
	}

	if ( **Query )
	{
		Result += TEXT("?");
		Result += Query;
	}

	if ( Fragment )
	{
		Result += TEXT("#");
		Result += Fragment;
	}

	return Result;
}
//========= URI::operator* - end ==========//


void FURI::setScheme( const TCHAR* Val)
{
	Scheme = Val;
	TransformLowerCase( (TCHAR*)*Scheme); //CACUSLIB
}

void FURI::setPort( const TCHAR* Val)
{
	int NewVal = appAtoi( Val);
	if ( NewVal & 0xFFFF0000 )
		Port = 0;
	else
		Port = NewVal;
}



//========= URI::setAuthority - begin ==========//
//
// Splits the Authority component into the sub components
// and assigns them to the URI.
//
void FURI::setAuthority( const FString& Authority)
{
	Auth = FString();
	Hostname = FString();
	Port = 0;
	const TCHAR* Start = *Authority;

	TCHAR* Separator = appStrchr( Start, '@');
	if ( Separator ) //UserInfo
	{
		Auth = FStringBetween( Start, Separator);
		Start = Separator + 1;
	}

	//IPv6 enclosed by square brackets
	if ( *Start == '[' )
	{
		Separator = appStrchr( Start+1, ']');
		if ( Separator )
		{
			Hostname += FStringBetween( Start, Separator+1);
			Start = Separator + 1;
		}
	}

	Separator = appStrchr( Start, ':'); //Port separator
	if ( !Separator || appStrchr( Separator+1, ':') ) //Hack: IPv6 without brackets when more than one : (doesn't parse port)
		Hostname += Start;
	else
	{
		Hostname += FStringBetween( Start, Separator);
		setPort( Separator + 1);
	}
}
//========= URI::setAuthority - end ==========//



struct port_entry
{
	INT port;
	const TCHAR* name;
	port_entry( INT in_port, const TCHAR* in_name)
		: port(in_port), name(in_name) {}
};

//========= URI::DefaultPort - begin ==========//
//
// Returns a known default port for Scheme
//
INT FURI::DefaultPort( const FString& Scheme)
{
	static port_entry entries[] = 
	{
		port_entry(   21, TEXT("ftp")),
		port_entry(   22, TEXT("ssh")),
		port_entry(   23, TEXT("telnet")),
		port_entry(   80, TEXT("http")),
		port_entry(  119, TEXT("nntp")),
		port_entry(  389, TEXT("ldap")),
		port_entry(  443, TEXT("https")),
		port_entry(  554, TEXT("rtsp")),
		port_entry(  636, TEXT("ldaps")),
		port_entry( 5060, TEXT("sip")),
		port_entry( 5061, TEXT("sips")),
		port_entry( 5222, TEXT("xmpp")),
		port_entry( 7777, TEXT("unreal")),
		port_entry( 9418, TEXT("git")),
	};

	if ( Scheme.Len() < 8 ) //Our largest entry is 7 chars long, this is a quick filter
	{
		for ( INT i=0 ; i<ARRAY_COUNT(entries) ; i++ )
			if ( !appStricmp( *Scheme, entries[i].name) )
				return entries[i].port;
	}
	return 0;
}
//========= URI::DefaultPort - end ==========//


//========= URI::Parse - begin ==========//
//
// Imports a plain text URI into this object
//
void FURI::Parse( const FString& Address)
{
	guard(FURI::Parse)
	// Move the contents of this URI into 'Base'
	// 'This' will temporarily become the 'Reference URI'
	// Until it's time to become the 'Transform URI' (result of parsing)
	// The Base URI will be deleted at end of function, so we can safely move elements to Reference using 'ExchangeRaw'
	debugf( NAME_Log, TEXT("== Parsing URI: %s"), *Address);
	FURI Base;
	ExchangeRaw( *this, Base);

	// Skip spaces
	const TCHAR* Pos = *Address;
	while ( CGetCharType(*Pos) & CHTYPE_TokSp ) Pos++;

	// The reference URI is blank, safe to assign values without worrying about old data
	FString Authority;
	UBOOL bDiscardBaseQuery = 0; //If we get an empty query string '?', we need to discard Base's query string
	UBOOL bDiscardBaseFragment = 0; //If we get an empty fragment string '#', we need to discard Base's fragment string

	Scheme = ParseScheme(Pos);
	Authority = ParseAuthority(Pos);
	Path = ParsePath(Pos);
	Query = ParseQuery(Pos, bDiscardBaseQuery);
	Fragment = ParseFragment(Pos, bDiscardBaseFragment);

	// TODO: https://tools.ietf.org/html/rfc3986#section-5.2.1  Pre-parse the Base URI

	// RFC 3986 section 5.2.2:  Transform References
	// Here 'Reference' (this) becomes 'Transform', make necessary adjustments
	if ( **Scheme != '\0' )
	{
		setAuthority( Authority);
		RemoveDotSegments( Path);
	}
	else
	{
		if ( **Authority != '\0' )
		{
			setAuthority( Authority);
			RemoveDotSegments( Path);
		}
		else
		{
			if ( **Path == '\0' )
			{
				ExchangeRaw( Path, Base.Path);
				if ( **Query == '\0' )
					ExchangeRaw( Query, Base.Query);
			}
			else
			{
				if ( **Path == '/' )
					RemoveDotSegments( Path);
				else
				{
					Path = Base.Path + Path;
					RemoveDotSegments( Path);
				}
			}
			ExchangeRaw( Auth, Base.Auth);
			ExchangeRaw( Hostname, Base.Hostname);
			Port = Base.Port;
		}
		ExchangeRaw( Scheme, Base.Scheme);
	}
	//Base and it's remaining elements is destructed via out-of-scope
	unguard;
}
//========= URI::Parse - end ==========//



//*****************************************************
//************* PROPERTY IMPLEMENTATION ***************
//*****************************************************
