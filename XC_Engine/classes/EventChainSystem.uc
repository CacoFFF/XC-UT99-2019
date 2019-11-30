class EventChainSystem expands FV_Addons
	abstract;

/** ================ Initialization
 *
 * General EventLink analysis functions.
*/
static final function StaticInit( XC_Engine_Actor XCGEA)
{
	local EventLink E;

	ForEach XCGEA.DynamicActors( class'EventLink', E)
	{
		E.Update();
		if ( !E.bDeleteMe )
			E.AnalyzedBy( None);
	}
}

/** ================ Utils
 *
 * GetEnablers       - Looks for enablers using the Event Chain System.
 * GetEnablerActors  - Returns the actors said enablers correspond to.
*/
static final function array<EventLink> GetEnablers( Actor Other)
{
	local EventLink EL;
	local array<EventLink> EmptyArray;
	
	Assert( Other != None );
	if ( EventLink(Other) != None )
		return EventLink(Other).GetEnabledRoots();
	ForEach Other.DynamicActors( class'EventLink', EL, Other.Tag)
		if ( (EL.Owner == Other) && EL.bLink )
			return EL.GetEnabledRoots();
	return EmptyArray;
}

static final function array<Actor> GetEnablerActor( Actor Other)
{
	local int i, j, Count;
	local array<EventLink> Enablers;
	local array<Actor> EnablerActors;
	
	Enablers = GetEnablers( Other);
	Count = Array_Length(Enablers);
	for ( i=0 ; i<Count ; i++ )
		if ( Enablers[i].Owner != None )
			EnablerActors[j++] = Enablers[i].Owner;
	return EnablerActors;
}

/** ================ Reroute EndPoint
 *
 * Conditional path creation and reassignment based on analyzed events.
 *
 * OwnerEvent      - EventLink that controls path creation and reassignment.
 * TargetPaths     - List of EndPoints that need an alternate route to be reached.
 * Seeker          - Instigator of this action.
 * ForceEnabler    - Instigator wants this specific actor to be considered as primary Enabler.
 *
 * Event Attactor is a single instance of an actor that can force attraction towards multiple
 * locked destinations.
 *
 * Event Detractors are multiple intercepters between the TargetPaths and their upstream paths.
*/

static final function RerouteEndPoint( EventLink OwnerEvent, NavigationPoint TargetPath, Pawn Seeker, optional Actor ForceEnabler)
{
	local array<NavigationPoint> TargetPaths;
	
	if ( TargetPath != None )
	{
		TargetPaths[0] = TargetPath;
		RerouteEndPoints( OwnerEvent, TargetPaths, Seeker, ForceEnabler);
	}
}

static final function RerouteEndPoints( EventLink OwnerEvent, array<NavigationPoint> TargetPaths, Pawn Seeker, optional Actor ForceEnabler)
{
	local int i, j, k;
	local int PathCount, EnablerCount, AttractorCount, DetractorCount;

	local EventLink EL;
	local array<EventLink> EnablerEvents;
	local EventDetractorPath EDP;
	local EventAttractorPath EAP;

	local Pawn OldInstigator;

	if ( OwnerEvent == None )
		return;

	PathCount = Array_Length( TargetPaths);
	if ( PathCount <= 0 )
		return;
	
	//Add a specific set of enablers
	if ( ForceEnabler != None )
	{
		ForEach ForceEnabler.ChildActors( class'EventLink', EL)
			if ( EL.bRoot )
			{
				EnablerEvents = EL.GetEnabledRoots();
				EnablerCount = Array_Length( EnablerEvents);
				break;
			}
	}

	//Find enablers (using AnalysisRoot)
	if ( EnablerCount == 0 )
	{
		EnablerEvents = OwnerEvent.GetEnabledRoots();
		EnablerCount = Array_Length( EnablerEvents);
		if ( EnablerCount == 0 )
			return;
	}

	//Setup or update attractor if needed
	AttractorCount = Array_Length( OwnerEvent.Attractors);
	for ( i=0 ; i<EnablerCount ; i++ )
	{
		EAP = None;
		for ( j=0 ; j<AttractorCount && EAP==None ; j++ )
			if ( OwnerEvent.Attractors[j].EnablerEvent == EnablerEvents[i] )
				EAP = OwnerEvent.Attractors[j];
		if ( (EAP != None) && (EAP.upstreamPaths[0] == -1) )
		{
			EAP.BroadcastMessage("Testlog"@EAP.Name);
			Log("Testlog"@EAP.Name);
			EAP.Destroy();
			EAP = None;
		}
		if ( EAP == None )
		{
			EAP = OwnerEvent.Spawn( class'EventAttractorPath', None,, EnablerEvents[i].Location);
			EAP.Setup( None, OwnerEvent, EnablerEvents[i]);
		}
		EAP.AddAttractorDestinations( TargetPaths);
	}

	//Update existing detractors
	OldInstigator = OwnerEvent.Instigator;
	OwnerEvent.Instigator = Seeker;
	DetractorCount = Array_Length( OwnerEvent.Detractors);
	for ( i=0 ; i<EnablerCount ; i++ )
	{
		for ( k=0 ; k<PathCount ; k++ )
		{
			EDP = None;
			for ( j=0 ; j<DetractorCount && EDP==None ; j++ )
				if ( (OwnerEvent.Detractors[j].EnablerEvent == EnablerEvents[i]) && (OwnerEvent.Detractors[j].TargetPath == TargetPaths[k]) )
				{
					EDP = OwnerEvent.Detractors[j];
					OwnerEvent.DetractorUpdate( EDP);
				}
			if ( EDP == None ) //Setup missing detractor
			{
				EDP = OwnerEvent.Spawn( class'EventDetractorPath', None, OwnerEvent.Tag, TargetPaths[k].Location+VRand(), TargetPaths[k].Rotation);
				EDP.Setup( TargetPaths[k], OwnerEvent, EnablerEvents[i]);
			}
		}
	}
	OwnerEvent.Instigator = OldInstigator;
}
