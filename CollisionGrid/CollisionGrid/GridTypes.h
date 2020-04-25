/**
	GridTypes.h
	Author: Fernando Velázquez

	Grid system designed for UE1 physics.

	Actors may reside in:
	- global references if they're too large (>1000 slots)
	- shared references if they touch the boundaries of grid slots
	- internal references held in a mini 'octree' if inside the grid slot

	This grid has been designed so that no upwards references are held
	and memory allocations/deallocations are kept to a minimum (or none)

*/

#pragma once


#define GRID_NODE_DIM 512.f
#define GRID_NODE_SPHERE 887.f
// !!!! sqrt isn't constexpr, so change the sphere everytime we change DIM !!!!
#define GRID_MULT 1.f/GRID_NODE_DIM
#define MAX_TREE_DEPTH 3
#define REQUIRED_FOR_SUBDIVISION 4
#define MAX_NODES_BOUNDARY 512
#define MAX_GRID_COUNT 64
#define MAX_NODE_LINKS 128 //Maximum amount of Node elements before using 'global' placement

extern cg::Vector Grid_Unit;
extern cg::Vector Grid_Mult;
extern cg::Integers XYZi_One;


struct ActorLink;
struct MiniTree;
struct PrecomputedRay;
class GenericQueryHelper;
class PointHelper;
class RadiusHelper;
class EncroachHelper;

enum EFullGrid { E_FullGrid = 0 };


//
// Grid container, UE interface
//
class FCollisionGrid : public FCollisionHashBase
{
public:
	struct Grid* Grid;
	class ActorInfoHolder* AIH;
	class MiniTreeHolder* MTH;
	uint32 CollisionTag;

	//FCollisionGrid interface.
	FCollisionGrid( ULevel* Level);
	~FCollisionGrid();

	// FCollisionHashBase interface.
	virtual void Tick();
	virtual void AddActor(AActor *Actor);
	virtual void RemoveActor(AActor *Actor);
	virtual FCheckResult* ActorLineCheck(FMemStack& Mem, FVector End, FVector Start, FVector Extent, uint32 ActorQueryFlags, uint8 ExtraNodeFlags);
	virtual FCheckResult* ActorPointCheck(FMemStack& Mem, FVector Location, FVector Extent, uint32 ActorQueryFlags, uint32 ExtraNodeFlags);
	virtual FCheckResult* ActorRadiusCheck(FMemStack& Mem, FVector Location, float Radius, uint32 ActorQueryFlags, int32 bAddOtherRadius);
	virtual FCheckResult* ActorEncroachmentCheck(FMemStack& Mem, AActor* Actor, FVector Location, FRotator Rotation, uint32 ActorQueryFlags, uint32 ExtraNodeFlags);
	virtual void CheckActorNotReferenced(AActor* Actor) {};

	// FCollisionGrid interface.
	bool FixActorLocation( AActor* Actor);
};


//
// Basic Actor container
//
struct DE ActorInfo : public FHoldableElement
{
	uint32 ObjIndex;
	AActor* Actor;
	UPrimitive* Primitive;
	uint32 CollisionTag;
	uint32 ActorQueryFlags;
	struct
	{
		uint32 bCommited:1; //Not free (element holder, needed because the reference can be shared)
		uint32 bIsMovingBrush:1;
		uint32 bBoxReject:1;
		uint32 bGlobal:1;
	} Flags;
	cg::Box GridBox;


	//[16] Container info!
/*	uint8 GXStart;
	uint8 GYStart;
	uint8 GZStart;
	uint8 GXEnd;
	uint8 GYEnd;
	uint8 GZEnd;
	*/

	ActorInfo() {}

	static const TCHAR* Name() { return TEXT("ActorInfo"); }

	void Init( AActor* InActor);
	bool IsValid() const;


	bool CanAcquire() const
	{
		return !Flags.bCommited;
	}
	bool CanRelease() const
	{
		return Flags.bCommited;
	}
	void SetAcquired( bool bNewAcquired)
	{
		Flags.bCommited = bNewAcquired;
	}
};

//int tat = sizeof(ActorInfo);

//
// Actor Link container with helper functions
//
class DE ActorLinkContainer : public TArray<ActorInfo*>
{
public:
	uint32 ActorQueryFlags;

	ActorLinkContainer()
		: TArray<ActorInfo*>()
		, ActorQueryFlags(0)
	{}

	~ActorLinkContainer()
	{
		if( Data )
			appFree( Data );
		Data = nullptr;
		ArrayNum = ArrayMax = 0;
	}

	int32 AddItem( ActorInfo* const& Item )
	{
		ActorQueryFlags |= Item->ActorQueryFlags;
		return TArray<ActorInfo*>::AddItem(Item);
	}

	void Remove( int32 Index) //Fast remove, no memory move or deallocation
	{
		if ( Index < --ArrayNum )
			(*this)(Index) = (*this)(ArrayNum);
		(*this)(ArrayNum) = 0; //Temporary
		if ( !ArrayNum )
			ActorQueryFlags = 0;
	}

	bool RemoveItem( ActorInfo* AInfo)
	{
		for ( int32 i=0 ; i<ArrayNum ; i++ )
			if ( (*this)(i) == AInfo )
			{
				Remove(i);
				return true;
			}
		return false;
	}

	class Iterator
	{
		ActorLinkContainer& Cont;
		int32 Cur;
	public:
		Iterator( ActorLinkContainer& InContainer) : Cont(InContainer), Cur(0) {}
		ActorInfo* GetInfo();
	};
};

//
// Grid 'node' used as container for actors
//
struct DE GridElement
{
	ActorLinkContainer Actors;
	uint32 CollisionTag;
	uint8 X, Y, Z, W;

	GridElement( uint32 i, uint32 j, uint32 k);
	~GridElement() {};

	cg::Integers Coords()
	{
		return cg::Integers( X, Y, Z, W);
	}
};

//
// Grid of 128^3 cubes
//
struct DE Grid
{
	cg::Integers Size;
	cg::Box Box;
	ActorLinkContainer Actors;
	GridElement* Nodes;

	Grid( class ULevel* Level);
	~Grid();
	
	bool InsertActorInfo( ActorInfo* AInfo);
	void RemoveActorInfo( ActorInfo* AInfo);
	FCheckResult* LineQuery( const PrecomputedRay& Ray, uint32 ExtraNodeFlags);
	void Tick();

	//Accessor
	GridElement* Node( int32 i, int32 j, int32 k);
	GridElement* Node( const cg::Integers& I);
	cg::Box GetNodeBoundingBox( const cg::Integers& Coords) const;

	//Utils
	void BoundsToGrid( cg::Vector::reg_type vMin, cg::Vector::reg_type vMax, cg::Integers& iMin, cg::Integers& iMax);
};


/*-----------------------------------------------------------------------
                      Specialized Element Holders
-----------------------------------------------------------------------*/

//
// Customized element holder for ActorInfo(s) (should contain ~ elements)
//
class ActorInfoHolder : public TElementHolder<ActorInfo,1500>
{
public:
	//Picks up a new element, will create new holder if no new elements
	ActorInfo* Acquire( class AActor* InitFor)
	{
		int32 GlobalIndex;
		ActorInfo* AInfo = THolder::Acquire(GlobalIndex);
		InitFor->CollisionTag = GlobalIndex;
		if ( AInfo )
			AInfo->Init(InitFor);
		return AInfo;
	}
};


/*-----------------------------------------------------------------------
                               Inlines
-----------------------------------------------------------------------*/

// Convert two Vector4 points into grid coordinates.
inline void Grid::BoundsToGrid
(
	cg::Vector::reg_type vMin,
	cg::Vector::reg_type vMax,
	cg::Integers& iMin,
	cg::Integers& iMax
)
{
	cg::Vector GridMin = Box.Min;
	vMin -= GridMin;
	vMax -= GridMin;
	cg::Vector Top = cg::Vectorize( Size - XYZi_One);
	iMin = Clamp((vMin * Grid_Mult), cg::Vector(E_Zero), Top).Truncate32();
	iMax = Clamp((vMax * Grid_Mult), cg::Vector(E_Zero), Top).Truncate32();
}