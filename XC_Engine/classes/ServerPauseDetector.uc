//=============================================================================
// ServerPauseDetector.
//=============================================================================
class ServerPauseDetector expands XC_Engine_Actor
	transient;

var string OldPauser;
	
event PostBeginPlay()
{
	NetPriority = Level.NetPriority;
	NetUpdateFrequency = Level.NetUpdateFrequency;
}
	
event Tick( float DeltaTime)
{
	if ( Level.Pauser != OldPauser )
	{
		OldPauser = Level.Pauser;
		Level.NetPriority = NetPriority * 2;
		Level.NetUpdateFrequency = 200;
		SetTimer( 0.5 * Level.TimeDilation, false);
	}
}

event Timer()
{
	Level.NetPriority = NetPriority;
	Level.NetUpdateFrequency = NetUpdateFrequency;
}


defaultproperties
{
	bAlwaysTick=True
}
