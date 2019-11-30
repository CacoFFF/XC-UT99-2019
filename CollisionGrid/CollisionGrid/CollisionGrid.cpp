//
// CollisionGrid.cpp
// Bridge between XC_Engine and CollisionGrid
// 
//

#include "API.h"
#include "GridTypes.h"
#include "GridMem.h"
#include "GridMath.h"


ActorInfoHolder* G_AIH = nullptr;
MiniTreeHolder* G_MTH = nullptr;
FMemStack* Mem = nullptr;

namespace cg
{
	const Integers Vector::MASK_3D  ( 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000);
	const Integers Vector::MASK_SIGN( 0x80000000, 0x80000000, 0x80000000, 0x80000000);
	const Integers Vector::MASK_ABS ( 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF);
};

//
// Grid container, UE interface
//
class FCollisionGrid : public FCollisionHashBase
{
public:
	struct Grid* Grid;
	static uint32 GridCount;

	//FCollisionGrid interface.
	FCollisionGrid( class ULevel* Level)
	{
		GridCount++;
		Grid = new ::Grid( Level);
	}
	~FCollisionGrid();

	// FCollisionHashBase interface.
	virtual void Tick();
	virtual void AddActor(AActor *Actor);
	virtual void RemoveActor(AActor *Actor);
	virtual FCheckResult* ActorLineCheck(FMemStack& Mem, FVector End, FVector Start, FVector Extent, uint8 ExtraNodeFlags);
	virtual FCheckResult* ActorPointCheck(FMemStack& Mem, FVector Location, FVector Extent, uint32 ExtraNodeFlags);
	virtual FCheckResult* ActorRadiusCheck(FMemStack& Mem, FVector Location, float Radius, uint32 ExtraNodeFlags);
	virtual FCheckResult* ActorEncroachmentCheck(FMemStack& Mem, AActor* Actor, FVector Location, FRotator Rotation, uint32 ExtraNodeFlags);
	virtual void CheckActorNotReferenced(AActor* Actor) {};
};

uint32 FCollisionGrid::GridCount = 0;

//
// XC_Engine's first interaction
//
extern "C"
{

	TEST_EXPORT FCollisionHashBase* GNewCollisionHash( ULevel* Level)
	{
		if ( !G_AIH )	G_AIH = new ActorInfoHolder();
		if ( !G_MTH )	G_MTH = new MiniTreeHolder();
		GLog->Log( TEXT("[CG] Element holders succesfully spawned.") );
		//Unreal Engine destroys this object
		//Therefore use Unreal Engine allocator
		return new(TEXT("FCollisionGrid")) FCollisionGrid( Level);
	}

}

FCollisionGrid::~FCollisionGrid()
{
	//		DebugLock( "DeleteGrid", 'D');
	GridCount--;
	if ( !GridCount )
	{
		G_AIH->Exit();
		G_MTH->Exit();
		G_AIH = nullptr;
		G_MTH = nullptr;
	}
	delete Grid;
	Grid = nullptr;
};

GCC_STACK_ALIGN void FCollisionGrid::Tick()
{
	Grid->Tick();
}

GCC_STACK_ALIGN void FCollisionGrid::AddActor( AActor* Actor)
{
	Grid->InsertActor( Actor);
}

GCC_STACK_ALIGN void FCollisionGrid::RemoveActor(AActor *Actor)
{
	Grid->RemoveActor( Actor);
}

GCC_STACK_ALIGN FCheckResult* FCollisionGrid::ActorLineCheck(FMemStack& Mem, FVector End, FVector Start, FVector Extent, uint8 ExtraNodeFlags)
{
	::Mem = &Mem;
	guard(FCollisionGrid::ActorLineCheck);
	FCheckResult* Result = nullptr;
	PrecomputedRay Ray( Start, End, Extent, ExtraNodeFlags);
	if ( Ray.IsValid() )
		Result = Grid->LineQuery( Ray, ExtraNodeFlags);
	return Result;
	unguard;
}

GCC_STACK_ALIGN FCheckResult* FCollisionGrid::ActorPointCheck(FMemStack& Mem, FVector Location, FVector Extent, uint32 ExtraNodeFlags)
{
	::Mem = &Mem;
	PointHelper Helper( Location, Extent, ExtraNodeFlags);
	return Helper.QueryGrid( Grid);
}

GCC_STACK_ALIGN FCheckResult* FCollisionGrid::ActorRadiusCheck(FMemStack& Mem, FVector Location, float Radius, uint32 ExtraNodeFlags)
{
	::Mem = &Mem;
	RadiusHelper Helper( Location, Radius, ExtraNodeFlags);
	return Helper.QueryGrid( Grid);
}

GCC_STACK_ALIGN FCheckResult* FCollisionGrid::ActorEncroachmentCheck(FMemStack& Mem, AActor* Actor, FVector Location, FRotator Rotation, uint32 ExtraNodeFlags)
{
	::Mem = &Mem;
	EncroachHelper Helper( Actor, Location, &Rotation, ExtraNodeFlags);
	return Helper.QueryGrid( Grid);
}

