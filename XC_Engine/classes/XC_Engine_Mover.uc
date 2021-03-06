class XC_Engine_Mover expands Mover;

function TC_Trigger( actor Other, pawn EventInstigator )
{
	numTriggerEvents++;
	SavedTrigger = Other;
	Instigator = EventInstigator;
	if ( SavedTrigger != None )
		SavedTrigger.BeginEvent();
	if ( numTriggerEvents == 1 ) //Prevent multiple interpolation starts
		GotoState( 'TriggerControl', 'Open' );
}



// Stores Mover.InterpolateTo original code here
final function InterpolateTo_Org( byte NewKeyNum, float Seconds )
{
}


// Hacked version of Mover.InterpolateTo
// Fixes the 'Seconds' parameter so that movers don't badly
// desync in net games
final function InterpolateTo_MPFix( byte NewKeyNum, float Seconds )
{
	if ( Seconds > 0 ) //Do not alter instant-movement movers
	{
		Seconds = int(100.0 * FMax(0.01, (1.0 / FMax(Seconds, 0.005))));
		Seconds = 1.0 / (Seconds * 0.01);
	}
	InterpolateTo_Org( NewKeyNum, Seconds);
}

final function FinishedClosing_Test()
{
	// Update sound effects.
	PlaySound( ClosedSound, SLOT_None );

	// Notify our triggering actor that we have completed.
	if( SavedTrigger != None )
		SavedTrigger.EndEvent();
	SavedTrigger = None;
	Instigator = None;
	FinishNotify();

	//v469: Force extra update to make sure clients see this mover is closed
	if ( KeyNum != PrevKeyNum )	
	{
		SimInterpolate.X = 100 * PhysAlpha;
		Log("TEST!");
	}
}