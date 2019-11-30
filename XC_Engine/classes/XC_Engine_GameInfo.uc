class XC_Engine_GameInfo expands GameInfo
	abstract;

native(1718) final function bool AddToPackageMap( optional string PkgName);
native(3553) final iterator function DynamicActors( class<actor> BaseClass, out actor Actor, optional name MatchTag );
native(3540) final iterator function PawnActors( class<Pawn> PawnClass, out pawn P, optional float Distance, optional vector VOrigin, optional bool bHasPRI, optional Pawn StartAt);



//Listen server tweaks
final function InitGame_Org( string Options, out string Error )
{
	local string InOpt, LeftOpt, NextMutator, NextDesc;
	local int pos, i;
	local class<Mutator> MClass;
	
	InOpt = "";
	LeftOpt = "";
	NextMutator = "";
	NextDesc = "";
	pos = 0;
	MClass = none;
}

event InitGame_Listen( string Options, out string Error )
{
	local string InOpt, LeftOpt, NextMutator, NextDesc;
	local int pos, i;
	local class<Mutator> MClass;
	
	InitGame_Org( Options, Error);

	InOpt = ParseOption( Options, "Class" );
	pos = InStr(InOpt,".");
	AddToPackageMap( Left(InOpt,pos));

	InOpt = ParseOption( Options, "Skin" );
	pos = InStr(InOpt,".");
	AddToPackageMap( Left(InOpt,pos));
	
	InOpt = ParseOption( Options, "Face" );
	pos = InStr(InOpt,".");
	AddToPackageMap( Left(InOpt,pos));
	
	InOpt = ParseOption( Options, "Voice" );
	pos = InStr(InOpt,".");
	AddToPackageMap( Left(InOpt,pos));
}



//******************************************************************
//*** PostLogin
// Group up skins in batches and avoid spamming a player's log
// This should also save heaps of bandwidth in maps full of monsters

event PostLogin( PlayerPawn NewPlayer )
{
	local Pawn P;
	local array<Texture> TextureList;
	local Texture T[3];
	local int i, j, TLMax;

	// Start player's music.
	NewPlayer.ClientSetMusic( Level.Song, Level.SongSection, Level.CdTrack, MTRAN_Fade );
	
	// replicate skins
	ForEach PawnActors( class'Pawn', P,,, true, NewPlayer.NextPawn) //Guaranteed to not collide with NewPlayer
	{
		if ( P.bIsPlayer )
		{
			if ( P.bIsMultiSkinned )
			{
				For ( j=0 ; j<4 ; j++ )
				{
					if ( P.MultiSkins[j] != None )
					{
						For ( i=0 ; i<TLMax ; i++ )
							if ( P.MultiSkins[j] == TextureList[i] )
								Goto NEXT_SKIN;
						TextureList[TLMax++] = P.MultiSkins[j];
					}
					NEXT_SKIN:
				}
			}
			else if ( (P.Skin != None) && (P.Skin != P.Default.Skin) )
			{
				For ( i=0 ; i<TLMax ; i++ )
					if ( P.MultiSkins[j] == TextureList[i] )
						Goto NEXT_PAWN;
				TextureList[TLMax++] = P.Skin;
				NEXT_PAWN:
			}

			if ( P.PlayerReplicationInfo.bWaitingPlayer && P.IsA('PlayerPawn') )
			{
				if ( NewPlayer.bIsMultiSkinned )
					PlayerPawn(P).ClientReplicateSkins(NewPlayer.MultiSkins[0], NewPlayer.MultiSkins[1], NewPlayer.MultiSkins[2], NewPlayer.MultiSkins[3]);
				else
					PlayerPawn(P).ClientReplicateSkins(NewPlayer.Skin);	
			}
		}
	}
	
	i=0;
	j=0;
	while ( i < (TLMax-3) )
		NewPlayer.ClientReplicateSkins( TextureList[i++], TextureList[i++], TextureList[i++], TextureList[i++]);
	while ( i<TLMax )
		T[j++] = TextureList[i++];
	if ( T[0] != None )
		NewPlayer.ClientReplicateSkins( T[0], T[1], T[2]);
	if ( TLMax > 0 )
		Array_Length( TextureList, 0);
}

	
function Killed( pawn Killer, pawn Other, name damageType )
{
	local String Message, KillerWeapon, OtherWeapon;
	local bool bSpecialDamage;

	if (Other.bIsPlayer && Other.PlayerReplicationInfo != None )
	{
		if ( (Killer != None) && (!Killer.bIsPlayer) )
		{
			Message = Killer.KillMessage(damageType, Other);
			BroadcastMessage( Message, false, 'DeathMessage');
			if ( LocalLog != None )
				LocalLog.LogSuicide(Other, DamageType, None);
			if ( WorldLog != None )
				WorldLog.LogSuicide(Other, DamageType, None);
			return;
		}
		if ( (DamageType == 'SpecialDamage') && (SpecialDamageString != "") )
		{
			if ( Killer.PlayerReplicationInfo != None )
				BroadcastMessage( ParseKillMessage(
						Killer.PlayerReplicationInfo.PlayerName,
						Other.PlayerReplicationInfo.PlayerName,
						Killer.Weapon.ItemName,
						SpecialDamageString
						),
					false, 'DeathMessage');
			bSpecialDamage = True;
		}
		Other.PlayerReplicationInfo.Deaths += 1;
		if ( (Killer == Other) || (Killer == None) )
		{
			// Suicide
			if (damageType == '')
			{
				if ( LocalLog != None )
					LocalLog.LogSuicide(Other, 'Unknown', Killer);
				if ( WorldLog != None )
					WorldLog.LogSuicide(Other, 'Unknown', Killer);
			} else {
				if ( LocalLog != None )
					LocalLog.LogSuicide(Other, damageType, Killer);
				if ( WorldLog != None )
					WorldLog.LogSuicide(Other, damageType, Killer);
			}
			if (!bSpecialDamage)
			{
				if ( damageType == 'Fell' )
					BroadcastLocalizedMessage(DeathMessageClass, 2, Other.PlayerReplicationInfo, None);
				else if ( damageType == 'Eradicated' )
					BroadcastLocalizedMessage(DeathMessageClass, 3, Other.PlayerReplicationInfo, None);
				else if ( damageType == 'Drowned' )
					BroadcastLocalizedMessage(DeathMessageClass, 4, Other.PlayerReplicationInfo, None);
				else if ( damageType == 'Burned' )
					BroadcastLocalizedMessage(DeathMessageClass, 5, Other.PlayerReplicationInfo, None);
				else if ( damageType == 'Corroded' )
					BroadcastLocalizedMessage(DeathMessageClass, 6, Other.PlayerReplicationInfo, None);
				else if ( damageType == 'Mortared' )
					BroadcastLocalizedMessage(DeathMessageClass, 7, Other.PlayerReplicationInfo, None);
				else
					BroadcastLocalizedMessage(DeathMessageClass, 1, Other.PlayerReplicationInfo, None);
			}
		} 
		else 
		{
			if ( Killer.bIsPlayer && Killer.PlayerReplicationInfo != None )
			{
				KillerWeapon = "None";
				if (Killer.Weapon != None)
					KillerWeapon = Killer.Weapon.ItemName;
				OtherWeapon = "None";
				if (Other.Weapon != None)
					OtherWeapon = Other.Weapon.ItemName;
				if ( Killer.PlayerReplicationInfo.Team == Other.PlayerReplicationInfo.Team )
				{
					if ( LocalLog != None )
						LocalLog.LogTeamKill(
							Killer.PlayerReplicationInfo.PlayerID,
							Other.PlayerReplicationInfo.PlayerID,
							KillerWeapon,
							OtherWeapon,
							damageType
						);
					if ( WorldLog != None )
						WorldLog.LogTeamKill(
							Killer.PlayerReplicationInfo.PlayerID,
							Other.PlayerReplicationInfo.PlayerID,
							KillerWeapon,
							OtherWeapon,
							damageType
						);
				} else {
					if ( LocalLog != None )
						LocalLog.LogKill(
							Killer.PlayerReplicationInfo.PlayerID,
							Other.PlayerReplicationInfo.PlayerID,
							KillerWeapon,
							OtherWeapon,
							damageType
						);
					if ( WorldLog != None )
						WorldLog.LogKill(
							Killer.PlayerReplicationInfo.PlayerID,
							Other.PlayerReplicationInfo.PlayerID,
							KillerWeapon,
							OtherWeapon,
							damageType
						);
				}
				if (!bSpecialDamage && (Other != None))
				{
					BroadcastRegularDeathMessage(Killer, Other, damageType);
				}
			}
		}
	}
	ScoreKill(Killer, Other);
}



final function bool RestartPlayer_Original( Pawn aPlayer);
function bool RestartPlayer_Proxy( Pawn aPlayer )	
{
	local bool FoundStart;

	class'XC_Engine_Pawn'.default.bDisableTeamEncroach = true;
	FoundStart = RestartPlayer_Original( APlayer);
	class'XC_Engine_Pawn'.default.bDisableTeamEncroach = false;
	return FoundStart;
}
