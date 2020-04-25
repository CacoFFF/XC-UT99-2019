/**
	GridMem.h
	Author: Fernando Velázquez

	Preallocated memory managing subsystems.
	These are used to eliminate the usage of system memory allocators/deallocators.

*/

#pragma once

//
// Element holder behaviour flags.
//
enum EElementHolderFlags
{
	EHF_ZeroInit           = 0x01, // Memzero the element array upon creation (useful for elements without constructors)
	EHF_DestroyOnRelease   = 0x02, // Call destructor upon element release
	EHF_ZeroOnRelease      = 0x04, // Zero memory upon element release
};

//
// Element holder base class type
// Make all holdable elements subclass of this for correct functionality.
//
class FHoldableElement
{
public:
	constexpr bool CanAcquire() const
	{
		return true;
	}
	constexpr bool CanRelease() const
	{
		return true;
	}
	constexpr void SetAcquired( bool bNewAcquired)
	{
	}
};


//
// Element holder
// Holds a bunch of contiguous objects without required allocation/deallocation
//
template<typename T,uint32 ElementCount,uint32 HolderFlags=0> class TElementHolder
{
protected:
	typedef TElementHolder<T,ElementCount,HolderFlags> THolder;

private:
	THolder* Next;
	uint32   FreeCount;
	uint32   Available[ElementCount];
	T        Elements[ElementCount];

public:
		//Constructor
	TElementHolder()
	:	Next(nullptr)
	,	FreeCount(ElementCount)
	{
		uint32 j = ElementCount;
		for ( uint32 i=0; i<ElementCount; i++)
			Available[i] = --j;
		check(j==0);

		if ( HolderFlags & EHF_ZeroInit )
			appMemzero( &Elements, sizeof(Elements) );

		debugf( TEXT("[CG] Allocated element holder for %s with %i entries"), T::Name(), FreeCount);
	}

	//Destructs whole chain of holders without recursing
	~TElementHolder()
	{
		if ( Next ) // I am main holder
		{
			THolder *C, *N;
			for ( C=this ; C ; C=N )
			{
				N = C->Next;
				C->Next = nullptr;
				delete C;
			}
		}
	}

	//Get index of an element by pointer (local)
	int32 GetLocalIndex( const T* LocalElement )
	{
		if ( (LocalElement < &Elements[0]) || (LocalElement >= &Elements[ElementCount]) )
			return INDEX_NONE;
		return (int32) (((PTRINT)LocalElement - (PTRINT)Elements) / sizeof(Elements[0]));
	}

	//Get index of an element by pointer (global)
	int32 GetGlobalIndex( const T* Element )
	{
		int32 Skipped = 0;
		for ( THolder* Link=this ; Link ; Link=Link->Next )
		{
			int32 LocalIndex = Link->GetLocalIndex(Element);
			if ( LocalIndex < ElementCount )
				return Skipped + LocalIndex;
			Skipped += ElementCount;
		}
		return INDEX_NONE;
	}

	//Get an element using global index
	T* GetIndexedElement( int32 GlobalIndex)
	{
		if ( GlobalIndex >= 0 )
		{
			for ( THolder* Link=this ; Link ; Link=Link->Next )
			{
				if ( GlobalIndex < ElementCount )
					return &Elements[GlobalIndex];
				GlobalIndex -= ElementCount;
			}
		}
		return nullptr;
	}

	//Picks up a new element, will create new holder if no new elements
	T* Acquire( int32& GlobalIndex)
	{
		guard(#T#::Acquire); //Slow
		GlobalIndex = 0;
		for ( THolder* Link=this ; Link ; Link=Link->Next, GlobalIndex+=ElementCount )
		{
			if ( Link->FreeCount > 0 )
			{
				int32 FreeLocal = Link->Available[ --Link->FreeCount ];
				T& Element = Link->Elements[FreeLocal];
				GlobalIndex += FreeLocal;
				check(Element.CanAcquire());
				Element.SetAcquired(true);
				return &Element;
			}
			else if ( !Link->Next )
			{
				debugf( TEXT("[CG] Allocating extra element holder for %s"), T::Name() );
				Link->Next = new THolder();
			}
		}
		GError->Logf( TEXT("ElementHolder::GrabElement error.") );
		return nullptr;
		unguard;
	}

	//Releases element by adding index to '_free' list
	bool Release( T* Element)
	{
		guard(#T#::Release); //Slow
		if ( Element->CanRelease() )
		{
			for ( THolder* Link=this ; Link ; Link=Link->Next )
			{
				int32 i = Link->GetLocalIndex( Element );
				if ( (i != INDEX_NONE) )
				{
					if ( HolderFlags & EHF_DestroyOnRelease )
						Link->Elements[i].~T();
					if ( HolderFlags & EHF_ZeroOnRelease )
						appMemzero( &Link->Elements[i], sizeof(Elements[0]) );

					Link->Elements[i].SetAcquired(false);
					Link->Available[Link->FreeCount++] = i;
					return true;
				}
			}
		}
		return false;
		unguard;
	}

};

