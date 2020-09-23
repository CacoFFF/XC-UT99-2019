//====================================================
// XC_Engine actor transform.
//====================================================
class ActorTransform expands BrushBuilder;

enum EActorTransform
{
	AT_MIRROR_X,
	AT_MIRROR_Y,
	AT_MIRROR_Z,
	AT_ROTATE,
	AT_SCALE,
	AT_SCALE_3D
};

struct TransformInfo
{
	var() EActorTransform TransformType;
	var() rotator Rotate;
	var() vector Scale3D;
	var() float Scale;
};

var() array<TransformInfo> TransformSteps;
var() float OriginGridSnap;
var() bool bScaleCollision;
var() bool bScaleDrawScale;
var() bool bScaleLightRadius;
var() bool bOriginAtZero;

event bool Build()
{
	local LevelInfo LI;
	local Actor A;
	local string Output, CRLF;
	local int i, ActorCount;
	
	local vector VMin, VMax, CenterPoint;

	local vector NewLocation;
	local rotator NewRotation;
	
	ForEach class'XC_CoreStatics'.static.AllObjects( class'LevelInfo', LI)
		if ( !LI.bDeleteMe )
			break;
	
	i = 0;
	ForEach LI.AllActors( class'Actor', A)
		if ( A.bSelected && (A != LI) )
		{
			if ( !bOriginAtZero )
			{
				if ( i == 0 )
				{
					VMin = A.Location;
					VMax = A.Location;
				}
				else
				{
					VMin.X = FMin( A.Location.X, VMin.X);
					VMin.Y = FMin( A.Location.Y, VMin.Y);
					VMin.Z = FMin( A.Location.Z, VMin.Z);
					VMax.X = FMax( A.Location.X, VMax.X);
					VMax.Y = FMax( A.Location.Y, VMax.Y);
					VMax.Z = FMax( A.Location.Z, VMax.Z);
				}
			}
			i++;
		}
		
	CRLF = Chr(13)$Chr(10);
	if ( (i == 0) || (TransformSteps.Length == 0) )
	{
		return BadParameters( 
			"Usage:" $ CRLF $
			" - Select the actors you want to transform." $ CRLF $
			" - Right click this button to open the setup dialog." $ CRLF $
			" - Add new transform steps." $ CRLF $
			CRLF $
			"Notes:" $ CRLF $
			" * Collision, DrawScale, LightRadius are only affected by AT_SCALE." $ CRLF $
			" * The origin of the transformation is snapped to [OriginGridSnap] grid." $ CRLF );
	}
	
	ActorCount = i;
	CenterPoint = (VMin + VMax) / 2;
	if ( OriginGridSnap > 0 )
	{
		CenterPoint.X = int(CenterPoint.X / OriginGridSnap) * OriginGridSnap;
		CenterPoint.Y = int(CenterPoint.Y / OriginGridSnap) * OriginGridSnap;
		CenterPoint.Z = int(CenterPoint.Z / OriginGridSnap) * OriginGridSnap;
	}
	
	for ( i=0 ; i<TransformSteps.Length ; i++ )
	{
		ForEach LI.AllActors( class'Actor', A)
			if ( A.bSelected && (A != LI) )
			{
				NewLocation = A.Location;
				NewRotation = A.Rotation;

				// Mirror by X axis
				if ( TransformSteps[i].TransformType == AT_MIRROR_X )
				{
					NewLocation.X += (CenterPoint.X - NewLocation.X) * 2;
					if ( Brush(A) != None )
					{
						if ( Brush(A).PostScale.Scale == vect(1,1,1) )
							Brush(A).MainScale.Scale.X *= -1;
						else
							Brush(A).PostScale.Scale.X *= -1;
					}
					else
					{
						NewRotation.Yaw += (16384 - NewRotation.Yaw) * 2;
						NewRotation.Roll *= -1;
					}
					A.RotationRate.Yaw *= -1;
				}
				// Mirror by Y axis
				else if ( TransformSteps[i].TransformType == AT_MIRROR_Y )
				{
					NewLocation.Y += (CenterPoint.Y - NewLocation.Y) * 2;
					if ( Brush(A) != None )
					{
						if ( Brush(A).PostScale.Scale == vect(1,1,1) )
							Brush(A).MainScale.Scale.Y *= -1;
						else
							Brush(A).PostScale.Scale.Y *= -1;
					}
					else
					{
						NewRotation.Yaw *= -1;
						NewRotation.Roll *= -1;
					}
					A.RotationRate.Yaw *= -1;
				}
				// Mirror by Z axis
				else if ( TransformSteps[i].TransformType == AT_MIRROR_Z )
				{
					NewLocation.Z += (CenterPoint.Z - NewLocation.Z) * 2;
					if ( Brush(A) != None )
					{
						if ( Brush(A).PostScale.Scale == vect(1,1,1) )
							Brush(A).MainScale.Scale.Z *= -1;
						else
							Brush(A).PostScale.Scale.Z *= -1;
					}
					else
					{
						NewRotation.Pitch *= -1;
						NewRotation.Roll += (16384 - NewRotation.Roll) * 2;
					}
					A.RotationRate.Roll *= -1;
				}
				// Rotate
				else if ( TransformSteps[i].TransformType == AT_ROTATE )
				{
					NewLocation = CenterPoint + (NewLocation - CenterPoint) >> TransformSteps[i].Rotate;
					NewRotation += TransformSteps[i].Rotate;
				}
				// Scale
				else if ( TransformSteps[i].TransformType == AT_SCALE )
				{
					NewLocation = CenterPoint + (NewLocation - CenterPoint) * TransformSteps[i].Scale;
					if ( Brush(A) != None )
						Brush(A).MainScale.Scale *= TransformSteps[i].Scale;
					if ( bScaleCollision && !A.bIsPawn )
						A.SetCollisionSize( A.CollisionRadius * TransformSteps[i].Scale, A.CollisionHeight * TransformSteps[i].Scale);
					if ( bScaleDrawScale )
						A.DrawScale *= TransformSteps[i].Scale;
					if ( bScaleLightRadius )
						A.LightRadius = byte(FClamp( float(A.LightRadius) * TransformSteps[i].Scale, 0, 255));
				}
				// Scale3D
				else if ( TransformSteps[i].TransformType == AT_SCALE_3D )
				{
					NewLocation = CenterPoint + (NewLocation - CenterPoint) * TransformSteps[i].Scale3D;
					if ( Brush(A) != None )
						Brush(A).PostScale.Scale *= TransformSteps[i].Scale3D;
				}
				
				A.SetLocation(NewLocation);
				A.SetRotation(NewRotation);
			}
	}
	

	return BadParameters("Applied"@TransformSteps.Length@"transformations on"@ActorCount@"actors.");
}

defaultproperties
{
	ToolTip="Actor Transform"
	BitmapFilename="BBActorMirror"
	OriginGridSnap=16
}
