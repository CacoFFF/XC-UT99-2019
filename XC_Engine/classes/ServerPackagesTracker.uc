//=============================================================================
// ServerPackagesTracker.
//=============================================================================
class ServerPackagesTracker expands XC_Engine_Actor
	native;

var() transient array<string> ServerPackages;
var() native array<string> DynamicPackages;

event XC_Init()
{
	local ServerPackagesTracker OtherTracker;

	ForEach DynamicActors( class'ServerPackagesTracker', OtherTracker)
		if ( OtherTracker != self )
		{
			Destroy();
			OtherTracker.RestoreDynamicPackages();
			return;
		}
}

function RestoreDynamicPackages()
{
	local int i;

	For ( i=0; i<DynamicPackages.Length; i++)
	{
		Log("Restoring old Dynamic Package:"@DynamicPackages[i]);
		AddToPackageMap( DynamicPackages[i] );
	}
}
