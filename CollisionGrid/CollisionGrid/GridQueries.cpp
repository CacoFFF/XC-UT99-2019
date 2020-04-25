
#include "CollisionGrid.h"

//Constants:
cg::Vector Grid_Unit = cg::Vector( GRID_NODE_DIM, GRID_NODE_DIM, GRID_NODE_DIM, 0);
cg::Vector Grid_Mult = cg::Vector( GRID_MULT, GRID_MULT, GRID_MULT, 0);
cg::Integers XYZi_One = cg::Integers( 1, 1, 1, 0);

// Move somewhere, use?
/*
static uint32 RemoveBadResults( FCheckResult** Result)
{
	guard(RemoveBadResults);
	uint32 RemoveCount = 0;

	FCheckResult** FCR = Result;
	while ( *FCR )
	{
		bool bRemove = false;
		if ( cg::Vector( &(*FCR)->Location.X).InvalidBits() & 0b0111 ) //Something isn't valid
			bRemove = true;
		else
		{
			cg::Vector Normal( (*FCR)->Normal, E_Unsafe );
			if ( (Normal.InvalidBits() & 0b0111) || (Normal.SizeSq() > 2.f) )
				bRemove = true;
		}

		if ( bRemove )
		{
			UE_DEV_LOG( TEXT("[CG] Removing: %s L(%f,%f,%f) N(%f,%f,%f)"), (*FCR)->Actor->GetName(), (*FCR)->Location.X, (*FCR)->Location.Y, (*FCR)->Location.Z
																	, (*FCR)->Normal.X, (*FCR)->Normal.Y, (*FCR)->Normal.Z);
			FCheckResult* Next = (FCheckResult*) (*FCR)->Next;
			*FCR = Next;
			RemoveCount++;
		}
		else
			FCR = (FCheckResult**) &((*FCR)->Next);
	}
	return RemoveCount;
	unguard;
}*/


