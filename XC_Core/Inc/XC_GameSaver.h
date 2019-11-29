/*=============================================================================
	XC_GameSaver.h

	Author: Fernando Velázquez
=============================================================================*/

#ifndef _INC_XC_GAMESAVER
#define _INC_XC_GAMESAVER

struct FSaveSummary;

namespace XC
{
	enum ESaveFlags
	{
		SAVE_Full = 0x00,
		SAVE_NoGame = 0x01,
	};

	XC_CORE_API void GetSaveGameList( TArray<FSaveSummary>& SaveList);
	XC_CORE_API UBOOL LoadSaveGameSummary( FSaveSummary& SaveSummary, const TCHAR* FileName);
	XC_CORE_API UBOOL SaveGame( ULevel* Level, const TCHAR* FileName, uint32 SaveFlags=0);
	XC_CORE_API UBOOL LoadGame( ULevel* Level, const TCHAR* FileName); //To be used during LoadMap, before level initialization.
}



struct FSaveSummary
{
	FString Players;
	FString LevelTitle;
	FString Notes;
	FURL    URL;
	FGuid   GUID;

	UBOOL IsValid();

	friend FArchive& operator<<( FArchive& Ar, FSaveSummary& S);
};








#endif