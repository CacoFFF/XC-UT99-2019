
/**
	GridQueries.h
	Author: Fernando Velázquez

	Grid query system.

	
	Actors may reside in:
	- global references if they're too large (>1000 slots)
	- shared references if they touch the boundaries of grid slots
	- internal references held in a mini 'octree' if inside the grid slot

	This grid has been designed so that no upwards references are held
	and memory allocations/deallocations are kept to a minimum (or none)

*/

#pragma once


namespace cg
{
namespace Query
{


//
// Base class of all queries.
//
template<class T> class Base
{
public:
	Grid*           Grid;
	FCheckResult*   ResultList;
	FMemStack&      Mem;
	uint32          ActorQueryFlags;
	uint32          ExtraNodeFlags;
	uint32          CollisionTag;

	Base( FCollisionGrid* InCollisionGrid, FMemStack& InMem, uint32 InActorQueryFlags, uint32 InExtraNodeFlags);

	bool ShouldQueryActor( ActorInfo* Info) const;
	bool Intersects( const cg::Box& Box) const;
	void PrimitiveQuery( const ActorInfo* Info);

	void Query();
	void QueryActors( ActorLinkContainer& ActorList);
};

class Radius : public Base<Radius>
{
public:
	cg::Vector SearchOrigin;
	float      SearchRadius;

	Radius( FCollisionGrid* InCollisionGrid, FMemStack& InMem, uint32 InActorQueryFlags, int32 InAddOtherRadius, const FVector& InLocation, float InRadius);

	void Query();
	void PrimitiveQuery( const ActorInfo* Info);
};

//
// Base class of Box bound queries
//
template<class T> class BoxBounded : public Base<T>
{
public:
	cg::Box Bounds;

	BoxBounded( FCollisionGrid* InCollisionGrid, FMemStack& InMem, uint32 InActorQueryFlags, uint32 InExtraNodeFlags, cg::Box Bounds);

	void Query();
	bool Intersects( const cg::Box& Box) const;
};


class Encroachment : public BoxBounded<Encroachment>
{
public:
	AActor*  Actor;
	UPrimitive* Primitive;
	uint32   bCheckWorld;

	Encroachment( FCollisionGrid* InCollisionGrid, FMemStack& InMem, uint32 InActorQueryFlags, uint32 InExtraNodeFlags, AActor* InActor);

	bool ShouldQueryActor( ActorInfo* Info) const;
	void PrimitiveQuery( const ActorInfo* Info);
};

class Point : public BoxBounded<Point>
{
public:
	const FVector& Location;
	const FVector& Extent;

	Point( FCollisionGrid* InCollisionGrid, FMemStack& InMem, uint32 InActorQueryFlags, uint32 InExtraNodeFlags, const FVector& InLocation, const FVector& InExtent);

	cg::Box MakeBounds( const FVector& Location, const FVector& Extent);
	void PrimitiveQuery( const ActorInfo* Info);
};

class Line : public Base<Line>
{
public:
public:
	cg::Vector End;
	cg::Vector Start;
	cg::Vector Extent;

	DWORD BoxExtremas; //TODO: Make ray calculation run on demand
	const FVector& TraceStart;
	const FVector& TraceEnd;

	cg::Vector Dir;
	cg::Vector Inv;

	Line( FCollisionGrid* InCollisionGrid, FMemStack& InMem, uint32 InActorQueryFlags, uint32 InExtraNodeFlags, const FVector& InEnd, const FVector& InStart, const FVector& InExtent);

	void Query();
	void PrimitiveQuery( const ActorInfo* Info);

	bool Intersects( const cg::Box& Box);
	void ComputeRay();
	cg::Vector GridStart();
};


/*-----------------------------------------------------------------------------
	Query Constructors.

Initializes the query's state (full or on-demand)
-----------------------------------------------------------------------------*/

#define BASE_CTOR_PARAMS InCollisionGrid, InMem, InActorQueryFlags, InExtraNodeFlags

//
// Construct the Base query structure.
//
template<class T>
inline cg::Query::Base<T>::Base( FCollisionGrid* InCollisionGrid, FMemStack& InMem, uint32 InActorQueryFlags, uint32 InExtraNodeFlags)
	: Grid            (InCollisionGrid->Grid)
	, ResultList      (nullptr)
	, Mem             (InMem)
	, ActorQueryFlags (InActorQueryFlags)
	, ExtraNodeFlags  (InExtraNodeFlags)
	, CollisionTag    (++InCollisionGrid->CollisionTag)
{
}

template<class T>
inline cg::Query::BoxBounded<T>::BoxBounded( FCollisionGrid* InCollisionGrid, FMemStack& InMem, uint32 InActorQueryFlags, uint32 InExtraNodeFlags, cg::Box InBounds)
	: Base(BASE_CTOR_PARAMS)
	, Bounds(InBounds)
{
}

inline cg::Query::Radius::Radius( FCollisionGrid* InCollisionGrid, FMemStack& InMem, uint32 InActorQueryFlags, int32 InExtraNodeFlags, const FVector& InLocation, float InRadius)
	: Base(BASE_CTOR_PARAMS)
	, SearchOrigin( InLocation, E_Unsafe)
	, SearchRadius( InRadius)
{
}

inline cg::Query::Encroachment::Encroachment( FCollisionGrid* InCollisionGrid, FMemStack& InMem, uint32 InActorQueryFlags, uint32 InExtraNodeFlags, AActor* InActor)
	: BoxBounded(BASE_CTOR_PARAMS, InActor->GetPrimitive()->GetCollisionBoundingBox(InActor))
	, Actor(InActor)
	, Primitive(InActor->GetPrimitive())
	, bCheckWorld(InActor->IsBrush())
{
}

inline cg::Query::Point::Point( FCollisionGrid* InCollisionGrid, FMemStack& InMem, uint32 InActorQueryFlags, uint32 InExtraNodeFlags, const FVector& InLocation, const FVector& InExtent)
	: BoxBounded(BASE_CTOR_PARAMS, MakeBounds(InLocation,InExtent) )
	, Location(InLocation)
	, Extent(InExtent)
{
}

inline cg::Query::Line::Line( FCollisionGrid* InCollisionGrid, FMemStack& InMem, uint32 InActorQueryFlags, uint32 InExtraNodeFlags, const FVector& InEnd, const FVector& InStart, const FVector& InExtent)
	: Base(BASE_CTOR_PARAMS)
	, End   (&InEnd.X)
	, Start (&InStart.X)
	, Extent(&InExtent.X)
	, BoxExtremas((DWORD)INDEX_NONE)
	, TraceStart(InStart)
	, TraceEnd(InEnd)
{
}

#undef BASE_CTOR_PARAMS

/*-----------------------------------------------------------------------------
	Grid Query directive.

The grid has two main actor storages.
Global and Gridded.

Global actors are checked first, then the ones in the grid elements.
-----------------------------------------------------------------------------*/

template<class T>
inline void cg::Query::Base<T>::Query()
{
}

inline void cg::Query::Radius::Query()
{
	QueryActors(Grid->Actors);

	cg::Integers Min, Max;
	cg::Vector Expand( SearchRadius);
	Grid->BoundsToGrid( SearchOrigin-Expand, SearchOrigin+Expand, Min, Max);

	for ( int i=Min.i ; i<=Max.i ; i++ )
	for ( int j=Min.j ; j<=Max.j ; j++ )
	for ( int k=Min.k ; k<=Max.k ; k++ )			
	{
		GridElement* Element = Grid->Node(i,j,k);
		if ( Element )
			QueryActors(Element->Actors);
	}
}

template<class T>
inline void cg::Query::BoxBounded<T>::Query()
{
	static_cast<Base<T>*>(this)->QueryActors(Grid->Actors);

	cg::Integers Min, Max;
	Grid->BoundsToGrid( Bounds.Min, Bounds.Max, Min, Max);

	for ( int i=Min.i ; i<=Max.i ; i++ )
	for ( int j=Min.j ; j<=Max.j ; j++ )
	for ( int k=Min.k ; k<=Max.k ; k++ )			
	{
		GridElement* Element = Grid->Node(i,j,k);
		if ( Element )
			static_cast<T*>(this)->QueryActors(Element->Actors);
	}
}

inline void cg::Query::Line::Query()
{
	QueryActors(Grid->Actors);

	// Start inside the grid, do not go past this point if line doesn't intersect it
	cg::Vector FixedStart = GridStart();
	if ( ActorQueryFlags == 0 )
		return;

	cg::Integers iEnd, iStart;
	cg::Vector Expand = (End - FixedStart).SignFloatsNoZero() * Extent;
	Grid->BoundsToGrid( End+Extent, FixedStart-Extent, iEnd, iStart);

	// One grid element, no need to traverse
	if ( iStart == iEnd )
	{
		GridElement* Element = Grid->Node(iStart);
		if ( Element )
			QueryActors(Element->Actors);
		return;
	}

	// Calc directions
	int32 AdvCount = 0;
	cg::Integers iAdv(0,0,0,0);
	for ( uint32 i=0 ; i<3 ; i++ )
	{
		int32 j = iEnd.coord(i)-iStart.coord(i);
		iAdv.coord(i) = (j>0) - (j<0);
		AdvCount += (iAdv.coord(i) != 0);
	}

	// Special case, single axis trace
	if ( AdvCount == 1 )
	{
		cg::Integers iGrid = iStart;
		while ( true )
		{
			GridElement* Element = Grid->Node(iGrid);
			if ( Element )
				QueryActors(Element->Actors);

			if ( iGrid == iEnd )
				break;
			iGrid = iGrid + iAdv;
		}
	}
	// Scan nodes
	else
	{
		// NOTE: First node is guaranteed to intersect the ray.
		// The boxes will be expanded by Extent, so the ray's shape is cubic.
		bool bIntersect = false;
		bool bFound_j = false;
		bool bFound_k = false;
		cg::Integers iGrid = iStart;

		// [i] goes from start to end, not bound by special conditions.
		for ( ; ; iGrid.i+=iAdv.i, iGrid.j=iStart.j, iGrid.k=iStart.k)
		{
			for ( bFound_j=false; ; iGrid.j+=iAdv.j, iGrid.k=iStart.k)
			{
				for ( bFound_k=false; ; iGrid.k+=iAdv.k)
				{
					// Process intersection, query actors
					bIntersect = Intersects(Grid->GetNodeBoundingBox(iGrid));
					if ( bIntersect )
					{
						GridElement* Element = Grid->Node(iGrid);
						if ( Element )
							QueryActors(Element->Actors);
					}

					// [k] first found, update starting point
					if ( bIntersect && !bFound_k )
					{
						iStart.k = iGrid.k;
						bFound_k = true;
					}

					// No more [k] intersections
					if ( !bIntersect && bFound_k )
						break;
					// End loop
					if ( iGrid.k == iEnd.k )
						break;
				}

				// [j] first found, update starting point
				if ( bFound_k && !bFound_j )
				{
					iStart.j = iGrid.j;
					bFound_j = true;
				}

				// No more [j] intersections (no [k] found)
				if ( !bFound_k && bFound_j )
					break;
				// End loop
				if ( iGrid.j == iEnd.j )
					break;
			}
			// End loop
			if ( iGrid.i == iEnd.i )
				break;
		}
	}
}


/*-----------------------------------------------------------------------------
	Actor Query directive.

Does the following with the ActorInfos:
- Apply logic filter.
- Apply box filter.
- Check validity.
-- Remove from list if invalid.
- Do the primitive checks.
-- Add new result if passed.
-----------------------------------------------------------------------------*/

template<class T>
inline void cg::Query::Base<T>::QueryActors( ActorLinkContainer& ActorList)
{
	for ( int32 i=0; i<ActorList.Num(); i++)
	{
		ActorInfo* Info = ActorList(i);

		// Check once, filter by query flags
		if ( (Info->CollisionTag == CollisionTag) || !(Info->ActorQueryFlags & ActorQueryFlags) )
			continue;
		Info->CollisionTag = CollisionTag;

		if	(	static_cast<T*>(this)->ShouldQueryActor(Info)
			&&	(!Info->Flags.bBoxReject || static_cast<T*>(this)->Intersects(Info->GridBox)) )
		{
			if ( !Info->IsValid() )
			{
				ActorList.Remove(i--);
				continue;
			}
			static_cast<T*>(this)->PrimitiveQuery(Info);
		}
	}
}



/*-----------------------------------------------------------------------------
	ActorInfo logic filters.
-----------------------------------------------------------------------------*/

template<class T>
inline bool cg::Query::Base<T>::ShouldQueryActor( ActorInfo* Info) const
{
	return true;
}

inline bool cg::Query::Encroachment::ShouldQueryActor( ActorInfo* Info) const
{
	return	Base::ShouldQueryActor( Info )
		&&	Info->Actor != Actor //Do not encroach self
		&&	!Info->Flags.bIsMovingBrush; //Do not encroach Moving Brushes
}

/*-----------------------------------------------------------------------------
	ActorInfo box filters.
-----------------------------------------------------------------------------*/

template<class T>
inline bool cg::Query::Base<T>::Intersects( const cg::Box& Box) const
{
	return true;
}

template<class T>
inline bool cg::Query::BoxBounded<T>::Intersects( const cg::Box& Box) const
{
	return Bounds.Intersects3(Box);
}

inline bool cg::Query::Line::Intersects( const cg::Box& Box)
{
	// Backface culling can't check if already inside the Box, use this.
	cg::Box EBox( Box.Min-Extent, Box.Max+Extent, E_Strict);
	if ( EBox.Contains3(Start) )
		return true;

	// Compute data needed for Box intersection checks
	if ( BoxExtremas == (DWORD)INDEX_NONE )
		ComputeRay();

	cg::Vector T
	(	EBox.GetExtrema( ((BYTE*)&BoxExtremas)[0] ).X
	,	EBox.GetExtrema( ((BYTE*)&BoxExtremas)[1] ).Y
	,	EBox.GetExtrema( ((BYTE*)&BoxExtremas)[2] ).Z );
	T = (T - Start) * Inv; //The 'seconds' it takes to reach each plane (second = unit moved)

	if ( (T.X >= T.Y) && (T.X >= T.Z) ) //X is maximum
	{
		if ( T.X >= 0 )
		{
			float ptY = Start.Y + Dir.Y * T.X;
			float ptZ = Start.Z + Dir.Z * T.X;
			return ptY >= EBox.Min.Y && ptY <= EBox.Max.Y
				&& ptZ >= EBox.Min.Z && ptZ <= EBox.Max.Z;
		}
	}
	else if ( T.Y >= T.Z ) //Y is maximum
	{
		if ( T.Y >= 0 )
		{
			float ptX = Start.X + Dir.X * T.Y;
			float ptZ = Start.Z + Dir.Z * T.Y;
			return ptX >= EBox.Min.X && ptX <= EBox.Max.X
				&& ptZ >= EBox.Min.Z && ptZ <= EBox.Max.Z;
		}
	}
	else //Z is maximum
	{
		if ( T.Z >= 0 )
		{
			float ptX = Start.X + Dir.X * T.Z;
			float ptY = Start.Y + Dir.Y * T.Z;
			return ptX >= EBox.Min.X && ptX <= EBox.Max.X
				&& ptY >= EBox.Min.Y && ptY <= EBox.Max.Y;
		}
	}

	return false;
}

//TODO: THIS IS NEEDED TO ENSURE THE GRID WON'T HAVE PROBLEMS IN ADDITIVE LEVELS
inline cg::Vector cg::Query::Line::GridStart()
{
	cg::Box Box = Grid->Box;
	Box.ExpandBounds( cg::Vector(1,1,1,0) );
	if ( Box.Contains3(Start) )
		return Start;

	// Compute data needed for Box intersection checks
	if ( BoxExtremas == (DWORD)INDEX_NONE )
		ComputeRay();

	cg::Vector T
	(	Box.GetExtrema( ((BYTE*)&BoxExtremas)[0] ).X
	,	Box.GetExtrema( ((BYTE*)&BoxExtremas)[1] ).Y
	,	Box.GetExtrema( ((BYTE*)&BoxExtremas)[2] ).Z );
	T = (T - Start) * Inv; //The 'seconds' it takes to reach each plane (second = unit moved)

	float Time = Max(T.X,Max(T.Y,T.Z));
	cg::Vector Point = Start + Dir * Time;
	if ( Box.Contains3(Point) )
		return Point;

	// Failed to hit the box
	ActorQueryFlags = 0;
	return End;
}


/*-----------------------------------------------------------------------------
	Actor primitive query.

At this stage, the ActorInfo has been fully validated and is safe to perform
the respective primitive query and FCheckResult creation in case of a 'hit'.
-----------------------------------------------------------------------------*/

template<class T>
inline void cg::Query::Base<T>::PrimitiveQuery( const ActorInfo* Info)
{
}

inline void cg::Query::Radius::PrimitiveQuery( const ActorInfo* Info)
{
	cg::Vector Location( Info->Actor->Location, E_Unsafe);
	float MaxRange = SearchRadius;
	if ( ExtraNodeFlags )
		MaxRange += Info->Actor->CollisionRadius;
	if ( (Location-SearchOrigin).SizeSq() <= Square(MaxRange) )
	{
		ResultList = new(Mem) FCheckResult( 1.f, ResultList);
		ResultList->Actor = Info->Actor;
	}
}

inline void cg::Query::Encroachment::PrimitiveQuery( const ActorInfo* Info)
{
	AActor* Other = Info->Actor;
	if ( !bCheckWorld || Other->bCollideWorld )
	{
		FCheckResult TestHit( 1.f, ResultList);
		if ( !Primitive->PointCheck( TestHit, Actor, Other->Location, Other->GetCylinderExtent(), 0) )
		{
			TestHit.Actor     = Other;
			TestHit.Primitive = nullptr;
			ResultList = new(Mem) FCheckResult(TestHit);
		}
	}
}

inline void cg::Query::Point::PrimitiveQuery( const ActorInfo* Info)
{
	FCheckResult TestHit( 1.f, ResultList);
	if ( !Info->Primitive->PointCheck( TestHit, Info->Actor, Location, Extent, ExtraNodeFlags) )
		ResultList = new(Mem) FCheckResult(TestHit);
}

inline void cg::Query::Line::PrimitiveQuery( const ActorInfo* Info)
{
	FCheckResult TestHit( 1.f, ResultList);
	if ( !Info->Primitive->LineCheck( TestHit, Info->Actor, TraceEnd, TraceStart, *(FVector*)&Extent, ExtraNodeFlags) )
		ResultList = new(Mem) FCheckResult(TestHit);
}


/*-----------------------------------------------------------------------------
	Common query utils
-----------------------------------------------------------------------------*/



inline cg::Box cg::Query::Point::MakeBounds( const FVector& Location, const FVector& Extent)
{
	cg::Vector L( &Location.X);
	cg::Vector E( &Extent.X);
	return cg::Box( L-E, L+E, E_Strict);
}

inline void cg::Query::Line::ComputeRay()
{
	Dir = ((End - Start) & cg::Vector::MASK_3D).Normal();
	Inv = cg::Vector(1.f) / Dir;

	// We need to check against planes facing opposite to our line.
	BoxExtremas = 0;
	uint8* Extremas = (uint8*)&BoxExtremas;
	float* Components = &Inv.X;
	for ( INT i=0 ; i<3 ; i++ )
	{
		if ( appIsNan(Components[i]) )
			Components[i] = 0;
		else if ( Components[i] < 0.0f )
			Extremas[i] = 1;
	}
}

};
};



