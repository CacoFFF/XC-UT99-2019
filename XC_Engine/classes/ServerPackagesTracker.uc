//=============================================================================
// ServerPackagesTracker.
//=============================================================================
class ServerPackagesTracker expands XC_Engine_Actor
	native;

var() transient array<string> ServerPackages;
var() native array<string> DynamicPackages;

event PreBeginPlay()
{
	local int i, Count;
	local ServerPackagesTracker OtherTracker;

	ForEach DynamicActors( class'ServerPackagesTracker', OtherTracker)
		if ( OtherTracker != self )
		{
			Destroy();
			return;
		}

	Count = Array_Length(DynamicPackages);
	For ( i=0 ; i<Count ; i++ )
		AddToPackageMap( DynamicPackages[i] );
}

//Does not set bScriptInitialized, can be intialized multiple times (needed for SaveGame)
event SetInitialState();
