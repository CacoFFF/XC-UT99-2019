//=============================================================================
// EventModifierPath.
//
// Event Chain system's basic path modifier template
//
// Added as a proxy to a NavigationPoint in order to modify access to it.
// These modifiers are always one way routes.
//=============================================================================
class EventModifierPath expands LiftCenter
	abstract;
	
struct ReachSpec
{
	var() int Distance; 
	var() Actor Start;
	var() Actor End;
	var() int CollisionRadius; 
    var() int CollisionHeight; 
	var() int ReachFlags;
	var() byte bPruned;
};

var NavigationPoint TargetPath;

var EventLink OwnerEvent; //This is what controls this detractor
var EventLink EnablerEvent; //This the root required to enable reach the controller




function Setup( NavigationPoint InTargetPath, EventLink InOwnerEvent, EventLink InEnablerEvent)
{
	TargetPath = InTargetPath;
	OwnerEvent = InOwnerEvent;
	EnablerEvent = InEnablerEvent;
	class'XC_Engine_Actor'.static.LockToNavigationChain( self, true);
}

//It's likely that TargetPath has been destroyed
event BaseChange()
{
	if ( (TargetPath != None) && (Base != TargetPath) ) 
		Destroy();
}

event NeighborDestroyed( ReachSpec R)
{
//	Log("NeighborDestroyed:"@R.Start.Name@R.End.Name@R.Distance@R.ReachFlags);
}



defaultproperties
{
     bStatic=False
     bNoDelete=False
     RemoteRole=ROLE_None
     bSpecialCost=True
     bCollideWhenPlacing=False
     bCollideWorld=False
}
