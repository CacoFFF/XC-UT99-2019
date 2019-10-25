
#include "XC_Core.h"
#include "UnXC_Math.h"
#include "Cacus/Math/Constants.h"


static CFVector4 WMinusOne( 0, 0, 0, -1.f);

// Does PlaneDot on both Start and End on the same plane (Unaligned plane)
// Dist must be a FLOAT[2] array
void DoublePlaneDot( const FPlane& Plane, const CFVector4& Start, const CFVector4& End, FLOAT* Dist2)
{
	CFVector4 VPlane( &Plane.X);
	CFVector4 VStart = _mm_or_ps( _mm_and_ps( Start, CIVector4::MASK_3D), WMinusOne); // Set W to -1
	CFVector4 VEnd   = _mm_or_ps( _mm_and_ps( End  , CIVector4::MASK_3D), WMinusOne); // ORPS has very low latency and CPI
	Dist2[0] = VStart | VPlane;
	Dist2[1] = VEnd   | VPlane;
}



//Obtains intersection using distances to plane as alpha (optimal for traces)
//Use DoublePlaneDot to obtain the Dist array
CFVector4 LinePlaneIntersectDist( const CFVector4& Start, const CFVector4& End, FLOAT* Dist2)
{
	float Alpha = Dist2[0] / (Dist2[1] - Dist2[0]);
	CFVector4 Middle = Start - (End - Start) * Alpha;
	return Middle;
}
