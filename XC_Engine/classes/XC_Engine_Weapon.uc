class XC_Engine_Weapon expands Weapon
	abstract;


//Fix message spam
function Weapon WeaponChange( byte F )
{	
	local Weapon newWeapon;
	 
	if ( InventoryGroup == F )
	{
		if ( (AmmoType != None) && (AmmoType.AmmoAmount <= 0) )
		{
			if ( Inventory != None ) //newWeapon always NULL before this
				newWeapon = Inventory.WeaponChange(F);
			if ( newWeapon == None )
			{
				if ( class'XC_Engine_PlayerPawn'.default.GW_TimeSeconds != Level.TimeSeconds ) //Antispam, fixes bandwidth exploit
				{
					class'XC_Engine_PlayerPawn'.default.GW_TimeSeconds = Level.TimeSeconds;
					class'XC_Engine_PlayerPawn'.default.GW_Counter = 0;
				}
				if ( class'XC_Engine_PlayerPawn'.default.GW_Counter++ < 3 )
					Pawn(Owner).ClientMessage( ItemName$MessageNoAmmo );		
			}
			return newWeapon;
		}		
		return self;
	}
	if ( Inventory != None )
		return Inventory.WeaponChange(F);
	//Implicity NULL return
}


function CheckVisibility()
{
	local Pawn PawnOwner;

	PawnOwner = Pawn(Owner);
	if (PawnOwner != None && PawnOwner.Health > 0)
	{
		if( PawnOwner.bHidden && (PawnOwner.Visibility < PawnOwner.Default.Visibility) )
		{
			PawnOwner.bHidden = false;
			PawnOwner.Visibility = PawnOwner.Default.Visibility;
		}
	}
}
