class ElevatorReachspecModifier expands EventLink;

var NavigationPoint Path;
var Mover Lift;

struct ReachSpecCache
{
	var int Index;
	var int KeyFrame;
	var vector MarkerPos;
};

var ReachSpecCache InboundReachSpecs[16];
var ReachSpecCache OutboundReachSpecs[16];
var int Inbound, Outbound;


function Setup( NavigationPoint InPath, Mover InLift)
{
	local Actor Start, End;
	local int ReachSpecIdx, i;
	local int ReachFlags, Distance;
	local vector MarkerPos;
	local NavigationPoint OldMarker;
	
	Path = InPath;
	Lift = InLift;
	
	OldMarker = Lift.myMarker;
	Lift.myMarker = InPath;
	
	ForEach ConnectedDests( Path, End, ReachSpecIdx, i)
	{
		if ( NavigationPoint(End) != None )
		{
			OutboundReachSpecs[Outbound].Index = ReachSpecIdx;
			OutboundReachSpecs[Outbound].KeyFrame = XCS.static.NearestMoverKeyframe( Lift, End.Location, MarkerPos);
			OutboundReachSpecs[Outbound].MarkerPos = MarkerPos;
			Outbound++;
		}
	}
	
	For ( i=0 ; i<16 && Path.upstreamPaths[i]>=0 ; i++ )
	{
		describeSpec( Path.upstreamPaths[i], Start, End, ReachFlags, Distance); 
		if ( (End == Path) && (NavigationPoint(Start) != None) )
		{
			InboundReachSpecs[Inbound].Index = Path.upstreamPaths[i];
			InboundReachSpecs[Inbound].KeyFrame = XCS.static.NearestMoverKeyframe( Lift, Start.Location, MarkerPos);
			InboundReachSpecs[Inbound].MarkerPos = MarkerPos;
			Inbound++;
		}
	}
	
	Lift.myMarker = OldMarker;
	SetTimer( 0.1 + InLift.MoveTime * 0.1, true);
}

// Do not disrupt timer
event Trigger( Actor Other, Pawn EventInstigator)
{
}

event Timer()
{
	local float AccumulatedTime;
	local float Mult;
	local int i, Keys, KeyNum;
	local float ReachTimes[8]; //Times to reach each keyframe based on current
	local bool bUp;
	local ReachSpec Spec;
	
	KeyNum = Lift.KeyNum; //Become INT
	bUp = (Lift.PrevKeyNum < KeyNum) || (KeyNum == 0 && !Lift.bInterpolating);
	
	//Calc arrival time of target keyframe
	if ( Lift.bInterpolating )
		ReachTimes[KeyNum] = (1.0 - Lift.PhysAlpha) * Lift.MoveTime;
	AccumulatedTime = ReachTimes[KeyNum];
	
	//Calc times for keyframes in my direction
	Keys = Min( 8, Lift.NumKeys);
	if ( bUp )
	{
		For ( i=KeyNum+1 ; i<Keys ; i++ )
		{
			ReachTimes[i] = Lift.MoveTime + AccumulatedTime;
			AccumulatedTime = ReachTimes[i];
		}
		AccumulatedTime *= 2; //Needs to go back thru same keys we're about to hit (approx)
		For ( i=KeyNum-1 ; i>=0 ; i-- )
		{
			ReachTimes[i] = Lift.MoveTime + AccumulatedTime;
			AccumulatedTime = ReachTimes[i];
		}
	}
	else
	{
		For ( i=KeyNum-1 ; i>=0 ; i-- )
		{
			ReachTimes[i] = Lift.MoveTime + AccumulatedTime;
			AccumulatedTime = ReachTimes[i];
		}
		AccumulatedTime *= 2; //Needs to go back thru same keys we're about to hit (approx)
		For ( i=KeyNum+1 ; i<Keys ; i++ )
		{
			ReachTimes[i] = Lift.MoveTime + AccumulatedTime;
			AccumulatedTime = ReachTimes[i];
		}
	}

	For ( i=0 ; i<Inbound ; i++ )
		if ( GetReachSpec( Spec, InboundReachSpecs[i].Index) )
		{
			Mult = 1;
			if ( InboundReachSpecs[i].MarkerPos.Z - 50 > Path.Location.Z ) //User needs to jump down
			{
				Mult += (HSize(Path.Location - InboundReachSpecs[i].MarkerPos) / 200)
				- XCS.static.Phys_FreeFallVelocity( Path.Location.Z-InboundReachSpecs[i].MarkerPos.Z, Path.Region.Zone.ZoneGravity.Z) / 800;
				Spec.Distance = int( 50.0 * ReachTimes[InboundReachSpecs[i].KeyFrame] * Mult);
			}
			else
				Spec.Distance = int( 200.0 * ReachTimes[InboundReachSpecs[i].KeyFrame]);
			Spec.Distance += int( VSize( Spec.Start.Location - InboundReachSpecs[i].MarkerPos) * Mult); //SHOULD BE CACHED AS BASE COST
			Spec.Distance = Max( Spec.Distance, 1);
			SetReachSpec( Spec, InboundReachSpecs[i].Index, false);
		}
		
/*	For ( i=0 ; i<Outbound ; i++ )
		if ( GetReachSpec( Spec, OutboundReachSpecs[i].Index) )
		{
			Spec.Distance = 100 + int( 50.0 * ReachTimes[OutboundReachSpecs[i].KeyFrame]);
			SetReachSpec( Spec, OutboundReachSpecs[i].Index, false);
		}*/
}
