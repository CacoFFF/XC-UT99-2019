class XC_Engine_PlayerPawn expands PlayerPawn
	abstract;

var float GW_TimeSeconds;
var int GW_Counter;

//*******************************
// Native viewclass!!!
exec native function ViewClass( class<actor> aClass, optional bool bQuiet );
native event PlayerCalcView( out Actor ViewActor, out vector CameraLocation, out rotator CameraRotation);


final function bool CanGWSpam()
{
	if ( class'XC_Engine_PlayerPawn'.default.GW_TimeSeconds != Level.TimeSeconds ) //Antispam, fixes bandwidth exploit
	{
		class'XC_Engine_PlayerPawn'.default.GW_TimeSeconds = Level.TimeSeconds;
		class'XC_Engine_PlayerPawn'.default.GW_Counter = 0;
	}
	return ( class'XC_Engine_PlayerPawn'.default.GW_Counter++ < 3 );
}


//==================
// TeamSay extension
final function string TeamSayFilter( string Msg)
{
	local int i, j, k, Armor;
	local string Proc, Morph;
	local Inventory Inv;
	
	//Filter up to 6 occurences
	For ( i=0 ; i<6 ; i++ )
	{
		if ( Len(Msg) + Len(Proc) > 420 ) //weed
			break;
		//Find a command char
		j = InStr( Msg, "%");
		if ( (j == -1) || (Len(Msg)-j < 2) )//Make sure this is a two-char command
			break;
		//Split
		Proc = Proc $ Left(Msg,j);
		Morph = Mid(Msg,j,2);
		Msg = Mid(Msg,j+2);

		assert( Len(Morph) == 2 );
		k = Asc( Mid(Morph,1) ); //See what's after it

		switch ( k )
		{
			case 72: //H
				Morph = string(Health) @ "health";
				break;
			case 104: //h
				Morph = string(Health) $ "hp";
				break;
			case 65: //A
			case 97: //a
				Armor = 0;
				ForEach InventoryActors ( class'Inventory', Inv, true)
					if ( Inv.bIsAnArmor )
						Armor += Inv.Charge;
				if ( k == 65 )	Morph = string(Armor) @ "armor";
				else			Morph = string(Armor) $ "a";
				break;
			case 90: //Z
			case 122: //z
				if ( Region.Zone == None )
					Morph = "";
				else if ( PlayerReplicationInfo != None && PlayerReplicationInfo.PlayerLocation != None )
					Morph = PlayerReplicationInfo.PlayerLocation.LocationName;
				else
					Morph = Region.Zone.ZoneName;
				if ( Morph == "" )
					Morph = "Zone ["$Region.ZoneNumber$"]";
				break;
			case 87:
			case 119:
				if ( Weapon == None )	Morph = "[]";
				else					Morph = Weapon.GetHumanName();
			default:
				break;
		}
		Proc = Proc $ Morph;
	}
	return Proc $ Msg;
}

final function TeamSayInternal( string Msg)
{
	local PlayerPawn P;
	
	if ( Level.Game.AllowsBroadcast(self, Len(Msg)) )
		ForEach PawnActors ( class'PlayerPawn', P,,,true)
			if( P.PlayerReplicationInfo.Team == PlayerReplicationInfo.Team )
			{
				if ( (Level.Game != None) && (Level.Game.MessageMutator != None) )
				{
					if ( Level.Game.MessageMutator.MutatorTeamMessage(Self, P, PlayerReplicationInfo, Msg, 'TeamSay', true) )
						P.TeamMessage( PlayerReplicationInfo, Msg, 'TeamSay', true );
				} else
					P.TeamMessage( PlayerReplicationInfo, Msg, 'TeamSay', true );
			}
}

exec function TeamSay( string Msg )
{
	local Pawn P;

	if ( !Level.Game.bTeamGame )
	{
		Say(Msg);
		return;
	}
	if ( Msg ~= "Help" )
	{
		CallForHelp();
		return;
	}		
	TeamSayInternal( TeamSayFilter( Msg) );
}
//==============================================
// CheatFlying spectator goes through teleporter
final function bool IsTouchingInternal( Actor Other)
{
	local vector V;
	V = Other.Location - Location;
	return (HSize(V) < CollisionRadius+Other.CollisionRadius) && (Abs(V.Z) < CollisionHeight+Other.CollisionHeight);
}
final function PlayerTick_CF_SpecTeleInternal()
{
	local Teleporter T, TDest, TBest;
	local vector V;
	local name N;
	local int i;
	local bool bSpecTele;

	if ( (Teleporter(MoveTarget) != None) && !IsTouchingInternal(MoveTarget) )
		MoveTarget = None;
	ForEach CollidingActors (class'Teleporter', T, CollisionRadius+CollisionHeight+30)
	{
		if ( (T != MoveTarget) && IsTouchingInternal(T) && (T.URL != "") )
		{
			if ( (InStr( T.URL, "/" ) >= 0) || (InStr( T.URL, "#" ) >= 0) ) //Do not switch levels
				continue;

			N = class'XC_CoreStatics'.static.StringToName(T.URL); //Optimization, no need for URL check
			if ( N != '' )
				ForEach AllActors( class'Teleporter', TDest, N)
					if ( (TDest != T) && (Rand(++i) == 0) )
						TBest = TDest;

			if ( (TBest != None) && TBest.Accept( self, T) )
				MoveTarget = TBest;
			else
				MoveTarget = T;
			return;
		}
	}
}
event PlayerTick_CF( float DeltaTime )
{
	if ( !bCollideActors && IsA('Spectator') && FRand() < 0.5 && !IsA('bbCHSpectator') ) //Not all frames
		PlayerTick_CF_SpecTeleInternal();

	if ( bUpdatePosition )
		ClientUpdatePosition();

	PlayerMove(DeltaTime);
}


//=============================
// Feign death multigunning fix
function PlayerMove(float DeltaTime); //Will this work?
event PlayerTick_FD( float DeltaTime )
{
	if ( bFire + bAltFire == 0 )
		Weapon = None;
	if ( bUpdatePosition )
		ClientUpdatePosition();
	PlayerMove(DeltaTime);
}

//=========================
// Feign death log spam fix
function AnimEnd_FD()
{
	if ( Role < ROLE_Authority )
		return;
	if ( Health <= 0 )
	{
		GotoState('Dying');
		return;
	}
	GotoState('PlayerWalking');
	if ( PendingWeapon != None ) //Sanity check
	{
		if ( Weapon == None )
		{
			PendingWeapon.SetDefaultDisplayProperties();
			ChangedWeapon();
		}
		else if ( Weapon == PendingWeapon )
			PendingWeapon = None;
	}
}

//=================
// Mutate anti-spam
exec function Mutate(string MutateString)
{
	if( Level.NetMode == NM_Client )
		return;
	if ( bAdmin || PlayerReplicationInfo == None || class'XC_EngineStatics'.static.Allow_Mutate(Self) )
		Level.Game.BaseMutator.Mutate(MutateString, Self);
}


//============================================
// The original GetWeapon hook, improved a bit
final function bool SelectPendingInner( Weapon NewPendingWeapon)
{
	if ( (NewPendingWeapon.AmmoType != none) && (NewPendingWeapon.AmmoType.AmmoAmount <= 0) )
	{
		if ( CanGWSpam() )
			ClientMessage( NewPendingWeapon.ItemName$NewPendingWeapon.MessageNoAmmo );
	}
	else
	{
		PendingWeapon = NewPendingWeapon;
		if ( Weapon != none )
			Weapon.PutDown();
		else
		{
			Weapon = PendingWeapon;
			PendingWeapon = none;
			Weapon.BringUp();
		}
		return true;
	}
}

final function GetWeaponInner( class<Weapon> NewWeaponClass)
{
	local Weapon Weap, StartFrom;
	
	if ( (Weapon != None) && Weapon.IsA(NewWeaponClass.Name) )
		StartFrom = Weapon;

	ForEach InventoryActors ( class'Weapon', Weap, true, StartFrom)
		if ( Weap.IsA( NewWeaponClass.Name) && SelectPendingInner(Weap) )
			return;
		
	if ( StartFrom != None ) //Our weapon is already of said type, cycle
	{
		ForEach InventoryActors ( class'Weapon', Weap, true)
			if ( Weap == Weapon || (Weap.IsA( NewWeaponClass.Name) && SelectPendingInner(Weap)) )
				return;
	}
}

exec function GetWeapon(class<Weapon> NewWeaponClass )
{
	local Inventory Inv;

	if ( (Inventory == None) || (NewWeaponClass == None) )
		return;
	GetWeaponInner( NewWeaponClass);
}

// =====================
// Fix message spam AIDS
exec function PrevItem()
{
	local Inventory Inv, LastItem;

	if ( bShowMenu || Level.Pauser!="" || Inventory == None )
		return;
	if ( SelectedItem == None )
	{
		SelectedItem = Inventory.SelectNext();
		return;
	}
	if ( SelectedItem.Inventory != None )
	{	For( Inv=SelectedItem.Inventory; Inv!=None; Inv=Inv.Inventory )
			if (Inv.bActivatable)
				LastItem=Inv; }

	For( Inv=Inventory; Inv!=SelectedItem && Inv!=None; Inv=Inv.Inventory )
		if (Inv.bActivatable)
			LastItem=Inv;

	if (LastItem!=None)
	{
		SelectedItem = LastItem;
		if ( CanGWSpam() )
			ClientMessage(SelectedItem.ItemName$SelectedItem.M_Selected);
	}
}



//==============
// More easily find players
exec function ViewPlayer_Fast( string S)
{
	local Pawn P;
	
	S = Caps(S);
	ForEach PawnActors ( class'Pawn', P,,, true)
	{
		if ( P.PlayerReplicationInfo.PlayerName ~= S  )
			break;
		P = None;
	}

	if ( P == None )
		ForEach PawnActors ( class'Pawn', P,,, true)
		{
			if ( InStr( Caps(P.PlayerReplicationInfo.PlayerName), S) >= 0 )
				break;
			P = None;
		}
		
	if ( (P != None) && Level.Game.CanSpectate(self, P) )
	{
		ClientMessage(ViewingFrom@P.PlayerReplicationInfo.PlayerName, 'Event', true);
		if ( P == self)
			ViewTarget = None;
		else
			ViewTarget = P;
	}
	else
		ClientMessage(FailedView);

	bBehindView = ( ViewTarget != None );
	if ( bBehindView )
		ViewTarget.BecomeViewTarget();
}

//==============
// ViewPlayerNum AIDS given by client to server
// Prevent lag exploit and enhance Spectator experience

//Needs 'final' modifier to compile a non-virtual call
//This one does not target PRI-less monsters
final function Actor XC_CyclePlayer()
{
	local Pawn P;
	local PlayerReplicationInfo PRI;
	
	P = Pawn(ViewTarget);
	//Find player using DynamicActors
	if ( P == None || (P.PlayerReplicationInfo == None) )
	{
RESTART_SEARCH:
		ForEach DynamicActors ( class'PlayerReplicationInfo', PRI)
		{
			P = Pawn(PRI.Owner);
			if ( (P != None) && !PRI.bIsSpectator && (P != self) && Level.Game.CanSpectate( self, P) )
				return P;
		}
	}
	else
	{
		ForEach DynamicActors ( class'PlayerReplicationInfo', PRI)
		{
			if ( P == None ) //This means we found our current viewtarget's position
			{
				if ( (Pawn(PRI.Owner) != None) && !PRI.bIsSpectator && (PRI.Owner != self) && Level.Game.CanSpectate( self, Pawn(PRI.Owner)) )
					return PRI.Owner;
			}
			else if ( P.PlayerReplicationInfo == PRI ) //Finding our viewtarget!
				P = None;
		}
		Goto RESTART_SEARCH;
	}
}

exec function ViewPlayerNum_Fast(optional int num)
{
	local Pawn P;

	if ( PlayerReplicationInfo == None || Level.Game == None )
		return;
	if ( !PlayerReplicationInfo.bIsSpectator && !Level.Game.bTeamGame )
		return;
	if ( num >= 0 )
	{
		P = Pawn(ViewTarget);

		//UTPure style ViewPlayerNum, get players using their PlayerID!
		if ( PlayerReplicationInfo.bIsSpectator )
		{
			if ( ((P != None) && (P.PlayerReplicationInfo != None) && (P.PlayerReplicationInfo.PlayerID == num)) || (PlayerReplicationInfo.PlayerID == num) )
			{
				ViewTarget = None;
				bBehindView = False;
			}
			else
			{
				ForEach PawnActors (class'Pawn',P,,,true) //Guaranteed not self
					if ( !P.PlayerReplicationInfo.bIsSpectator && P.PlayerReplicationInfo.PlayerID == num )
					{
						ViewTarget = P;
						bBehindView = true;
						break;
					}
			}
			return;
		}

		//Normal style
		if ( ((P != None) && (P.PlayerReplicationInfo != None) && (P.PlayerReplicationInfo.TeamID == num)) || (PlayerReplicationInfo.TeamID == num) )
		{
			ViewTarget = None;
			bBehindView = false;
		}
		else
		{
			ForEach PawnActors ( class'Pawn',P,,,true)
				if ( (P.PlayerReplicationInfo.Team == PlayerReplicationInfo.Team) && !P.PlayerReplicationInfo.bIsSpectator && (P.PlayerReplicationInfo.TeamID == num) )
				{
					ViewTarget = P;
					bBehindView = true;
					break;
				}
		}
		return;
	}
	if ( Role == ROLE_Authority )
	{
		ViewTarget = XC_CyclePlayer(); //Maximum 2 iterators max
		if ( ViewTarget != None )
			ClientMessage(ViewingFrom@Pawn(ViewTarget).PlayerReplicationInfo.PlayerName, 'Event', true);
		else
			ClientMessage(ViewingFrom@OwnCamera, 'Event', true);
	}
}

