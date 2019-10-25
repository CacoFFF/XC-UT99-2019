/*=============================================================================
	UnXC_Math.h: XC_Core shared FPU/SSE math library
=============================================================================*/

#ifndef INC_XC_MATH
#define INC_XC_MATH

#include "Cacus/Math/Math.h"

// Does PlaneDot on both Start and End on the same plane (Unaligned plane)
// Dist must be a FLOAT[2] array
XC_CORE_API void DoublePlaneDot( const FPlane& Plane, const CFVector4& Start, const CFVector4& End, FLOAT* Dist2);

//Obtains intersection using distances to plane as alpha (optimal for traces)
//Use DoublePlaneDot to obtain the Dist array
XC_CORE_API CFVector4 LinePlaneIntersectDist( const CFVector4& Start, const CFVector4& End, FLOAT* Dist2);


/** This instruction copies the first element of an array onto the xmm0 register once populated
movss xmm0, [a]
shufps xmm0, xmm0, 0
(generates a,a,a,a )
*/


//Use unaligned loading
enum EUnsafe   {E_Unsafe  = 1};

#define appInvSqrt(a) _appInvSqrt(a)


/**
  a	%eax
  b 	%ebx
  c 	%ecx
  d 	%edx
  S	%esi
  D	%edi
*/


//Substracts origin, need version without it
inline FVector TransformPointByXY( const FCoords &Coords, const FVector& Point )
{
	FVector Temp = Point - Coords.Origin;
	return FVector(	Temp | Coords.XAxis, Temp | Coords.YAxis, Temp.Z);
}


inline FLOAT _appInvSqrt( FLOAT F )
{
#if ASM
	const FLOAT fThree = 3.0f;
	const FLOAT fOneHalf = 0.5f;
	FLOAT temp;

	__asm
	{
		movss	xmm1,[F]
		rsqrtss	xmm0,xmm1			// 1/sqrt estimate (12 bits)

		// Newton-Raphson iteration (X1 = 0.5*X0*(3-(Y*X0)*X0))
		movss	xmm3,[fThree]
		movss	xmm2,xmm0
		mulss	xmm0,xmm1			// Y*X0
		mulss	xmm0,xmm2			// Y*X0*X0
		mulss	xmm2,[fOneHalf]		// 0.5*X0
		subss	xmm3,xmm0			// 3-Y*X0*X0
		mulss	xmm3,xmm2			// 0.5*X0*(3-Y*X0*X0)
		movss	[temp],xmm3
	}
	return temp;
#elif ASMLINUX
	//Higor: finally got this working on GCC 2.95
	const FLOAT fThree = 3.0f;
	const FLOAT fOneHalf = 0.5f;
	FLOAT temp;

	__asm__ __volatile__("movss    %0,%%xmm1\n"
			"rsqrtss    %%xmm1,%%xmm0\n"
			"movss    %1,%%xmm3\n"
			"movss    %%xmm0,%%xmm2\n"
			"mulss    %%xmm1,%%xmm0\n"
			"mulss    %%xmm2,%%xmm0\n"
			"mulss    %2,%%xmm2\n"
			"subss    %%xmm0,%%xmm3\n"
			"mulss    %%xmm2,%%xmm3\n" : : "m" (F), "m" (fThree), "m" (fOneHalf) );
	__asm__ __volatile__("movss    %%xmm3,%0\n" : "=m" (temp) );
	return temp;
#else
	return 1.0f / appSqrt( F);
#endif
}


inline FVector _UnsafeNormal( const FVector& V)
{
	const FLOAT Scale = _appInvSqrt(V.X*V.X+V.Y*V.Y+V.Z*V.Z);
	return FVector( V.X*Scale, V.Y*Scale, V.Z*Scale );
}

inline FVector _UnsafeNormal2D( const FVector& V)
{
	const FLOAT Scale = _appInvSqrt(V.X*V.X+V.Y*V.Y);
	return FVector( V.X*Scale, V.Y*Scale, 0.f);
}

inline FVector _UnsafeNormal2D( const FLOAT& X, const FLOAT& Y)
{
	const FLOAT Scale = _appInvSqrt(X*X+Y*Y);
	return FVector( X*Scale, Y*Scale, 0.f);
}


///////////////////////////////////////////////////////////////////////
// SSE Vector 4 math
///////////////////////////////////////////////////////////////////////


#pragma warning (push)
#pragma warning (disable : 4035)
#pragma warning (disable : 4715)
//Checks if a given point is within a triangle ABC, make sure all have same W.
inline UBOOL PointInTriangle( const CFVector4* P, const CFVector4* A, const CFVector4* B, const CFVector4* C)
{
#if ASM
	const FLOAT fZero = 0.f;
	const FLOAT fOne = 1.f;
	__asm
	{
		mov      eax,[A]
		mov      ecx,[B]
		mov      edx,[C]
		mov      edi,[P]
		movaps   xmm0,[eax]		//x0: A
		movaps   xmm1,[ecx]		//x1: B
		movaps   xmm2,[edi]		//x2: P
		movaps   xmm3,[edx]		//x3: C
		subps    xmm1,xmm0		//x1: B-A (v1)
		subps    xmm2,xmm0		//x2: P-A (v2)
		subps    xmm3,xmm0		//x3: C-A (v0)
		//Smart calculation of v*v while eliminating unused variables at the same time
		movaps   xmm0,xmm3		//x0: v0
		movaps   xmm4,xmm2		//x4: v2
		mulps    xmm2,xmm0		//x2: v0*v2
		mulps    xmm4,xmm1		//x4: v1*v2
		movaps   xmm3,xmm1		//x3: v1
		mulps    xmm3,xmm3		//x3: v1*v1
		mulps    xmm1,xmm0		//x1: v0*v1
		mulps    xmm0,xmm0		//x0: v0*v0
		
		//Sum all scalars in registers (x0-x4) using x5 temp
		pshufd   xmm5,xmm0,49
		addps    xmm0,xmm5
		pshufd   xmm5,xmm0,2
		addss    xmm0,xmm5		//x0: Dot(v0,v0)
		pshufd   xmm5,xmm1,49
		addps    xmm1,xmm5
		pshufd   xmm5,xmm1,2
		addss    xmm1,xmm5		//x1: Dot(v0,v1)
		pshufd   xmm5,xmm2,49
		addps    xmm2,xmm5
		pshufd   xmm5,xmm2,2
		addss    xmm2,xmm5		//x2: Dot(v0,v2)
		pshufd   xmm5,xmm3,49
		addps    xmm3,xmm5
		pshufd   xmm5,xmm3,2
		addss    xmm3,xmm5		//x3: Dot(v1,v1)
		pshufd   xmm5,xmm4,49
		addps    xmm4,xmm5
		pshufd   xmm5,xmm4,2
		addss    xmm4,xmm5		//x4: Dot(v1,v2)

		movss    xmm5,xmm0
		movss    xmm6,xmm1
		mulss    xmm5,xmm3		//x5: dot00*dot11
		mulss    xmm6,xmm6		//x6: dot01*dot01
		subss    xmm5,xmm6		//x5: dot00*dot11 - dot01*dot01
		rcpss    xmm5,xmm5		//x5: invdenom (1/x5)
		mulss    xmm3,xmm2		//x3: dot11*dot02 (dot11 not used anymore)
		mulss    xmm2,xmm1		//x2: dot01*dot02 (dot02 not used anymore)
		mulss    xmm1,xmm4		//x1: dot01*dot12 (dot01 not used anymore)
		mulss    xmm4,xmm0		//x4: dot00*dot12 (dot00 and dot12 not used anymore)
		subss    xmm3,xmm1		//x3: dot11*dot02 - dot01*dot12
		subss    xmm4,xmm2		//x4: dot00*dot12 - dot01*dot02
		mulss    xmm3,xmm5		//x3: u
		mulss    xmm4,xmm5		//x4: v
		movss    xmm5,xmm4
		addss    xmm5,xmm3		//x5: u+v
		
		cmpss    xmm5,fOne,5	//x5: !(u+v < 1)
		cmpss    xmm3,fZero,1	//x3: (u < 0)
		cmpss    xmm4,fZero,1	//x4: (v < 0)
		//Positive results mean -1.#QNAN (0xFFFFFFFF)
		//Since we're doing reverse evaluation, we expect 0x00000000 on all 3 results
		cmpss    xmm5,xmm4,3	//x5: (x5 == NaN || x4 == Nan) >>> 0x00000000 if no NaN's
		cmpss    xmm5,xmm3,7	//x5: (x5 != NaN && x3 != Nan) >>> 0xFFFFFFFF if no NaN's
		movd     eax,xmm5		//Return value
	}
#else
	FVector v0( C->X - A->X, C->Y - A->Y, C->Z - A->Z);
	FVector v1( B->X - A->X, B->Y - A->Y, B->Z - A->Z);
	FVector v2( P->X - A->X, P->Y - A->Y, P->Z - A->Z);
	FLOAT dot00 = v0 | v0;
	FLOAT dot01 = v0 | v1;
	FLOAT dot02 = v0 | v2;
	FLOAT dot11 = v1 | v1;
	FLOAT dot12 = v1 | v2;
	FLOAT invDenom = 1 / (dot00 * dot11 - dot01 * dot01);
	FLOAT u = (dot11 * dot02 - dot01 * dot12) * invDenom;
	FLOAT v = (dot00 * dot12 - dot01 * dot02) * invDenom;
	return (u >= 0) && (v >= 0) && (u + v < 1);
#endif
}
#pragma warning (pop)


// Measure a line using a Dir vector, all data needs to be 16-aligned
// Make sure Dir.W = 0, or Start.W = End.W
inline FLOAT LengthUsingDirA( const CFVector4* Start, const CFVector4* End, const CFVector4* Dir)
{
	FLOAT MaxDist;
#if ASM
	__asm
	{
		mov      eax,[Start]
		mov      ecx,[End]
		mov      edx,[Dir]
		movaps   xmm0,[eax]
		movaps   xmm1,[ecx]
		movaps   xmm2,[edx]
		subps    xmm1,xmm0
		mulps    xmm1,xmm2
		//Sum all scalars in register x1 (using x2 temp)
		pshufd   xmm2,xmm1,49 // 1->0, 3->2 ...0b00110001 | 0x31
		addps    xmm1,xmm2 // 0+1, xx, 2+3, xx
		pshufd   xmm2,xmm1,2 // 2->0 ...0b00000010 | 0x02
		addss    xmm1,xmm2
		movss    MaxDist,xmm1
	}
/*#elif ASMLINUX
	__asm__ __volatile__("movaps    (%%eax),%%xmm0 \n"
						"movaps     (%%ecx),%%xmm1 \n"
						"movaps     (%%edx),%%xmm2 \n"
						"subps       %%xmm0,%%xmm1 \n"
						"mulps       %%xmm2,%%xmm1 \n"
						"pshufd  $49,%%xmm1,%%xmm2 \n"
						"addps       %%xmm2,%%xmm1 \n"
						"pshufd   $2,%%xmm1,%%xmm2 \n"
						"addss       %%xmm2,%%xmm1 \n" : : "a" (Start), "c" (End), "d" (Dir) : "memory"	);
	__asm__ __volatile__("movss      %%xmm1,%0\n" : "=m" (MaxDist) );*/
#else
	MaxDist = (End->X-Start->X)*Dir->X
			+ (End->Y-Start->Y)*Dir->Y
			+ (End->Z-Start->Z)*Dir->Z;
#endif
	return MaxDist;
}

// Measure a line using a Dir vector, all data needs to be 16-aligned (except Dir)
// Make sure Dir.W = 0, or Start.W = End.W
inline FLOAT LengthUsingDir2D( const CFVector4* Start, const CFVector4* End, const FVector* Dir)
{
	FLOAT MaxDist;
#if ASM
	__asm
	{
		mov      eax,[Start]
		mov      ecx,[End]
		mov      edx,[Dir]
		movaps   xmm0,[eax]
		movaps   xmm1,[ecx]
		movlps   xmm2,[edx]
		subps    xmm1,xmm0
		mulps    xmm1,xmm2
		//Sum low 2 scalars in register x1 (using x2 temp)
		pshufd   xmm2,xmm1,1 // 1->0 ...0b00000001 | 0x01
		addss    xmm1,xmm2 // 0+1
		movss    MaxDist,xmm1
	}
#else
	MaxDist = (End->X-Start->X)*Dir->X
			+ (End->Y-Start->Y)*Dir->Y;
#endif
	return MaxDist;
}

//Use this overload with care
inline FLOAT LengthUsingDir2D( const CFVector4* Start, const CFVector4* End, const FLOAT* Dir)
{
	return LengthUsingDir2D( Start, End, (FVector*)Dir);
}




//Construct a FVector4 plane using SSE instructions
inline void SSE_MakeFPlaneA( CFVector4* A, CFVector4* B, CFVector4* C, CFVector4* Plane)
{
	//TODO: PORT TO INTRINSICS
#if ASM
/*	const FLOAT AvoidUnsafe = 0.0001f;
	const FLOAT fThree = 3.0f;
	const FLOAT fOneHalf = 0.5f;
	__asm
	{
		mov      eax,[A]
		mov      ecx,[B]
		mov      edx,[C]
		movaps   xmm0,[eax]		//x0: A
		movaps   xmm1,[ecx]		//x1: B
		movaps   xmm2,[edx]		//x2: C
		subps    xmm1,xmm0		//x1: B-A
		subps    xmm2,xmm0		//x2: C-A
		movaps   xmm3,xmm1		//x3: x1 copy
		movaps   xmm4,xmm2		//x4: x2 copy

		shufps   xmm1,xmm1,0xD8	// 11 01 10 00  Flip the middle elements of x1
		shufps   xmm2,xmm2,0xE1	// 11 10 00 01  Flip first two elements of x2
		mulps    xmm1,xmm2		//x1: First part of cross product
		shufps   xmm3,xmm3,0xE1	// 11 10 00 01  Flip first two elements of the x1 copy
		shufps   xmm4,xmm4,0xD8	// 11 01 10 00  Flip the middle elements of the x2 copy
		mulps    xmm3,xmm4		//x3: Substract part of cross product
              
		subps    xmm1,xmm3		//x1: (B-A)^(C-A)
		andps    xmm1,FVector3Mask //x1: Zero 4th coord
		
		//Debug:
		shufps   xmm1,xmm1,0xC6	//x1: Swap X and Z (bad shuffles above, fixing here)
		
		//Normalize:
		movaps   xmm2,xmm1		//x2: x1 copy
		mulps    xmm2,xmm1		//x2: x1 squared coordinates
			//Sum all scalars in register x2 (using x3 temp)
		pshufd   xmm3,xmm2,49	// 1->0, 3->2 ...0b00110001 | 0x31
		addps    xmm2,xmm3		// 0+1, xx, 2+3, xx
		movhlps  xmm3,xmm2		// 2,3 -> 0,1
//		pshufd   xmm3,xmm2,2	// 2->0 ...0b00000010 | 0x02
		addps    xmm2,xmm3		//x2: VSizeSQ( (B-A)^(C-A) )
		addss    xmm2,AvoidUnsafe //Make normalization safe

		rsqrtss  xmm5,xmm2		//x5: 1/VSize( (B-A)^(C-A) ) -> low prec
		
		// Newton-Raphson iteration (X1 = 0.5*X0*(3-(Y*X0)*X0))
		movss    xmm3,fThree
		movss    xmm4,xmm5
		mulss    xmm5,xmm2
		mulss    xmm5,xmm4
		mulss    xmm4,fOneHalf
		subss    xmm3,xmm5
		mulss    xmm3,xmm4
		movss    xmm2,xmm3 //x2: high prec now
		//movss    xmm2,xmm5

		shufps   xmm2,xmm2,0	//x2: Populate all DWords in the register
		mulps    xmm1,xmm2		//x1: Normalize cross product
		mov      eax,[Plane]	//EAX: plane address
		movaps   [eax],xmm1     //Store the plane normal
		mulps    xmm0,xmm1		//x0: (A * CrossNorm)

			//Sum all scalars in register x0 (using x3 temp)
		pshufd   xmm3,xmm0,49	// 1->0, 3->2 ...0b00110001 | 0x31
		addps    xmm0,xmm3		// 0+1, xx, 2+3, xx
		movhlps  xmm3,xmm0		// 2,3 -> 0,1
//		pshufd   xmm3,xmm0,2	// 2->0 ...0b00000010 | 0x02
		addps    xmm0,xmm3		//x0: (A dot CrossNorm)
		add      eax,12			//EAX: Plane.W address
		movss    [eax],xmm0
	}*/
#else
	*(FPlane*)Plane = FPlane( *(FVector*)A,*(FVector*)B,*(FVector*)C);
#endif
}

/** Linux constraints
  a	%eax
  b	%ebx
  c	%ecx
  d	%edx
  S	%esi
  D	%edi
*/

#endif