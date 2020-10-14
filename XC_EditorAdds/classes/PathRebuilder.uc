//====================================================
// XC_Engine path rebuilder shortcut
//====================================================
class PathRebuilder expands BrushBuilder;

var() name ReferenceTag;
var() bool bBuildAir;
var() bool bBuildSelected;
var() float MaxDistance;


event bool Build()
{
	local LevelInfo LI;
	local class<Actor> AC;
	local Pawn P, ScoutReference;
	local int BuildFlags;
	
	ForEach class'XC_CoreStatics'.static.AllObjects( class'LevelInfo', LI)
		if ( !LI.bDeleteMe )
			break;
	
	if ( ReferenceTag != '' )
		ForEach LI.AllActors( class'Pawn', P, ReferenceTag)
		{
			ScoutReference = P;
			break;
		}
	
	if ( bBuildAir )
		BuildFlags += 1;
	if ( bBuildSelected )
		BuildFlags += 2;
	
	return BadParameters( class'XC_CoreStatics'.static.PathsRebuild( LI.XLevel, ScoutReference, BuildFlags, MaxDistance));
}

defaultproperties
{
	ToolTip="Path rebuilder [XC]"
	BitmapFilename="BBPathRebuilder"
	ReferenceTag=PathRebuilder
	MaxDistance=2000
}
