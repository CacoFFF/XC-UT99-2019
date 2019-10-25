/*=============================================================================
	Devices.cpp:

	Multiplatform devices implementation
	- Output devices
	- Memory allocators
=============================================================================*/

#include "XC_Core.h"

#include "Devices.h"
#include "Cacus/Atomics.h"


//*************************************************
// Thread-safe Log proxy
//*************************************************

bool FLogLine::operator==( const FLogLine& O)
{
	return (O.Event == Event)
		&& (O.Msg.Len() == Msg.Len())
		&& !appStrcmp(*O.Msg,*Msg);
}

FLogLine::FLogLine()
	: Event(NAME_Log)
{}

FLogLine::FLogLine( EName InEvent, const TCHAR* InData)
	: Event( InEvent)
	, Msg( InData)
{
}

FOutputDeviceInterceptor::FOutputDeviceInterceptor( FOutputDevice* InNext)
	:	Next( InNext )
	,	CriticalOut(NULL)
	,	ProcessLock(0)
	,	SerializeLock(0)
	,	Repeater(NAME_Log)
	,	MultiLineCount(0)
	,	MultiLineCur(0)
{
}

FOutputDeviceInterceptor::~FOutputDeviceInterceptor()
{
	if ( CriticalOut )
	{
		delete CriticalOut;
		CriticalOut = nullptr;
	}
	if ( Next )
		delete Next;
	ProcessLock = 0;
}

void FOutputDeviceInterceptor::SetRepeaterText( const TCHAR* Text)
{
	FName NewRepeater( Text, FNAME_Intrinsic);
	Repeater = (EName)NewRepeater.GetIndex();
}

void FOutputDeviceInterceptor::Serialize( const TCHAR* Msg, EName Event )
{
	guard(FOutputDeviceInterceptor::Serialize)
	if ( ProcessLock ) //Fugly hack to prevent deadlock
		return;
	if ( Msg && Msg[0] ) //No empty output or infinite loop
	{
		FLogLine Line( Event, Msg);
		ProcessMessage( Line);
	}
	unguard;
}

void FOutputDeviceInterceptor::ProcessMessage( FLogLine& Line)
{
	CSpinLock Lock(&ProcessLock);
	UBOOL bDoLog = true;

	//Flush all saved lines if we're printing a crash log
	if ( GIsCriticalError )
		FlushRepeater();

	if ( Line.Event == NAME_Critical )
	{
		if ( !CriticalOut )
		{
			INT Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec;
			appSystemTime( Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec );
			CriticalOut = new FOutputDeviceFile();
			appSprintf( CriticalOut->Filename , TEXT("Crash__%i-%02d-%02d__%02d-%02d.log"), Year, Month, Day, Hour, Min);
		}
		CriticalOut->Serialize(  *Line.Msg, NAME_Critical );
		CriticalOut->Flush();
	}
	else if ( Line.Event == NAME_Title )
	{
		bDoLog = false;
	}
	else
	{
		//Attempt to setup repeater
		if ( !MultiLineCount && !GIsCriticalError )
		{
			for ( INT i=0 ; i<MessageHistory.Num() && bDoLog ; i++ )
				if ( MessageHistory(i) == Line ) //Found a match!
				{
					RepeatCount = 1;
					MultiLineCur = MultiLineCount = (DWORD)i + 1;
					bDoLog = false;
				}
		}
		
		//Repeater exists
		if ( MultiLineCount )
		{
			if ( !bDoLog || MessageHistory(MultiLineCur-1) == Line ) //Matches
			{
				bDoLog = false;
				if ( --MultiLineCur == 0 ) //All lines match, increment repeater
				{
					RepeatCount++;
					MultiLineCur = MultiLineCount;
				}
			}
			else
				FlushRepeater();
		}
	}

	if ( bDoLog )
	{
		SerializeNext( *Line.Msg, Line.Event);
		MessageHistory.InsertZeroed( 0);
		appMemswap( &MessageHistory(0), &Line, sizeof(FLogLine)); //Destroy old message and keep 'line' in buffer (NO REALLOCS!)
	}

	if ( MessageHistory.Num() > OLD_LINES )
		MessageHistory.Remove( OLD_LINES, MessageHistory.Num()-OLD_LINES);
}

void FOutputDeviceInterceptor::FlushRepeater()
{
	if ( RepeatCount > 1 )
	{
		TCHAR Ch[128];
		if ( MultiLineCount <= 1 ) //One line
			appSprintf( Ch, TEXT("=== Last line repeats %i times."), RepeatCount);
		else //More than one line
			appSprintf( Ch, TEXT("=== Last %i lines repeat %i times."), MultiLineCount, RepeatCount);
		SerializeNext( Ch, Repeater);
	}

	//Messages that were held back must be added to history and printed
	TArray<FLogLine> HistoryAdd;
	INT Add = (INT)(MultiLineCount - MultiLineCur);
	if ( Add > 0 )
	{
		HistoryAdd.AddZeroed( Add);
		while ( --Add >= 0 )
		{
			HistoryAdd(Add) = MessageHistory( (INT)MultiLineCur + Add);
			SerializeNext( *HistoryAdd(Add).Msg, HistoryAdd(Add).Event);
		}
	}

	if ( RepeatCount > 1 )
		MessageHistory.Empty();

	if ( HistoryAdd.Num() )
	{
		MessageHistory.InsertZeroed( 0, HistoryAdd.Num());
		appMemswap( &MessageHistory(0), &HistoryAdd(0), sizeof(FLogLine) * HistoryAdd.Num() );
	}

	RepeatCount = 0;
	MultiLineCount = 0;
	MultiLineCur = 0;
}


void FOutputDeviceInterceptor::SerializeNext( const TCHAR* Text, EName Event)
{
	CSpinLock Lock(&SerializeLock);
	Next->Serialize( Text, Event);
}