//=============================================================================
// EventLink
//
// Rules:
// - I point to a single actor (Owner).
// - Owner must be set via 'Spawn'
// - My Tag is always equal to Owner's.
// - I do not exist without Owner
//=============================================================================
class EventLink expands EventChainSystem;

var() string EventList;

var EventChainHandler Handler;
var EventLink NextEvent;
var EventChainTriggerNotify NotifyList;
var array<EventAttractorPath> Attractors;
var array<EventDetractorPath> Detractors;
var int AnalysisDepth;
var int QueryTag;
var array<EventLink> RootEvents;
var XC_NavigationPoint AIMarker;

var bool bStaticCleanup;
var bool bDestroying;
var bool bCleanupPending;
var() bool bRoot; //Can emit 'Trigger' notifications by direct action
var() bool bRootEnabled;
var() bool bLink; //Can receive 'Trigger' notifications
var() bool bLinkEnabled;
var() bool bInProgress; //Is in the middle of scripted action
var() bool bDestroyMarker;

event XC_Init();


event Destroyed()
{
	local int i;

	if ( bDestroying )
		return;
	bDestroying = true;
		
	while ( NotifyList != None )
	{
		NotifyList.Destroy();
		NotifyList = NotifyList.NextNotify;
	}
	
	for ( i=Attractors.Length-1 ; i>=0 ; i-- )
		if ( Attractors[i] != None )
			Attractors[i].Destroy();

	for ( i=Detractors.Length-1 ; i>=0 ; i-- )
		if ( Detractors[i] != None )
			Detractors[i].Destroy();
	
	if ( (AIMarker != None) && (AIMarker.upstreamPaths[0] == -1 || AIMarker.Paths[0] == -1 || bDestroyMarker) ) //Useless marker
		DestroyAIMarker();
		
	CleanupEvents();
}

//========================== Notifications
//

event Timer()
{
	Update();
}

event Trigger( Actor Other, Pawn EventInstigator)
{
	SetTimer( 0.001, false);
}

event Actor SpecialHandling( Pawn Other)
{
	if ( Owner != None )
		return Owner.SpecialHandling( Other);
}


//========================== Modifiable Traits
//

//Set Root, Active, InProgress here
function Update(); 

//Initiate analysis of whatever this can trigger
function AnalyzedBy( EventLink Other); 

//Actor can initiate event chain by interacting with owner
function bool CanFireEvent( Actor Other)
{
	return bRoot && bRootEnabled && (Other != None);
}

//Analysis wants to register this trigger notify (for Update notifications)
//If EventLink knows how to do this without a notify, override so it does nothing
function AutoRegisterNotify( name aEvent)
{
	RegisterNotify( aEvent);
}

//AutoNotify sent a notify signal due to owner emitting an event
function TriggerNotify( name aEvent)
{
	Update();
}

//A pawn is at location or approaching us
function AIQuery( Pawn Seeker, NavigationPoint Nav);

//Creates a NavigationPoint to mark this event
function CreateAIMarker()
{
	if ( AIMarker == None )
	{
		AIMarker = Spawn( class'XC_NavigationPoint', self, 'AIMarker');
		LockToNavigationChain( AIMarker, true);
		DefinePathsFor( AIMarker, Owner);
		AIMarker.Move( Normal(Location - AIMarker.Location) * 4 );
		if ( Owner.Brush != None ) //More!
			AIMarker.Move( Normal(Location - AIMarker.Location) * 4 );
	}
}

//Destroys AI marker
function DestroyAIMarker()
{
	if ( AIMarker != None )
	{
		AIMarker.Destroy();
		AIMarker = None;
	}
}

//Optional AI marker to defer to
function NavigationPoint DeferTo()
{
	return AIMarker;
}


//========================== Attractor/Detractor logic
//

//Detractor wants this EventLink to grab paths leading to its marked TargetPath and redirect them
//SAMPLE FUNCTION: GRAB ALL REACHSPECS
function DetractorUpdate( EventDetractorPath EDP)
{
	local int i;
	local ReachSpec R;
	
	if ( EDP == None || EDP.TargetPath == None )
		return;

	CompactPathList(EDP.TargetPath);
	For ( i=0 ; i<16 && EDP.TargetPath.upstreamPaths[i]>=0 ; i++ )
		if ( GetReachSpec( R, EDP.TargetPath.upstreamPaths[i]) 
		&& (R.End == EDP.TargetPath) && (EventDetractorPath(R.Start) == None)
		/*&& ADDITIONAL CONDITIONS*/ )
		{
			R.End = EDP;
			SetReachSpec( R, EDP.TargetPath.upstreamPaths[i--], true);
		}

}

final function AddAttractor( EventAttractorPath EAP)
{
	local int i;
	
	i = Attractors.Length;
	Attractors.Insert(i);
	Attractors[i] = EAP;
}

final function AddDetractor( EventDetractorPath EDP)
{
	local int i;
	
	i = Detractors.Length;
	Detractors.Insert(i);
	Detractors[i] = EDP;
}

final function RemoveAttractor( EventAttractorPath EAP)
{
	local int i;

	for ( i=Attractors.Length-1 ; i>=0 ; i-- )
		if ( Attractors[i] == EAP || Attractors[i] == None || Attractors[i].bDeleteMe )
			Attractors.Remove(i);
}

final function RemoveDetractor( EventDetractorPath EDP)
{
	local int i;

	for ( i=Detractors.Length-1 ; i>=0 ; i-- )
		if ( Detractors[i] == EDP || Detractors[i] == None || Detractors[i].bDeleteMe )
			Detractors.Remove(i);
}


//========================== Event digest
//
final function bool AddEvent( name InEvent)
{
	if ( (InEvent != '') && !HasEvent(InEvent) )
	{
		EventList = EventList $ string(InEvent) $ ";";
		return true;
	}
	return false;
}

final function bool HasEvent( name InEvent)
{
	return InStr( EventList, ";" $ string(InEvent) $ ";") >= 0;
}

final function bool RemoveEvent( name InEvent)
{
	local int i;
	
	i = InStr( EventList, string(InEvent) $ ";");
	if ( i > 0 )
	{
		EventList = Left(EventList,i) $ Mid(EventList,i+Len(string(InEvent))+1);
		return true;
	}
}

final function CleanupEvents()
{
	local EventLink E;
	local int i;
	local name TmpEvent;
	local bool bRootCleanup;

	if ( bCleanupPending )
		return;
	bCleanupPending = true;
	
	bRootCleanup = !class'EventLink'.default.bStaticCleanup;
	class'EventLink'.default.bStaticCleanup = true;
	
	// Mark events related to self to be cleaned up.
	while ( (EventList != "") && bRootCleanup )
	{
		EventList = Mid( EventList, 1);
		i = InStr( EventList, ";");
		if ( i > 0 )
		{
			TmpEvent = XCS.static.StringToName( Left(EventList,i) );
			if ( TmpEvent != '' )
			{
				ForEach DynamicActors( class'EventLink', E, TmpEvent)
					E.CleanupEvents();
			}
		}
	}
	EventList = ";";
	
	// Queue single root for cleanup
	for ( i=0 ; i<RootEvents.Length ; i++ )
		RootEvents[i].CleanupEvents();
	RootEvents.Length = 0;
	
	//Last step, done once
	if ( bRootCleanup )
	{
		ForEach DynamicActors( class'EventLink', E)
			if ( !E.bDestroying && E.bCleanupPending )
			{
				E.Update();
				if ( !E.bDeleteMe )
				{
					E.AnalysisDepth = 0;
					E.AnalyzedBy( None);
				}
			}
		ForEach DynamicActors( class'EventLink', E)
			E.bCleanupPending = false;
		class'EventLink'.default.bStaticCleanup = false;
	}
}

final function AcquireEvents( EventLink From)
{
	local string RemainingEvents;
	local int i;
	
	For ( RemainingEvents=Mid(From.EventList,1) ; RemainingEvents!="" ; RemainingEvents=Mid(RemainingEvents,i+1) )
	{
		i = InStr( RemainingEvents, ";");
		AddEvent( XCS.static.StringToName( Left( RemainingEvents, i) ) );
	}
}

//========================== Event Analysis
// When analyzing upon destruction, all chained events lose their 'root'
//
final function AnalyzeEvent( name aEvent)
{
	local EventLink E;

	if ( bDeleteMe || bDestroying )
		return;

	AnalysisDepth++;
	if ( (aEvent != '') && !HasEvent(aEvent) ) //Link events
	{
		AddEvent( aEvent);
		AutoRegisterNotify( aEvent);
		ForEach DynamicActors( class'EventLink', E, aEvent)
		{
			E.AddRoot( self);
			if ( E.AnalysisDepth == 0 ) //Never self
				E.AnalyzedBy( self);
		}
	}
	AnalysisDepth--;
}

final function RegisterNotify( name aEvent)
{
	local EventChainTriggerNotify N;
	
	for ( N=NotifyList ; N!=None ; N=N.NextNotify )
		if ( N.Tag == aEvent )
			return;
	N = NotifyList;
	NotifyList = Spawn( class'EventChainTriggerNotify', self, aEvent, Location);
	NotifyList.NextNotify = N;
}

final function AddRoot( EventLink Other)
{
	local int i, Count;
	
	Count = RootEvents.Length;
	for ( i=0 ; i<Count ; i++ )
		if ( RootEvents[i] == Other )
			return;
	RootEvents.Insert( Count );
	RootEvents[Count] = Other;
}

final function array<EventLink> GetEnabledRoots( optional bool bSubQuery)
{
	local array<EventLink> EnabledList, RootAdd;
	local int i, j, EnabledCount;
	
	if ( !bSubQuery )
		StartQuery();
	if ( bRoot && bRootEnabled )
	{
		EnabledList.Insert(0);
		EnabledList[EnabledCount++] = self;
	}

	for ( i=0 ; i<RootEvents.Length ; i++ )
		if ( RootEvents[i].ValidQuery() )
		{
			RootAdd = RootEvents[i].GetEnabledRoots( true);
			EnabledList.Insert( EnabledCount, RootAdd.Length);
			for ( j=0 ; j<RootAdd.Length ; j++ )
				EnabledList[EnabledCount++] = RootAdd[j];
		}
	return EnabledList;
}

final function bool ChainInProgress( optional bool bNearestRootOnly, optional bool bSubQuery)
{
	local int i;
	
	if ( !bSubQuery )
		StartQuery();
	if ( bInProgress )
		return true;
	if ( bRoot && bSubQuery && bNearestRootOnly )
		return false;
		
	for ( i=0 ; i<RootEvents.Length ; i++ )
		if ( RootEvents[i].ValidQuery() && RootEvents[i].ChainInProgress( bNearestRootOnly, true) )
			return true;
	return false;
}

final function bool RootIsEnabled( optional bool bSubQuery)
{
	local int i;

	if ( !bSubQuery )
		StartQuery();
	if ( bRoot && bRootEnabled )
		return true;
		
	for ( i=0 ; i<RootEvents.Length ; i++ )
		if ( RootEvents[i].ValidQuery() && RootEvents[i].RootIsEnabled( true) )
			return true;
	return false;
}

//Query macros
final function StartQuery()
{
	class'EventLink'.default.QueryTag++;
	QueryTag = class'EventLink'.default.QueryTag;
}
final function bool ValidQuery()
{
	local int OldTag;
	
	OldTag = QueryTag;
	QueryTag = class'EventLink'.default.QueryTag;
	return OldTag != QueryTag;
}

defaultproperties
{
     EventList=";"
}
