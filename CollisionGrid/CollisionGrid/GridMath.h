#pragma once

#include "PlatformTypes.h"

#include <emmintrin.h>
#include <math.h>

#if __UNREAL_X86__ && !USES_SSE_INTRINSICS
	#include "UnMath_SSE.inl"
#endif

enum EZero    { E_Zero=0 };
enum EStrict  { E_Strict=0 };
enum EInit    { E_Init=0 };
enum ENoZero  { E_NoZero=0 };
enum EUnsafe  { E_Unsafe=0 };

#define GRIDMATH

//From UE
#define SMALL_NUMBER		(1.e-8f)
#define KINDA_SMALL_NUMBER	(1.e-4f)

struct Integers;

inline float appSqrt( float F)
{
	float result;
	__m128 res = _mm_sqrt_ss( _mm_load_ss( &F));
	_mm_store_ss( &result, res);
	return result;
}

inline float Square( float F)
{
	return F*F;
}

namespace cg
{

	struct Vector;

static uint32 NanMask = 0x7F800000;

inline __m128 _newton_raphson_rsqrtss( __m128 n)
{
	const float three = 3.0f;
	const float onehalf = 0.5f;
	__m128 rsq = _mm_rsqrt_ss( n);
	__m128 b = _mm_mul_ss( _mm_mul_ss(n, rsq), rsq); //N*rsq*rsq
	b = _mm_sub_ss( _mm_load_ss(&three), b); //3-N*rsq*rsq
	return _mm_mul_ss( _mm_load_ss(&onehalf), _mm_mul_ss(rsq, b) ); //0.5 * rsq * (3-N*rsq*rsq)
}

inline __m128 _size_xy_zw( __m128 v)
{
	__m128 vv = _mm_mul_ps( v, v);
	__m128 uu = _mm_castsi128_ps( _mm_shuffle_epi32( _mm_castps_si128(vv), 0b10110001)); //Swap x,y and z,w
	return _mm_add_ss( vv, uu); //xx+yy, yy, zz, ww
}




//
// 16-aligned SSE integers
//
struct DE Integers
{
	int32 i, j, k, l;

	//Constructors
	Integers() {}
	Integers( int32 ii, int32 jj, int32 kk, int32 ll)
		:	i(ii),	j(jj),	k(kk),	l(ll)	{}
	Integers( EZero )
	{	_mm_storeu_si128( mm(), _mm_setzero_si128() );	}
	Integers( __m128i reg)
	{	_mm_storeu_si128( (__m128i*)&i, reg);	}

	const TCHAR* String() const;

	//Accessor
	int32& coord( int32 c)
	{	return (&i)[c];	}

	inline __m128i* mm() const
	{
		return (__m128i*)this;
	}

	inline operator __m128() const
	{
		return _mm_loadu_ps( (float*)&i);
	}

	inline operator __m128i() const
	{
		return _mm_loadu_si128( (const __m128i*)&i);
	}

	//**************************
	//Basic comparison operators
	bool operator==(const Integers& I)
	{
		return (_mm_movemask_ps( _mm_castsi128_ps(_mm_cmpeq_epi32( *this, I))) & 0b0111) == 0b0111;
	}
	bool operator!=(const Integers& I) 
	{
		return (_mm_movemask_ps( _mm_castsi128_ps(_mm_cmpeq_epi32( *this, I))) & 0b0111) != 0b0111;
	}


	//********************************
	//Basic assignment logic operators
	Integers operator=(const Integers& I)
	{
		_mm_storeu_si128( mm(), _mm_loadu_si128( I.mm()) );
		return *this;
	}

	//*********************
	//Basic logic operators
	Integers operator+(const Integers& I) const
	{
		return _mm_add_epi32( _mm_loadu_si128( mm()), _mm_loadu_si128(I.mm()));
	}

	Integers operator-(const Integers& I) const
	{
		return _mm_sub_epi32( _mm_loadu_si128( mm()), _mm_loadu_si128(I.mm()));
	}

};

//
// unaligned SSE vector
//
struct DE Vector
{
	typedef __m128 reg_type;

	float X, Y, Z, W;

	Vector() 
	{}

	Vector( float iX, float iY, float iZ, float iW = 0)
		: X(iX) , Y(iY) , Z(iZ) , W(iW)
	{}

	Vector( float U)
	{
		_mm_storeu_ps( &X, _mm_load_ps1(&U));
	}

	Vector( const float* f)
	{
		_mm_storeu_ps( &X, _mm_loadu_ps(f));
	}

	Vector( const Vector& V)
	{
		_mm_storeu_ps( &X, _mm_loadu_ps(&V.X));
	}

	Vector( const FVector& V, EUnsafe)
	{
		_mm_storeu_ps( &X, _mm_and_ps(_mm_loadu_ps((float*)&V), MASK_3D));
	}

	Vector( EZero )
	{
		_mm_storeu_ps( &X, _mm_setzero_ps());
	}

	Vector( __m128 reg)
	{
		_mm_storeu_ps( &X, reg);
	}


	float* operator*()
	{
		return &X;
	}

	const float* operator*() const
	{
		return &X;
	}

	const TCHAR* String() const;

	operator __m128() const
	{
		return _mm_loadu_ps( &X);
	}

	//*********************
	//Basic arithmetic and logic operators
	Vector operator+( const Vector& V) const		{ return _mm_add_ps( *this, V ); }
	Vector operator-( const Vector& V) const		{ return _mm_sub_ps( *this, V ); }
	Vector operator*( const Vector& V) const		{ return _mm_mul_ps( *this, V ); }
	Vector operator/( const Vector& V) const		{ return _mm_div_ps( *this, V ); }

	friend reg_type operator*( reg_type reg, const Vector& V)		{ return _mm_mul_ps( reg, V); }
	friend reg_type operator-=( reg_type& reg, const Vector& V)		{ return reg = _mm_sub_ps( reg, V); }

	float operator|(const Vector& V) const //DOT4
	{
		float Result;
		_mm_store_ss( &Result, _mm_coords_sum_ps(*this * V));
		return Result;
	}

	Vector operator*(const float F) const
	{
		return _mm_mul_ps( *this, _mm_load_ps1(&F) );
	}

	Vector operator/(const float F) const
	{
		return _mm_div_ps( *this, _mm_load_ps1(&F) );
	}

	Vector operator&(const Integers& I) const
	{
		return _mm_and_ps( *this, I );
	}

	Vector operator&(const Vector& V) const
	{
		return _mm_and_ps( *this, V );
	}

	//********************************
	//Basic assignment logic operators

	Vector operator-() const
	{
		return _mm_xor_ps( MASK_SIGN, *this);
	}

	Vector operator=(const Vector& V)
	{
		_mm_storeu_ps( **this, V );
		return *this;
	}

	Vector operator+=(const Vector& V)	{	return (*this = *this + V);	}
	Vector operator-=(const Vector& V)	{	return (*this = *this - V);	}
	Vector operator*=(const Vector& V)	{	return (*this = *this * V);	}
	Vector operator/=(const Vector& V)	{	return (*this = *this / V);	}
	Vector operator*=(const float F)	{	return (*this = *this * F);	}


	//**************************
	//Basic comparison operators
	bool operator<(const Vector& V) const
	{	return _mm_movemask_ps(_mm_cmple_ps( *this, V) ) == 0b1111;	}

	bool operator<=(const Vector& V) const
	{	return _mm_movemask_ps(_mm_cmple_ps( *this, V) ) == 0b1111;	}

	//GET RID OF THIS
	Vector operator<<(const Vector& V) const //Bitmask of coordinates where A < B
	{
		return _mm_cmplt_ps( *this, V);
	}


	//**************************
	//Value handling
	//See if contains nan or infinity
	int32 InvalidBits()
	{
		__m128 m = _mm_load_ps1( reinterpret_cast<const float*>(&NanMask) );
		m = _mm_cmpeq_ps( _mm_and_ps( *this, m), m); //See if (v & m == m)
		return _mm_movemask_ps( m); //See if none of the 4 values threw a NAN/INF
	}
	bool IsValid()
	{
		return InvalidBits() == 0; //See if none of the 4 values threw a NAN/INF
	}
	//IMPORTANT: SEE IF SSE ORDERED (THAT CHECKS FOR NAN'S) WORKS WITH INFINITY


	//**************************
	//Geometrics

	float SizeSq() const
	{
		float Result;
		_mm_store_ss( &Result, _mm_coords_sum_ps(*this * *this));
		return Result;
	}

	uint32 SignBits() //Get sign bits of every component
	{	return _mm_movemask_ps( *this );	}

	// Return components with -1, 0, 1
	Vector SignFloatsNoZero()
	{
		__m128 mm_this = *this;
		__m128 mm_gcmp = _mm_cmpge_ps( mm_this, _mm_setzero_ps()); //bits to 1 if greater-or-equal
		__m128 mm_lcmp = _mm_cmplt_ps( mm_this, _mm_setzero_ps()); //bits to 1 if lesser (TODO: EVALUATE IF INVERT IS CHEAPER)
		return _mm_or_ps
		(
			_mm_and_ps( mm_gcmp, _mm_set_ps( 1, 1, 1,0)),
			_mm_and_ps( mm_lcmp, _mm_set_ps(-1,-1,-1,0))
		);
	}

	//**************************
	//Transformations

	Vector Absolute() const
	{
		return _mm_and_ps( _mm_loadu_ps( (const float*)&MASK_ABS ), *this );
	}

	//Truncate to 4 integers
	Integers Truncate32()
	{
		Integers result;
		__m128i n = _mm_cvttps_epi32( *this ); //Truncate to integer
		_mm_storeu_si128( result.mm(), n);
		return result;
	}

	//Return a normal
	Vector Normal() const
	{
		__m128 a = *this;
		__m128 b = _mm_mul_ps( a, a);
		__m128 c = _mm_shuffle_ps( b, b, 0b00011011);
		c = _mm_add_ps( c, b); //xx+ww, yy+zz, yy+zz, xx+ww
		b = _mm_movehl_ps( b, c);
		c = _mm_add_ss( c, b); //xx+ww+yy+zz
		b = _newton_raphson_rsqrtss( c); //1/sqrt(c)

		 //Need conditional to prevent this from jumping to infinite
		b = _mm_shuffle_ps( b, b, 0b00000000); //Populate YZW with X
		a = _mm_mul_ps( a, b); //Normalized vector
		return a;
//		return *this * (1.f/sqrtf(X*X+Y*Y+Z*Z));
	}

	//Fast 1/x computation
	Vector Reciprocal()
	{
//		return Vector(1,1,1,0) / (*this);
		__m128 x = *this;
		__m128 z = _mm_rcp_ps(x); //z = 1/x estimate
		__m128 _V = _mm_sub_ps( _mm_add_ps( z, z), _mm_mul_ps( x, _mm_mul_ps( z, z))); //2z-xzz
		return _V; //~= 1/x to 0.000012%
	}

	static const Integers MASK_3D;
	static const Integers MASK_SIGN;
	static const Integers MASK_ABS;
};
//**************
}; //NAMESPACE END

inline cg::Vector Min( const cg::Vector& A, const cg::Vector& B)	{	return _mm_min_ps( A, B );	}
inline cg::Vector Max( const cg::Vector& A, const cg::Vector& B)	{	return _mm_max_ps( A, B );	}
inline cg::Vector Clamp( const cg::Vector& V, const cg::Vector& Min, const cg::Vector& Max)		{	return _mm_min_ps( _mm_max_ps( V, Min), Max);	}


// NAMESPACE BEGIN
namespace cg
{

inline Vector Vectorize( const Integers& i)
{
	return _mm_cvtepi32_ps( _mm_loadu_si128(i.mm()) ); //Load and truncate to integer
}

//
// Simple bounding box type
//
struct DE Box
{
	Vector Min, Max;

	//************
	//Constructors

	Box()
	{}

	Box( const Box& B)
		:	Min(B.Min)
		,	Max(B.Max)
	{}

	//Create an empty box
	Box( EZero)
	{
		__m128 m = _mm_setzero_ps();
		_mm_storeu_ps( *Min, m);
		_mm_storeu_ps( *Max, m);
	} 

	//Non-strict constructor: used when Min, Max have to be deducted
	Box( const Vector& A, const Vector& B)
		:	Min( ::Min(A,B))
		,	Max( ::Max(A,B))
	{}

	//Strict constructor: used when Min, Max are obvious
	Box( const Vector& InMin, const Vector& InMax, EStrict)
		:	Min(InMin)
		,	Max(InMax)
	{}

	Box( const FBox& UTBox)
	{
		__m128 mask = _mm_castsi128_ps( _mm_load_si128( Vector::MASK_3D.mm() ));
		_mm_storeu_ps( *Min, _mm_and_ps( _mm_loadu_ps(&UTBox.Min.X), mask) );
		_mm_storeu_ps( *Max, _mm_and_ps( _mm_loadu_ps(&UTBox.Max.X), mask) );
	}

	//Construct a box containing all of Unreal vectors in the list
	Box( FVector* VList, int32 VNum)
	{
//		const uint32 imask = 0xFFFFFFFF;
		const float boundmin = -32768.f;
		const float boundmax = 32768.f; //Unreal bounds
		float* fArray = (float*)VList;

		//Load last vector first
		VNum--;
		__m128 mi = _mm_castsi128_ps( _mm_loadl_epi64( (__m128i*)(fArray+3*VNum) )); //X,Y,0,0
		__m128 ma = _mm_load_ss( fArray + 3*VNum + 2 ); //Z,0,0,0
		mi = _mm_movelh_ps( mi, ma); //X,Y,Z,0
		ma = mi;
		//Now expand using other vectors
		for ( int32 i=0 ; i<VNum ; i++ )
		{
			__m128 v = _mm_loadu_ps( fArray);
			mi = _mm_min_ps( mi, v );
			ma = _mm_max_ps( ma, v );
			fArray += 3;
		}
		//Clamp to unreal bounds
		__m128 mb = _mm_load_ps1( &boundmin);
		mi = _mm_max_ps( mi, mb);
		mb = _mm_load_ps1( &boundmax);
		ma = _mm_min_ps( ma, mb);
		//Set W=0
		__m128 mask = _mm_load_ps( (const float*) &Vector::MASK_3D); //m,m,m,0
//		mask = _mm_pshufd_ps( mask, 0b11000000); 
		mi = _mm_and_ps( mi, mask);
		ma = _mm_and_ps( ma, mask);
		//Save
		_mm_storeu_ps( *Min, mi);
		_mm_storeu_ps( *Max, ma);
	}

	//********************************
	//Basic assignment logic operators

	Box operator=(const Box& B)
	{
		Min = B.Min; //Does compiler automatically propagate user defined = operators?
		Max = B.Max;
		return *this;
	}

	//*********************
	//Basic logic operators
	Box operator+(const Vector& V) const
	{
		return Box( Min+V, Max+V, E_Strict);
	}

	Box operator-(const Vector& V) const
	{
		return Box( Min-V, Max-V, E_Strict);
	}

	Box operator*(const Vector& V) const
	{
		return Box( Min*V, Max*V, E_Strict);
	}
	
	//***********************
	//Characteristics queries

	bool Intersects3( const Box& Other) const
	{
		return (_mm_movemask_ps(_mm_cmple_ps( Min, Other.Max))
			& _mm_movemask_ps(_mm_cmple_ps( Other.Min, Max))
			& 0b0111) == 0b0111;
	}

	bool Contains3( const Vector& Other) const
	{
		return (_mm_movemask_ps(_mm_cmple_ps( Min, Other))
			& _mm_movemask_ps(_mm_cmple_ps( Other, Max))
			& 0b0111) == 0b0111;
	}

	//**************************
	//Transformations

	Box Expand( const Vector& Towards)
	{
		Min = ::Min( Min, Towards);
		Max = ::Max( Max, Towards);
		return *this;
	}

	Box Expand( const Box& By)
	{
		Min = ::Min( Min, By.Min);
		Max = ::Max( Max, By.Max);
		return *this;
	}

	//Enlarge a box by a 'Extent' vector
	void ExpandBounds( const Vector& By) 
	{
		Min -= By;
		Max += By;
	}

	const cg::Vector& GetExtrema( int i) const
	{
		return (&Min)[i];
	}
};

} //Namespace cg - end

typedef cg::Vector FVector4;


inline FVector FVectorUT( const cg::Vector& V)
{
	if ( sizeof(FVector) == 16 ) //64 bit builds
	{
		FVector Result;
		*(cg::Vector*)&Result = V;
		return Result;
	}
	else
		return FVector( V.X, V.Y, V.Z);
}

