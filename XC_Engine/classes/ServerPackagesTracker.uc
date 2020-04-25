//=============================================================================
// ServerPackagesTracker.
//=============================================================================
class ServerPackagesTracker expands XC_Engine_Actor
	native;

var() transient array<string> ServerPackages;
var() native array<string> DynamicPackages;

event PreBeginPlay()
{
	local int i;
	local ServerPackagesTracker OtherTracker;

	ForEach DynamicActors( class'ServerPackagesTracker', OtherTracker)
		if ( OtherTracker != self )
		{
			Destroy();
			return;
		}

	For ( i=0 ; i<DynamicPackages.Length ; i++ )
		AddToPackageMap( DynamicPackages[i] );
		
	SetTimer( 1.0, false); //Wait
}

// No use in keeping this actor
event Timer()
{
	if ( DynamicPackages.Length == 0 )
		Destroy();
}

//Does not set bScriptInitialized, can be intialized multiple times (needed for SaveGame)
event SetInitialState();
