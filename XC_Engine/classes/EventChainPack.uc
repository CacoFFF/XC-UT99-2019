//=============================================================================
// EventChainPack
//
// Use this to spawn handlers, useful for registering a single actor in 
// XC_Engine.ini while remaining compliant to the event chain system config.
//=============================================================================
class EventChainPack expands EventChainSystem;

event XC_Init()
{
	local Actor A;
	
	ForEach DynamicActors( Class, A)
		if ( (A.Class == Class) && (A != self) )
		{
			Destroy();
			return;
		}

	if ( class'XC_Engine_Actor_CFG'.default.bEventChainAddon )
		SpawnHandlers();
}

// Spawn event chain handlers here
function SpawnHandlers()
{
	CreateHandler( class'ElevatorReachspecHandler');
	CreateHandler( class'EngineTriggersHandler');
	CreateHandler( class'EngineMoversHandler');
	CreateHandler( class'EngineInventoryHandler');
	CreateHandler( class'EngineTeleportersHandler');
}

final function EventChainHandler CreateHandler( class<EventChainHandler> HandlerClass)
{
	local EventChainHandler NewHandler;
	NewHandler = Spawn( HandlerClass, self, 'EventChainHandler');
	NewHandler.XC_Init();
	return NewHandler;
}
