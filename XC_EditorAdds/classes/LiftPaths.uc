//====================================================
// XC_Engine lift paths quick setup.
//====================================================
class LiftPaths expands BrushBuilder;

var() name LiftTag;

event bool Build()
{
	local LevelInfo LI;
	local Actor A;
	local NavigationPoint N;
	local Mover SelectedMover, M;
	local int LiftCenters;
	local int LiftExits;
	local name NewLiftTag;
	local string Output, CRLF;
	
	NewLiftTag = LiftTag;
	if ( NewLiftTag == '' )
		NewLiftTag = 'AutoDetect';
	
	ForEach class'XC_CoreStatics'.static.AllObjects( class'LevelInfo', LI)
		if ( !LI.bDeleteMe )
			break;
	
	ForEach LI.AllActors( class'Actor', A)
		if ( A.bSelected )
		{
			if ( LiftCenter(A) != None )
				LiftCenters++;
			else if ( LiftExit(A) != None )
				LiftExits++;
			else if ( Mover(A) != None )
			{
				if ( SelectedMover != None )
					return BadParameters("Error: more than one mover selected");
				SelectedMover = Mover(A);
			}
		}

	CRLF = Chr(13)$Chr(10);
	
	if ( LiftCenters + LiftExits == 0 )
	{
		return BadParameters(
			"Usage instructions:" $ CRLF $
			" - Select LiftCenter, LiftExit actors you want to link." $ CRLF $
			" - Select *one* Mover you want to attach the LiftCenter to (optional)" $ CRLF $
			" - You may modify the LiftTag by changing the 'AutoDetect' value to something else.");
	}
		
	if ( (SelectedMover == None) && (NewLiftTag == 'AutoDetect') )
		return BadParameters("Error: Select a Mover or specify a LiftTag.");
		
	if ( SelectedMover != None )
	{
		if ( NewLiftTag == 'AutoDetect' )
		{
			if ( SelectedMover.Tag == SelectedMover.Class.Name )
			{
				Output = Output $ CRLF $ " * Note:" @ SelectedMover.Name $ "'s Tag changed from" @ SelectedMover.Tag @ "to" @ SelectedMover.Name;
				NewLiftTag = SelectedMover.Name;
			}
			else
			{
				ForEach LI.AllActors( class'Mover', M, SelectedMover.Tag)
					if ( M != SelectedMover )
					{
						if ( Output != "" )
							Output = Output $ ", ";
						Output = Output $ M.Name;
					}
				if ( Output != "" )
					return BadParameters("Error: Movers" @ Output @ "have same Tag as" @ SelectedMover.Name);
				NewLiftTag = SelectedMover.Tag;
			}
		}
	}
	
	if ( LiftExits < 2 )
		Output = Output $ CRLF $ "Note: less than two LiftExits selected.";
	if ( LiftCenters == 0 )
		Output = Output $ CRLF $ "Note: no LiftCenter selected.";
	
	SelectedMover.Tag = NewLiftTag;
	ForEach LI.AllActors( class'NavigationPoint', N)
		if ( N.bSelected )
		{
			if ( LiftCenter(N) != None )
				LiftCenter(N).LiftTag = NewLiftTag;
			else if ( LiftExit(N) != None )
				LiftExit(N).LiftTag = NewLiftTag;
		}
		
	Output = "(" $ NewLiftTag $ ")" $ Output;
	if ( SelectedMover != None )
		Output = "to" @ SelectedMover.Name @ Output;
	return BadParameters( "Linked" @ LiftCenters @ "LiftCenter and" @ LiftExits @ "LiftExit" @ Output);
}

defaultproperties
{
	ToolTip="Setup Lift Paths"
	BitmapFilename="BBLiftPaths"
	LiftTag=AutoDetect
}
