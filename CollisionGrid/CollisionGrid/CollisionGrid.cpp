//
// CollisionGrid.cpp
// Bridge between XC_Engine and CollisionGrid
// 
//

#include "CollisionGrid.h"

constexpr int AISize = sizeof(class ActorInfoHolder);

namespace cg
{
	const Integers Vector::MASK_3D  ( 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000);
	const Integers Vector::MASK_SIGN( 0x80000000, 0x80000000, 0x80000000, 0x80000000);
	const Integers Vector::MASK_ABS ( 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF);
};

//
// XC_Engine's first interaction
//
extern "C"
{
	TEST_EXPORT FCollisionHashBase* GNewCollisionHash( ULevel* Level)
	{
		//Unreal Engine destroys this object
		//Therefore use Unreal Engine allocator
		return new(TEXT("FCollisionGrid")) FCollisionGrid( Level);
	}

}

//const TEST_EXPORT __m128i TEST_M128i = _mm_set_epi32(1,2,3,4);


/*-----------------------------------------------------------------------
                    FCollisionHashBase interface
-----------------------------------------------------------------------*/

#define QUERY_PARAMETERS this, Mem, ActorQueryFlags, ExtraNodeFlags

void FCollisionGrid::Tick()
{
	Grid->Tick();
}

void FCollisionGrid::AddActor( AActor* Actor)
{
	guard(FCollisionGrid::AddActor);
	check(Actor);

	// Validate actor flags.
	if ( !Actor->bCollideActors || Actor->bDeleteMe ) 
		return;

	// Anomaly: remove existing actor info.
	if ( Actor->CollisionTag != 0 )
		RemoveActor(Actor);

	// Anomaly: actor with invalid position.
	if ( !FixActorLocation(Actor) )
		return;


	ActorInfo* AInfo = AIH->Acquire(Actor);
	if ( AInfo )
	{
		if ( !Grid->InsertActorInfo(AInfo) )
		{
			AIH->Release(AInfo);
			Actor->CollisionTag = 0;
		}
		else
			Actor->ColLocation = Actor->Location;
	}
	unguard;
}

void FCollisionGrid::RemoveActor( AActor *Actor)
{
	guard(FCollisionGrid::RemoveActor);
	check(Actor);

	// Not in grid (hopefully)
	if ( Actor->CollisionTag == 0 )
		return;

	ActorInfo* AInfo = AIH->GetIndexedElement(Actor->CollisionTag);
	if ( AInfo && (AInfo->Actor == Actor) && AInfo->Flags.bCommited )
	{
		Grid->RemoveActorInfo(AInfo);
		AIH->Release(AInfo);
	}
	Actor->CollisionTag = 0;

	unguard;
}

FCheckResult* FCollisionGrid::ActorLineCheck(FMemStack& Mem, FVector End, FVector Start, FVector Extent, uint32 ActorQueryFlags, uint8 ExtraNodeFlags)
{
	guard(FCollisionGrid::ActorLineCheck);
	if ( FDistSquared(End,Start) > SMALL_NUMBER )
	{
		cg::Query::Line Helper( QUERY_PARAMETERS, End, Start, Extent);
		Helper.Query();
		return Helper.ResultList;
	}
	return nullptr;
	unguard;
}

FCheckResult* FCollisionGrid::ActorPointCheck(FMemStack& Mem, FVector Location, FVector Extent, uint32 ActorQueryFlags, uint32 ExtraNodeFlags)
{
	guard(FCollisionGrid::ActorPointCheck);
	cg::Query::Point Helper( QUERY_PARAMETERS, Location, Extent);
	Helper.Query();
	return Helper.ResultList;
	unguard;
}

FCheckResult* FCollisionGrid::ActorRadiusCheck(FMemStack& Mem, FVector Location, float Radius, uint32 ActorQueryFlags, int32 ExtraNodeFlags)
{
	guard(FCollisionGrid::ActorRadiusCheck);
	if ( Radius >= 0 )
	{
		cg::Query::Radius Helper( QUERY_PARAMETERS, Location, Radius);
		Helper.Query();
		return Helper.ResultList;
	}
	return nullptr;
	unguard;
}

FCheckResult* FCollisionGrid::ActorEncroachmentCheck(FMemStack& Mem, AActor* Actor, FVector Location, FRotator Rotation, uint32 ActorQueryFlags, uint32 ExtraNodeFlags)
{
	guard(FCollisionGrid::ActorEncroachmentCheck);
	check(Actor);
	Exchange( Actor->Location, Location);
	Exchange( Actor->Rotation, Rotation);
	cg::Query::Encroachment Helper( QUERY_PARAMETERS, Actor);
	Helper.Query();
	Exchange( Actor->Location, Location);
	Exchange( Actor->Rotation, Rotation);
	return Helper.ResultList;
	unguard;
}


/*-----------------------------------------------------------------------
                      FCollisionGrid interface.
-----------------------------------------------------------------------*/

FCollisionGrid::FCollisionGrid( ULevel* Level)
{
	guard(FCollisionGrid::FCollisionGrid);

	int32 GlobalIndex;
	Grid = new ::Grid( Level);
	AIH = new ActorInfoHolder();
	AIH->ActorInfoHolder::TElementHolder::Acquire(GlobalIndex); //ActorInfo 0 is not used.
	GLog->Log( TEXT("[CG] Element holders succesfully spawned.") );
	CollisionTag = 0;

	unguard;
}

FCollisionGrid::~FCollisionGrid()
{
	guard(FCollisionGrid::~FCollisionGrid);
	check(AIH);
	check(Grid);
	
//	DebugLock( "DeleteGrid", 'D');
	delete AIH;
	delete Grid;

	unguard;
};


inline bool FCollisionGrid::FixActorLocation( AActor* Actor)
{
	cg::Vector Location( &Actor->Location.X);
	if ( Location.InvalidBits() & 0x0111 ) //Validate location
	{
#if GRID_DEVELOPMENT
		debugf( TEXT("[CG] Invalid actor location: %s [%f,%f,%f]"), Actor->GetName(), Actor->Location.X, Actor->Location.Y, Actor->Location.Z );
#endif
		cg::Vector NewLoc( &Actor->ColLocation.X);
		if ( NewLoc.InvalidBits() & 0x0111 ) //EXPERIMENTAL, RELOCATE ACTOR
			return false;
		Actor->Location = Actor->ColLocation;
#if GRID_DEVELOPMENT
		debugf( TEXT("[CG] Relocating to [%f,%f,%f]"), Actor->Location.X, Actor->Location.Y, Actor->Location.Z);
#endif
	}
	return true;
}