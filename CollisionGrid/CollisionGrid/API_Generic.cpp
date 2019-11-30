
#include "API.h"
#include "GridMath.h"


static char ExceptionMarker[] = "EXCEPT_0_000000000";
static uint32 DebugDepth = 0;

DebugToken::DebugToken( char C)
{
	DebugDepth++;
	ExceptionMarker[7] = '0' + DebugDepth;
	ExceptionMarker[8+DebugDepth] = C;
}

DebugToken::~DebugToken()
{
	ExceptionMarker[8+DebugDepth] = '0';
	DebugDepth--;
	ExceptionMarker[7] = '0' + DebugDepth;
}

DebugLock::DebugLock( const char* Keyword, char Lock)
{
	while ( Keyword[0] == Lock )	{}
}


#define BUFFER_LINES 16
#define BUFFER_LINE_SIZE 512
static TCHAR TextBuffer[BUFFER_LINES][BUFFER_LINE_SIZE];
static uint32 CurLine = 0;

#if UNICODE
static char AnsiBuffer[BUFFER_LINES][BUFFER_LINE_SIZE];
static uint32 CurAnsi = 0;
#endif

const TCHAR* cg::Vector::String() const
{
	PlainText Tmp = PlainText(TEXT("(")) + X + TEXT(",") + Y + TEXT(",") + Z + TEXT(",") + W + TEXT(")");
//	CurLine--; //Make this line reusable for next buffer
	return *Tmp;
}

const TCHAR* cg::Integers::String() const
{
	PlainText Tmp = PlainText(TEXT("[")) + i + TEXT(",") + j + TEXT(",") + k + TEXT(",") + l + TEXT("]");
//	CurLine--; //Make this line reusable for next buffer
	return *Tmp;
}



//
// Simple text parser and concatenator
//
PlainText::PlainText()
{
	TAddr = TextBuffer[(CurLine++) & (BUFFER_LINES-1)];
	TAddr[Size=0] = 0;
}

PlainText::PlainText( const TCHAR *T)
{
	TAddr = TextBuffer[(CurLine++) & (BUFFER_LINES-1)];
	for ( Size=0 ; Size<BUFFER_LINE_SIZE-1 && T[Size] ; Size++ )
		TAddr[Size] = T[Size];
	TAddr[Size] = 0;
}

PlainText PlainText::operator+(const TCHAR * T)
{
	for ( uint32 i=0 ; Size<BUFFER_LINE_SIZE-1 && T[i] ; i++, Size++ )
		TAddr[Size] = T[i];
	TAddr[Size] = 0;
	return *this;
}

PlainText PlainText::operator+(UObject* O)
{
	const TCHAR *T = O->GetName();
	return (*this) + T;
}

PlainText PlainText::operator+(int32 N)
{
	static TCHAR Buf[32];
#if __GNUC__
	sprintf(Buf,"%i",N);
#elif UNICODE
	_itow( N, Buf, 10);
#else
	_itoa( N, Buf, 10);
#endif
	return (*this) + Buf;
}

PlainText PlainText::operator+(uint32 N)
{	return (*this) + (int32)N;	}

PlainText PlainText::operator+(float F)
{	return (*this) + (int32)F;	}

PlainText PlainText::operator+(const FVector& V)
{	return (*this) /*+ V.String()*/;	}

PlainText PlainText::operator+ (const cg::Vector& V)
{	return (*this) + V.String();	}

PlainText PlainText::operator+ (const cg::Integers& V)
{	return (*this) + V.String();	}

PlainText PlainText::operator+ (const void* Ptr)
{	return (*this) + (uint32)Ptr;	}

const TCHAR* PlainText::operator*()
{
	return TAddr;
}

void PlainText::Reset()
{
	TAddr[Size=0] = 0;
}

#if UNICODE
char* PlainText::Ansi()
{
	char* AnsiAddr = AnsiBuffer[(CurAnsi++) & (BUFFER_LINES-1)];
	for ( uint32 i=0 ; i<=Size ; i++ )
		AnsiAddr[i] = TAddr[i];
	return AnsiAddr;
}
#endif


void* operator new( size_t Size, ESize Units, uint32 RealSize)
{
	return appMalloc( RealSize << Units, TEXT(""));
}

void* operator new( size_t Size, EAlign Tag )
{
	return appMallocAligned( Size, Tag);
}

void* operator new( size_t Size, EAlign Tag, ESize Units, uint32 RealSize)
{
	return appMallocAligned( RealSize << Units, Tag);
}

void* appMallocAligned( uint32 Size, uint32 Align)
{
	//Align must be power of 2, I won't do exception checking here
	void* BasePtr = appMalloc( Size + Align, TEXT(""));
	if ( !BasePtr )
	{
		GError->Log( TEXT("[CG] Failed to allocate aligned memory") );
		return nullptr;
	}

	void* Result = (void*) (((INT)BasePtr+(Align))&~(Align-1)); //If pointer is already aligned, still advance 'align' bytes
																//Store BasePtr before result for 'free'
	((void**)Result)[-1] = BasePtr;
	return Result;
}


void* appFreeAligned( void* Ptr)
{
	if ( Ptr )
		appFree( ((void**)Ptr)[-1] );
	return nullptr;
}

