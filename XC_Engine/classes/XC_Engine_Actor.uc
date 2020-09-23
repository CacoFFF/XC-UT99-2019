//=============================================================================
// XC_Engine_Actor
// Automatically spawned by XC_Engine in the server/local game
// You may subclass this actor for your mod.
// All actors subclassed from this are arranged first in dynamic list
//=============================================================================
class XC_Engine_Actor expands Actor
	native;

// Opcodes used:
/*  3538 - 3542
    3552 - 3561
    3570 - 3572
*/
	
//Reach flags used in navigation
const R_WALK       = 0x00000001; //walking required
const R_FLY        = 0x00000002; //flying required 
const R_SWIM       = 0x00000004; //swimming required
const R_JUMP       = 0x00000008; //jumping required
const R_DOOR       = 0x00000010;
const R_SPECIAL    = 0x00000020;
const R_PLAYERONLY = 0x00000040;

const XCS = class'XC_EngineStatics';
	
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
var() ReachSpec DummyReachSpec;
var() transient editconst XC_Engine_Actor_CFG ConfigModule;

//Numbered natives cannot be safely replaced with script functions
native /*(532)*/ final function bool PlayerCanSeeMe_XC();
native /*(539)*/ final function string GetMapName_XC( string NameEnding, string MapName, int Dir );


/** ================ Other useful functions
*/
static final preoperator  bool  !  ( int N )
{
	return N == 0;
}

static final preoperator  bool  !  ( Object O )
{
	return O == none;
}




/** ================ Reach spec manipulation
Notes:

 All reachspecs reside in ULevel::ReachSpecs (TArray<FReachSpec>)
These functions allow copying from and to unrealscript templates
It's also possible to add new elements to said array but it's always
prefferable to reutilize unused reachspecs (Start=None,End=None)

 AddReachSpec and SetReachSpec will automatically add the reachspec
index to both Start.Paths and End.upstreamPaths if bAutoSet is True

 Note: you don't need to cleanup reachspecs or unlink a navigation
point if you're destroying it, XC_Engine will deal with the cleanup
tasks internally. (not on Net Clients)
*/


//Natives that are exclusive to this actor type and are safe to call in clients.
native final function bool GetReachSpec( out ReachSpec R, int Idx);
native final function bool SetReachSpec( ReachSpec R, int Idx, optional bool bAutoSet); //[Idx -1] forces auto-set and stores on any available ReachSpec slot
native final function int ReachSpecCount();
native final function int AddReachSpec( ReachSpec R, optional bool bAutoSet); //Returns index of newle created ReachSpec
native final function int FindReachSpec( Actor Start, Actor End); //-1 if not found, useful for finding unused specs (actor = none)
native final function iterator AllReachSpecs( out ReachSpec R, out int Idx); //Idx can actually modify the starting index!!!
native final function DefinePathsFor( NavigationPoint N, optional Actor AdjustTo, optional Pawn Reference, optional float MaxDistance); //N must have no connections!

//Natives that can be called from anywhere and are safe to call in clients.
native static final function CompactPathList( NavigationPoint N); //Also cleans up invalid paths (Start or End = NONE)
native static final function LockToNavigationChain( NavigationPoint N, bool bLock);

static function ResetReachSpec( out ReachSpec R)
{
	R.Start = None;
	R.End = None;
	R.bPruned = 0;
	R.Distance = 0;
	R.CollisionHeight = 60;
	R.CollisionRadius = 60;
}

//Find all reachspecs linking to/from N, clear and dereference (SLOW!)
function CleanupNavSpecs( NavigationPoint N)
{
	local ReachSpec R, R_Empty;
	local int RI;
	
	ForEach AllReachSpecs( R, RI)
		if ( R.Start == N || R.End == N )
			SetReachSpec( R_Empty, RI, true);
}

//Find all outgoing paths from this NavigationPoint (no unsafe operations)
function CleanupNavOutgoing( NavigationPoint N)
{
	local int i;
	local ReachSpec R, RClean;
	
	CompactPathList( N); //Stabilizes list and following algorithm
	while ( i<16 && N.Paths[i] >= 0 )
	{
		if ( GetReachSpec( R, N.Paths[i]) && (R.Start == N) )
			SetReachSpec( RClean, N.Paths[i], true);
		else
			i++;
	}
}

//Creates a special connection with a single call
function SpecialConnectNavigationPoints( NavigationPoint Start, NavigationPoint End, optional int Distance, optional int ReachFlags)
{
	local ReachSpec R;
	
	if ( Start != None && End != None )
	{
		R.Distance = Max( 1, Distance);
		R.Start = Start;
		R.End = End;
		R.CollisionRadius = 100;
		R.CollisionHeight = 100;
		R.ReachFlags = ReachFlags;
		R.bPruned = 0;
		SetReachSpec( R, -1);
	}
}

//EZ quick connect between both nodes
function EzConnectNavigationPoints( NavigationPoint Start, NavigationPoint End, optional float Scale, optional bool bOneWay)
{
	local ReachSpec R;
	local Actor A;
	local int rIdx, pIdx;
	local bool bConnected;
	
	if ( Start == End )
		return;
	
	if ( Scale <= 0 )
		Scale = 1;
	ForEach ConnectedDests ( Start, A, rIdx, pIdx)
		if ( A == End )
		{
			bConnected = true;
			break;
		}
	if ( !bConnected )
	{
		R.Start = Start;
		R.End = End;
		R.ReachFlags = R_WALK | R_JUMP;
		if ( Start.Region.Zone.bWaterZone || End.Region.Zone.bWaterZone )
			R.ReachFlags = R.ReachFlags | R_SWIM;
		if ( Start.IsA('LiftCenter') || End.IsA('LiftCenter') )			R.Distance = VSize( Start.Location - End.Location) * 0.2;
		else if ( Start.IsA('Teleporter') && End.IsA('Teleporter') )	R.Distance = 0;
		else															R.Distance = VSize( Start.Location - End.Location);
		R.CollisionHeight = 50 * Scale;
		R.CollisionRadius = 25 * Scale;
		AddReachSpec( R, true); //Auto-register in path
	}
	//Add incoming path as well
	if ( !bOneWay )
		EzConnectNavigationPoints( End, Start, Scale, true);
}


/** ================ Global actor natives

Registration of these natives doesn't occur at load time, but manually at map load
See XC_CoreStatics for more natives for global object natives

These functions only work if XC_Engine is loaded and running in server/standalone mode.
In order to use them, copy/paste into the target class and call.

This way it's possible to add optional XC_Engine functionality without creating
package dependancy.

*/


native(1718) final function bool AddToPackageMap( optional string PkgName);
native(1719) final function bool IsInPackageMap( optional string PkgName, optional bool bServerPackagesOnly); //Second parameter doesn't exist in 227!

//Iterators
native(3540) final iterator function PawnActors( class<Pawn> PawnClass, out pawn P, optional float Distance, optional vector VOrigin, optional bool bHasPRI, optional Pawn StartAt);
native(3541) final iterator function NavigationActors( class<NavigationPoint> NavClass, out NavigationPoint P, optional float Distance, optional vector VOrigin, optional bool bVisible);
native(3542) final iterator function InventoryActors( class<Inventory> InvClass, out Inventory Inv, optional bool bSubclasses, optional Actor StartFrom); 
native(3552) final iterator function CollidingActors( class<Actor> BaseClass, out actor Actor, float Radius, optional vector Loc);
native(3553) final iterator function DynamicActors( class<Actor> BaseClass, out actor Actor, optional name MatchTag );
native(3554) static final function iterator ConnectedDests( NavigationPoint Start, out Actor End, out int ReachSpecIdx, out int PathArrayIdx); //XC_Core


/** Important notes!

Careful with InState, the specified state we're targeting MUST exist in ReplaceClass.
Multiple classes in a hierarchy may have a definition of the same state.

Make sure the function bodies match.

If the function needs to access a variable that doesn't exist in 'Replace' class,
then subclass the 'With' class from 'Replace' and define the function there.
Otherwise, do a typecast to allow variable access, ex: Transporter(self).Offset

Turning an Event into a native Event will most likely crash the game.
Events don't parse parameters from stack, they're already prestored in the parent stack.
See "_PlayerPawn_funcs.h" in XC_Engine's source for more information.

You cannot replace operators.
Net, Exec flags are not replaced.
Native, Singular flags are replaced.
Iterator, Latent, Static, Const, Invariant flags must match or replacement will fail.

======
XC_Engine will internally store the information of original functions so they can
be restored to their original state prior to level switch.

*/
native(3560) static final function bool ReplaceFunction( class<Object> ReplaceClass, class<Object> WithClass, name ReplaceFunction, name WithFunction, optional name InState);
native(3561) static final function bool RestoreFunction( class<Object> RestoreClass, name RestoreFunction, optional name InState);



/** ================ XC_Init
 *
 * This event is the very first UnrealScript event in a XC_Engine server/standalone game
 *
 * This is the best place to replace functions, and the only one where it's possible to affect
 * level initialization. You may create your own mutator-like plugins and load them via
 * XC_Engine.ini in order to modify the behaviour of the game.
 *
 * Every single actor spawned within XC_Init will not have it's initial events called
 * (PreBeginPlay, BeginPlay, PostBeginPlay, SetInitialState) and neither their base or zone will
 * be set. This means that they are treated the same as other actors loaded with the level and
 * affected by mutators spawned later by Level.Game.InitGame
*/

//Note: This is the only script event called before GameInfo.Init
event XC_Init()
{
	local class<Actor> AC, AC2;
	local float Time[2];

	// Some subclasses may not override this function, avoid double replacements.
	if ( Class != class'XC_Engine_Actor' )
		return;
	
	// Sample version check here
//if ( int(ConsoleCommand("Get ini:Engine.Engine.GameEngine XC_Version")) < 19 )
//		return;

	XCS.static.ResetAll();
	bDirectional = true; //GetPropertyText helper

	// Instantiate the CFG object here, but don't init yet
	ConfigModule = New( Class.Outer, 'GeneralConfig') class'XC_Engine_Actor_CFG';
	class'XC_Engine_Actor_CFG'.default.bEventChainAddon = ConfigModule.bEventChainAddon;

	//Fixes
	class'XC_CoreStatics'.static.Clock( Time);
	ReplaceFunction( class'Actor', class'XC_Engine_Actor', 'PlayerCanSeeMe', 'PlayerCanSeeMe_XC');
	ReplaceFunction( class'Actor', class'XC_Engine_Actor', 'GetMapName', 'GetMapName_XC');

	//Server-only fixes
	if ( Level.NetMode == NM_ListenServer || Level.NetMode == NM_DedicatedServer )
	{
		Spawn( class'PreLoginHookAddon');
		Spawn( class'PreLoginHookXCGE');
		Spawn( class'ServerPauseDetector');
		if ( ConfigModule.bFixBroadcastMessage )
		{
			ReplaceFunction( class'Actor', class'XC_Engine_Actor', 'BroadcastMessage', 'BroadcastMessage');
			ReplaceFunction( class'Actor', class'XC_Engine_Actor', 'BroadcastLocalizedMessage', 'BroadcastLocalizedMessage');
		}
		if ( ConfigModule.bPatchUdpServerQuery )
		{
			AC = class<InternetInfo>( class'XC_CoreStatics'.static.FindObject( "UdpServerQuery", class'Class'));
			if ( AC != none )
				AC2 = class<InternetInfo>( DynamicLoadObject("XC_IpServerFix.XC_UdpServerQuery",class'Class') );
			if ( AC2 != none )
			{
				ReplaceFunction( AC, AC2, 'SendPlayers', 'SendPlayers');
				while ( AC != None && AC.Name != 'InternetLink' )
					AC = class<InternetInfo>(class'XC_CoreStatics'.static.GetParentClass( AC));
				if ( AC != None )
				{
					ReplaceFunction( AC2, AC, 'Validate_Org', 'Validate'); //Backup the function
					ReplaceFunction( AC, AC2, 'Validate', 'Validate'); //Securevalidate patch
				}
			}
		}
		if ( ConfigModule.bFixMoverTimeMP )
		{
			ReplaceFunction( class'XC_Engine_Mover', class'Mover', 'InterpolateTo_Org', 'InterpolateTo'); //Backup the function
			ReplaceFunction( class'Mover', class'XC_Engine_Mover', 'InterpolateTo', 'InterpolateTo_MPFix'); //Apply the fix
		}
		ReplaceFunction( class'Mover', class'XC_Engine_Mover', 'FinishedClosing', 'FinishedClosing_Test');
		ReplaceFunction( class'GameInfo', class'XC_Engine_GameInfo', 'PostLogin', 'PostLogin');
		ReplaceFunction( class'PlayerPawn', class'XC_Engine_PlayerPawn', 'Mutate', 'Mutate');
	}
	if ( Level.NetMode == NM_ListenServer )
	{
		if ( ConfigModule.bListenServerPlayerRelevant )
		{
			ReplaceFunction( class'XC_Engine_GameInfo', class'GameInfo', 'InitGame_Org', 'InitGame');
			ReplaceFunction( class'GameInfo', class'XC_Engine_GameInfo', 'InitGame', 'InitGame_Listen');
		}
	}

	//General fixes
	ReplaceFunction( class'Mover', class'XC_Engine_Mover', 'Trigger', 'TC_Trigger', 'TriggerControl');
	ReplaceFunction( class'PlayerPawn', class'XC_Engine_PlayerPawn', 'TeamSay', 'TeamSay');
	ReplaceFunction( class'PlayerPawn', class'XC_Engine_PlayerPawn', 'ViewClass', 'ViewClass'); //Native version
	ReplaceFunction( class'PlayerPawn', class'XC_Engine_PlayerPawn', 'PlayerCalcView', 'PlayerCalcView'); //Native version
	ReplaceFunction( class'PlayerPawn', class'XC_Engine_PlayerPawn', 'ViewPlayer', 'ViewPlayer_Fast'); //Partial name search
	ReplaceFunction( class'PlayerPawn', class'XC_Engine_PlayerPawn', 'GetWeapon', 'GetWeapon');
	ReplaceFunction( class'PlayerPawn', class'XC_Engine_PlayerPawn', 'PrevItem', 'PrevItem');
	if ( ConfigModule.bSpectatorHitsTeleporters )
		ReplaceFunction( class'PlayerPawn', class'XC_Engine_PlayerPawn', 'PlayerTick', 'PlayerTick_CF', 'CheatFlying'); //Spectators go thru teles
	ReplaceFunction( class'PlayerPawn', class'XC_Engine_PlayerPawn', 'PlayerTick', 'PlayerTick_FD', 'FeigningDeath'); //Multiguning fix
	ReplaceFunction( class'PlayerPawn', class'XC_Engine_PlayerPawn', 'AnimEnd', 'AnimEnd_FD', 'FeigningDeath');
	ReplaceFunction( class'Pawn', class'XC_Engine_Pawn', 'BaseChange', 'BaseChange');
	ReplaceFunction( class'Pawn', class'XC_Engine_Pawn', 'FindInventoryType', 'FindInventoryType_Fast');
	ReplaceFunction( class'Pawn', class'XC_Engine_Pawn', 'EncroachedBy', 'EncroachedBy');
	ReplaceFunction( class'XC_Engine_Pawn', class'Pawn', 'FindPathToward_Org', 'FindPathToward');
	ReplaceFunction( class'Pawn', class'XC_Engine_Pawn', 'FindPathToward', 'FindPathToward_RouteMapper');
	if ( Level.Game != None )
		ReplaceFunction( class'PlayerPawn', class'XC_Engine_PlayerPawn', 'ViewPlayerNum', 'ViewPlayerNum_Fast'); //Lag+crash exploit fix
	ReplaceFunction( class'GameInfo', class'XC_Engine_GameInfo', 'Killed', 'Killed');
	ReplaceFunction( class'XC_Engine_GameInfo', class'GameInfo', 'RestartPlayer_Original', 'RestartPlayer');
	ReplaceFunction( class'GameInfo', class'XC_Engine_GameInfo', 'RestartPlayer', 'RestartPlayer_Proxy');
	ReplaceFunction( class'GameReplicationInfo', class'XC_GameReplicationInfo', 'Timer', 'Timer_Server');
	ReplaceFunction( class'Weapon', class'XC_Engine_Weapon', 'CheckVisibility', 'CheckVisibility');
	ReplaceFunction( class'Weapon', class'XC_Engine_Weapon', 'WeaponChange', 'WeaponChange');
	ReplaceFunction( class'LiftCenter', class'XC_Engine_LiftCenter', 'SpecialHandling', 'SpecialHandling');
	ReplaceFunction( class'LiftExit', class'XC_Engine_LiftExit', 'SpecialHandling', 'SpecialHandling');

	Log( "Engine script addons setup ("$class'XC_CoreStatics'.static.UnClock(Time)$" second)",'XC_Engine');
	
	//Init CFG here
	ConfigModule.Setup(self);
	Log( "Other script addons setup ("$class'XC_CoreStatics'.static.UnClock(Time)$" second)",'XC_Engine');

	AttachMenu();
}

event PreBeginPlay()
{
//	Log("PRE "$Name);
	Super.PreBeginPlay();
}

event PostBeginPlay()
{
//	Log("POST "$Name);
}

//This event is called right after global PostBeginPlay
//Post-Initialize everything here
event SetInitialState()
{
//	Log("SIS "$Name);

	Super.SetInitialState();
	if ( Class == class'XC_Engine_Actor' && Level.bStartup )
	{
		FixLiftCenters();
		if ( ConfigModule.bEventChainAddon )
		{
			class'EventChainSystem'.static.StaticInit( self);
		}
	}	
}

//Called after LiftCenter finishes initializing
final function FixLiftCenters()
{
	local Mover M;
	local LiftCenter LC;
	local vector HitLocation, HitNormal;
	
	ForEach NavigationActors ( class'LiftCenter', LC)
		if ( (LC.Class == class'LiftCenter') && (LC.MyLift == None) )
		{
			M = Mover(LC.Trace( HitLocation, HitNormal, LC.Location - vect(0,0,80)) );
			if ( M != None )
			{
				LC.MyLift = M;
				M.MyMarker = LC;
				LC.SetBase( M);
				LC.LiftOffset = LC.Location - M.Location;
			}
		}
}

function bool TaggedMover( name MTag)
{
	local Mover M;
	ForEach DynamicActors (class'Mover', M, MTag )
		return true;
}

final function bool AttachMenu()
{
	local class<XC_CoreStatics> MenuStatics;

	if ( Level.NetMode != NM_DedicatedServer )
	{
		// Even if menu hasn't been loaded, it's likely to be loaded in the future, so preload
		MenuStatics = class<XC_CoreStatics>( DynamicLoadObject("XC_Engine_Menu.XC_MenuStatics", class'Class', true));
		if ( MenuStatics != None )
			return MenuStatics.static.StaticCall("OPTIONS") == "OK";
	}
	return false;
}


//=======================================================================
//=======================================================================
// ROUTE MAPPER
// 

native(3538) final function NavigationPoint MapRoutes_FPTW( Pawn Seeker, optional NavigationPoint StartAnchors[16], optional name RouteMapperEvent);

event FindPathToward_Event( Pawn Seeker, array<NavigationPoint> StartAnchors)
{
	local NavigationPoint N, Best;
	local float Dist, BestDist;
	
	if ( Target == None ) //For some reason this was not set!! (GiantManta)
		return;
	Assert( Target != None);
	
	if ( NavigationPoint(Target) != None )
		Best = NavigationPoint(Target);
	else
	{
		BestDist = 200 + int(Seeker.bHunting) * 2000;
		ForEach NavigationActors( class'NavigationPoint', N, BestDist, Target.Location, true)
		{
			Dist = VSize(N.Location - Target.Location);
			if ( Dist < BestDist )
			{
				Best = N;
				BestDist = Dist;
			}
		}
	}
	
	if ( Best != None )
		Best.bEndPoint = true;
}


//=======================================================================
//=======================================================================
// MESSAGE SPAM EXPLOIT FIXES
// Restrict the broadcasting powers of players

final function Actor GetTopOwner( Actor Other)
{
	while ( Other.Owner != None )
		Other = Other.Owner;
	return Other;
}

event BroadcastMessage( coerce string Msg, optional bool bBeep, optional name Type )
{
	local Pawn P;

	if (Type == '')
		Type = 'Event';

	P = PlayerPawn( GetTopOwner(self) );
	if ( (P != None) && (PlayerPawn(P).Player != None) )
	{
		if ( !P.IsA('Spectator') || (P.PlayerReplicationInfo == None) )
			return;
		if ( (P.PlayerReplicationInfo.PlayerName$":") != Left(Msg,Len(P.PlayerReplicationInfo.PlayerName)+1) ) //SAY
			return;
	}

	if ( Level.Game.AllowsBroadcast(self, Len(Msg)) )
		For( P=Level.PawnList; P!=None; P=P.nextPawn )
			if( P.bIsPlayer || P.IsA('MessagingSpectator') )
			{
				if ( (Level.Game != None) && (Level.Game.MessageMutator != None) )
				{
					if ( Level.Game.MessageMutator.MutatorBroadcastMessage(Self, P, Msg, bBeep, Type) )
						P.ClientMessage( Msg, Type, bBeep );
				}
				else
					P.ClientMessage( Msg, Type, bBeep );
			}
}


event BroadcastLocalizedMessage( class<LocalMessage> Message, optional int Switch, optional PlayerReplicationInfo RelatedPRI_1, optional PlayerReplicationInfo RelatedPRI_2, optional Object OptionalObject )
{
	local Pawn P;

	P = PlayerPawn( GetTopOwner(Self) );
	if ( (P != None) && (PlayerPawn(P).Player != None) )
		return;
	
	For ( P=Level.PawnList; P != None; P=P.nextPawn )
		if ( P.bIsPlayer || P.IsA('MessagingSpectator') )
		{
			if ( (Level.Game != None) && (Level.Game.MessageMutator != None) )
			{
				if ( Level.Game.MessageMutator.MutatorBroadcastLocalizedMessage(Self, P, Message, Switch, RelatedPRI_1, RelatedPRI_2, OptionalObject) )
					P.ReceiveLocalizedMessage( Message, Switch, RelatedPRI_1, RelatedPRI_2, OptionalObject );
			}
			else
				P.ReceiveLocalizedMessage( Message, Switch, RelatedPRI_1, RelatedPRI_2, OptionalObject );
		}
}



defaultproperties
{
     bHidden=True
	 bGameRelevant=True
	 RemoteRole=ROLE_None
}








