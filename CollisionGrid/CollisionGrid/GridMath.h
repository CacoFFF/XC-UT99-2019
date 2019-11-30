#pragma once

#include "PlatformTypes.h"

#include <emmintrin.h>
#include <math.h>


enum EZero    { E_Zero=0 };
enum EStrict  { E_Strict=0 };
enum EInit    { E_Init=0 };
enum ENoZero  { E_NoZero=0 };
enum EStatic3D{ E_Static3D=0 };
enum ENoSSEFPU{ E_NoSSEFPU=0 };
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

#define _mm_pshufd_ps(v,i) _mm_castsi128_ps( _mm_shuffle_epi32( _mm_castps_si128(v), i))





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

	//**************************
	//Basic comparison operators
	bool operator ==(const Integers& I)
	{
		__m128i a, b;
		a = _mm_loadu_si128(mm());
		b = _mm_loadu_si128(I.mm());
		__m128i c = _mm_cmpeq_epi32( a, b);
		return _mm_movemask_ps( *(__m128*)&c ) == 0b1111;
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
	float X, Y, Z, W;

	Vector() 
	{}

	Vector( float iX, float iY, float iZ, float iW = 0)
		: X(iX) , Y(iY) , Z(iZ) , W(iW)
	{}

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

	Vector( float U, EStatic3D)
		: X(U) , Y(U) , Z(U) , W(0)
	{}

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
	Vector operator+( const Vector& V) const	{	return _mm_add_ps( *this, V );	}
	Vector operator-( const Vector& V) const	{	return _mm_sub_ps( *this, V );	}
	Vector operator*( const Vector& V) const	{	return _mm_mul_ps( *this, V );	}
	Vector operator/( const Vector& V) const	{	return _mm_div_ps( *this, V );	}

	float operator|(const Vector& V) const //DOT4
	{
/*		__m128 _V = _mm_mul_ps(_mm_loadu_ps(fa()), _mm_loadu_ps(V.fa()));
		__m128 x = _mm_castsi128_ps( _mm_shuffle_epi32( _mm_castps_si128(_V), 0b11110101)); //Force a PSHUFD (y,y,w,w)
		x = _mm_add_ps( x, _V); //x+y,...,z+w,...
		_V = _mm_movehl_ps( _V, x); //z+w,........
		_V = _mm_add_ss( _V, x);
		float ReturnValue;
		_mm_store_ss( &ReturnValue, _V);
		return ReturnValue;*/
		return X*V.X + Y*V.Y + Z*V.Z;
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
	{	return _mm_movemask_ps( *this << V ) == 0b1111;	}

	bool operator<=(const Vector& V) const
	{	return _mm_movemask_ps(_mm_cmple_ps( *this, V) ) == 0b1111;	}

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


	//Compute >= in parallel, store in integers
	Integers GreaterThanZeroPS()
	{
		__m128i cmp = _mm_castps_si128( _mm_cmpge_ps( *this , _mm_setzero_ps() ) );
		cmp = _mm_srli_epi32( cmp, 31);
		return cmp;
	}

	//**************************
	//Geometrics

	//Cylinder check against Radius, Height
	bool InCylinder(float Radius, float Height) const //VS2015 generates an unnecessary MOVAPS instruction!
	{
		__m128 v = _size_xy_zw( *this ); //XX+YY,YY,ZZ,WW
		__m128 h = _mm_load_ss(&Height); //H,0,0,0
		__m128 r = _mm_load_ss(&Radius); //R,0,0,0
		r = _mm_movelh_ps( r, h); //R,0,H,0
		r = _mm_mul_ps( r, r); //RR,0,HH,0
		v = _mm_cmple_ps( v, r); //C,C,C,C comparison result (C=0 if greater, C=-1 if less or equal)
		return (_mm_movemask_ps(v) & 0b0101) == 0b0101; //See that Horiz,Vert are less than Radius,Height
	}

	//Cylinder check against Radius (infinite height)
	bool InCylinder( float Radius) const
	{
		return X*X+Y*Y < Radius*Radius;
	}

	//Cylinder check against unreal Extent vector
	bool InCylinder( const Vector& Extent) const //FIX THIS
	{
		__m128 v = _size_xy_zw( *this ); //XX+YY,YY,ZZ,WW
		__m128 r = Extent; //R,R,H,0
		r = _mm_mul_ps( r, r); //RR,RR,HH,0
		v = _mm_cmple_ps( v, r); //C,C,C,C comparison result (C=0 if greater, C=-1 if less or equal)
		return (_mm_movemask_ps(v) & 0b0101) == 0b0101; //See that X,Y,Z are all less or equal
	}

	float SizeSq() const
	{
/*		__m128 v = _mm_loadu_ps( fa() );
		v = _mm_mul_ps( v, v);
		__m128 w = _mm_pshufd_ps( v, 0b10110001); //Y,X,W,Z
		w = _mm_add_ps( w, w); //Y+X, ..., Z+W, ...
		v = _mm_movehl_ps( v, w);
		v = _mm_add_ss( v, w);
		return v.m128_f32[0];*/
		return X*X+Y*Y+Z*Z;
	}

	float SizeXYSq() const
	{
		float size;
		__m128 v = *this;
		v = _mm_mul_ps( v, v);
		__m128 w = _mm_pshufd_ps( v, 0b10110001); //Y,X,W,Z
		w = _mm_add_ss( v, w);
		_mm_store_ss( &size, w);
		return size;
	}

	uint32 SignBits() //Get sign bits of every component
	{	return _mm_movemask_ps( *this );	}

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

	//Return a normal on 2 components
	Vector NormalXY() const
	{
		__m128 a = *this;
		__m128 z = _mm_setzero_ps();
		a = _mm_movelh_ps( a, z); //x,y,0,0
		__m128 b = _mm_mul_ps( a, a); //xx,yy,0,0
		z = _mm_pshufd_ps(b,0b11100001); //yy,xx,0,0
		b = _mm_add_ss( b, z); //c=xx+yy
		b = _newton_raphson_rsqrtss( b); //1/sqrt(c)
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

	//Transform by a normalized XY dir vector
	Vector TransformByXY( const Vector& Dir) const
	{
		__m128 org = *this;
		__m128 dir = Dir;
		__m128 y = Vector(1,1,-1,1);

		//result.x = org DOT dir
		//result.y = org DOT rotated_dir
		//result.z = result.z

		//Dir: X,Y
		//Rotated dir: -Y, X
		dir = _mm_pshufd_ps( dir, 0b00010100); //Force a PSHUFD (x,y,y,x)
		dir = _mm_mul_ps( dir, y); //x,y,-y,x

		__m128 opvec = _mm_movelh_ps( org, org); //Get X,Y,X,Y here
		opvec = _mm_mul_ps( opvec, dir); // ox*dx, oy*dy, ox*-dy, oy*dx
		opvec = _mm_castsi128_ps( _mm_shuffle_epi32( _mm_castps_si128(opvec), 0b11011000)); //Force another PSHUFD (x,z,y,w)
		y = _mm_movehl_ps( y, opvec);
		opvec = _mm_add_ps( opvec, y); //ox*dx+oy*dy, ox*-dy+oy*dx
		opvec = _mm_shuffle_ps( opvec, org, 0b11100100); //Mix X,Y of OPVEc and Z,W of ORG

		return opvec;
	}

	static const Integers MASK_3D;
	static const Integers MASK_SIGN;
	static const Integers MASK_ABS;
};
//**************

inline Vector Min( const Vector& A, const Vector& B)	{	return _mm_min_ps( A, B );	}
inline Vector Max( const Vector& A, const Vector& B)	{	return _mm_max_ps( A, B );	}
inline Vector Clamp( const Vector& V, const Vector& Min, const Vector& Max)		{	return _mm_min_ps( _mm_max_ps( V, Min), Max);	}

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
		:	Min( cg::Min(A,B))
		,	Max( cg::Max(A,B))
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

	//Give us one of the component vectors
	Vector& Vec( uint32 i)
	{	return (&Min)[i];	}

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

	bool IsZero() const
	{
		__m128 cmin, cmax;
		__m128 m = _mm_setzero_ps();
		cmin = _mm_cmpeq_ps( m, Min );
		cmax = _mm_cmpeq_ps( m, Max );
		m = _mm_cmpeq_ps( cmin, cmax );
		return _mm_movemask_ps( m) == 0b1111;
	}

	Vector CenterPoint() const
	{
		return (Min + Max) * 0.5;
	}

	bool Intersects( const Box& Other) const
	{
		return (Min <= Other.Max) & (Other.Min <= Max);
	}

	bool Contains( const Vector& Other) const
	{
		return (Min <= Other) & (Other <= Max);
	}

	//**************************
	//Transformations

	Box Expand( const Vector& Towards)
	{
		Min = cg::Min( Min, Towards);
		Max = cg::Max( Max, Towards);
		return *this;
	}

	Box Expand( const Box& By)
	{
		Min = cg::Min( Min, By.Min);
		Max = cg::Max( Max, By.Max);
		return *this;
	}

	Box Expand( const Box& By, ENoZero)
	{
		if ( !By.IsZero() )
		{
			if ( IsZero() )
				(*this = By);
			else
			{
				Min = cg::Min( Min, By.Min);
				Max = cg::Max( Max, By.Max);
			}
		}
		return *this;
	}

	//Enlarge a box by a 'Extent' vector
	void ExpandBounds( const Vector& By) 
	{
		Min -= By;
		Max += By;
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

