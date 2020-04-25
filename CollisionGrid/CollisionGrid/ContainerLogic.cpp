

#include "CollisionGrid.h"

//*************************************************
//
// ActorInfo
//
//*************************************************

//Set important variables
void ActorInfo::Init( AActor* InActor)
{
	guard_slow(ActorInfo::Init);

	ObjIndex = InActor->GetIndex();
	Actor = InActor;
	Primitive = InActor->GetPrimitive();
	ActorQueryFlags = FCollisionHashBase::GetActorQueryFlags(InActor);
	GridBox = Primitive->GetCollisionBoundingBox(InActor);
	Flags.bIsMovingBrush = InActor->IsMovingBrush();
	if ( InActor->Brush )
	{
		GridBox.ExpandBounds( cg::Vector( 2.0f, 2.0f, 2.0f, 0)); //Just in case
		Flags.bBoxReject = true;
	}

	unguard_slow;
}

bool ActorInfo::IsValid() const
{
	if ( !Flags.bCommited )
	{}
	else if ( UObject::GetIndexedObject(ObjIndex) != Actor )
		debugf( TEXT("[CG] ActorInfo::IsValid -> Using invalid object"));
	else if ( Actor->bDeleteMe || !Actor->bCollideActors )
		debugf( TEXT("[CG] ActorInfo::IsValid -> %s shouldn't be in the grid"), Actor->GetName() );
//	else if ( reinterpret_cast<ActorInfo*>(Actor->CollisionTag) != this )
//		debugf( TEXT("[CG] ActorInfo::IsValid -> Mismatching CollisionTag"));
	else
		return true;
	return false;
}


//*************************************************
//
// Grid
//
//*************************************************

Grid::Grid( ULevel* Level)
	: Actors()
{
	UModel* Model = Level->Model;
	if ( Model->RootOutside )
	{
		Size = cg::Integers( 128, 128, 128, 0);
		Box = cg::Box( cg::Vector(-32768,-32768,-32768,0), cg::Vector(32768,32768,32768,0), E_Strict);
	}
	else
	{
		cg::Box GridBox( &Model->Points(0), Model->Points.Num());
		Size = ((GridBox.Max - GridBox.Min) * Grid_Mult + cg::Vector(0.99f,0.99f,0.99f)).Truncate32();
		Size.l = 0;
		Box = GridBox;
		UE_DEV_THROW( Size.i > 128 || Size.j > 128 || Size.k > 128, "New grid exceeds 128^3 dimensions"); //Never deprecate
	}
	uint32 BlockSize = Size.i * Size.j * Size.k * sizeof(struct GridElement);
	Nodes = (GridElement*) appMalloc( BlockSize, TEXT("GridNodes"));
	uint32 l = 0;
	for ( int32 i=0 ; i<Size.i ; i++ )
	for ( int32 j=0 ; j<Size.j ; j++ )
	for ( int32 k=0 ; k<Size.k ; k++ )
		new ( Nodes + l++, E_Stack) GridElement(i,j,k);
	debugf( TEXT("[CG] Grid allocated [%i,%i,%i]"), Size.i, Size.j, Size.k );
}

Grid::~Grid()
{
	uint32 Total = Size.i * Size.j * Size.k;
	for ( uint32 i=0 ; i<Total ; i++ )
		Nodes[i].~GridElement();
	appFree( Nodes);
}

GridElement::GridElement( uint32 i, uint32 j, uint32 k)
	: Actors()
	, CollisionTag(0)
	, X(i), Y(j), Z(k), W(0)
{}

GridElement* Grid::Node( int32 i, int32 j, int32 k)
{
#if GRID_DEVELOPMENT
	check(i >= 0);
	check(i < Size.i);
	check(j >= 0);
	check(j < Size.j);
	check(k >= 0);
	check(k < Size.k);
#endif
	UE_DEV_THROW( i >= Size.i || j >= Size.j || k >= Size.k , "Bad node request"); //Never deprecate
	return &Nodes[ i*Size.j*Size.k + j*Size.k + k];
}

GridElement* Grid::Node( const cg::Integers& I)
{
	return Node( I.i, I.j, I.k);
}

bool Grid::InsertActorInfo( ActorInfo* AInfo)
{
	//Classify whether to add as boundary actor or inner actor
	//It may even be possible that an actor actually doesn't fit here!!

	if ( Box.Intersects3(AInfo->GridBox) )
	{
		cg::Integers Min, Max;
		BoundsToGrid( AInfo->GridBox.Min, AInfo->GridBox.Max, Min, Max);

		// Calculate how big the node list will be before doing any listing
		cg::Integers Total = XYZi_One + Max - Min;
		UE_DEV_THROW( Total.i <= 0 || Total.j <= 0 || Total.k <= 0, "Bad iBounds calculation"); 

		// No placement
		if ( Total.i <= 0 || Total.j <= 0 || Total.k <= 0 )
			return false;
		// Global placement
		else if ( Total.i*Total.j*Total.k >= MAX_NODE_LINKS )
		{
			AInfo->Flags.bGlobal = 1;
			Actors.AddItem( AInfo);
		}
		// Grid placement
		else
		{
			AInfo->Flags.bGlobal = 0;
			for ( int32 i=Min.i ; i<=Max.i ; i++ )
			for ( int32 j=Min.j ; j<=Max.j ; j++ )
			for ( int32 k=Min.k ; k<=Max.k ; k++ )
			{
				GridElement* Element = Node( i, j, k);
				if ( Element )
					Element->Actors.AddItem(AInfo);
			}
		}
	}
	else
	{
		AInfo->Flags.bGlobal = 1;
		Actors.AddItem( AInfo);
	}

	return true;
}


void Grid::RemoveActorInfo( ActorInfo* AInfo)
{
	if ( AInfo->Flags.bGlobal )
		Actors.RemoveItem( AInfo);
	else
	{
		cg::Integers Min, Max;
		BoundsToGrid( AInfo->GridBox.Min, AInfo->GridBox.Max, Min, Max);
		for ( int i=Min.i ; i<=Max.i ; i++ )
		for ( int j=Min.j ; j<=Max.j ; j++ )
		for ( int k=Min.k ; k<=Max.k ; k++ )
			Node(i,j,k)->Actors.RemoveItem( AInfo);
	}
}


cg::Box Grid::GetNodeBoundingBox( const cg::Integers& Coords) const
{
	cg::Vector Min = Box.Min + cg::Vectorize(Coords) * Grid_Unit; //(VC2015) If I don't use this, two useless memory access MOVAPS are added
	return cg::Box( Min, Min+Grid_Unit, E_Strict);
}

void Grid::Tick()
{
	guard(Grid::Tick);
	unguard;
}
