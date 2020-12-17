class XCGEConfigOptionsClient expands UMenuPageWindow;

var native XC_Engine_Actor_CFG ConfigModule;
var native XC_GameEngine Engine;

// Lan player host
var UWindowCheckbox LanPlayerHostCheck;
var localized string LanPlayerHostText;
var localized string LanPlayerHostHelp;

// Any face on skin
var UWindowCheckbox AnyFaceOnSkinCheck;
var localized string AnyFaceOnSkinText;
var localized string AnyFaceOnSkinHelp;

// Developer logs
var UWindowCheckbox DevLogsCheck;
var localized string DevLogsText;
var localized string DevLogsHelp;

// Event Chain System
var UWindowCheckbox EventChainCheck;
var localized string EventChainText;
var localized string EventChainHelp;

// LZMA autocompressor
var UWindowCheckbox LZMAServerCheck;
var localized string LZMAServerText;
var localized string LZMAServerHelp;

// Save game system
var UWindowCheckbox SaveGameCheck;
var localized string SaveGameText;
var localized string SaveGameHelp;


//=====================
//==== Map List Sorting

// Map list sort label
var UMenuLabelControl MapListTitle;
var localized string MapListText;

// Sort by Folder
var UWindowCheckbox MapSortFolderCheck;
var localized string MapSortFolderText;
var localized string MapSortFolderHelp;

// Sort Inverted
var UWindowCheckbox MapSortInvertCheck;
var localized string MapSortInvertText;
var localized string MapSortInvertHelp;


//====================
//=== Dynamic settings

// Other settings label
var UMenuLabelControl DynamicSettingsTitle;
var localized string DynamicSettingsText;

// Renderer Settings button
var UWindowSmallButton RendererSettingsButton;
var localized string RendererSettingsText;
var localized string RendererSettingsHelp;

// AudioSub Settings button
var UWindowSmallButton AudioSubSettingsButton;
var localized string AudioSubSettingsText;
var localized string AudioSubSettingsHelp;

// NetDrv Settings button
var UWindowSmallButton NetDrvSettingsButton;
var localized string NetDrvSettingsText;
var localized string NetDrvSettingsHelp;


var float ControlOffset;

function bool GetScriptConfig()
{
	if ( ConfigModule == None )
		ConfigModule = XC_Engine_Actor_CFG( Class'XC_CoreStatics'.static.FindObject( "XC_Engine.GeneralConfig", class'XC_Engine_Actor_CFG'));
	return ConfigModule != None;
}

function SaveEngineConfig()
{
	if ( Engine == None )
		Engine = XC_GameEngine(Class'XC_CoreStatics'.static.FindObject( "XC_GameEngine0", class'XC_GameEngine'));
	Engine.SaveConfig();
}

function Created()
{
	local int ControlWidth, ControlLeft, ControlRight;
	local int CenterWidth, CenterPos;

	Super.Created();

	ControlWidth = WinWidth/2.5;
	ControlLeft = (WinWidth/2 - ControlWidth)/2;
	ControlRight = WinWidth/2 + ControlLeft;

	CenterWidth = (WinWidth/4)*3;
	CenterPos = (WinWidth - CenterWidth)/2;
	
	ControlOffset = 40;
	
	//========
	// Server
	
	// LanPlayerHost //1L
	LanPlayerHostCheck = UWindowCheckbox(CreateControl(class'UWindowCheckbox', ControlLeft, ControlOffset, ControlWidth, 1));
	if ( GetScriptConfig() )
		LanPlayerHostCheck.bChecked = ConfigModule.bListenServerPlayerRelevant;
	LanPlayerHostCheck.SetText(LanPlayerHostText);
	LanPlayerHostCheck.SetHelpText(LanPlayerHostHelp);
	LanPlayerHostCheck.SetFont(F_Normal);
	LanPlayerHostCheck.Align = TA_Right;
	// Any face on Skin // 1R
	AnyFaceOnSkinCheck = UWindowCheckbox(CreateControl(class'UWindowCheckbox', ControlRight, ControlOffset, ControlWidth, 1));
	if ( GetScriptConfig() )
		AnyFaceOnSkinCheck.bChecked = ConfigModule.bAnyFaceOnSkin;
	AnyFaceOnSkinCheck.SetText(AnyFaceOnSkinText);
	AnyFaceOnSkinCheck.SetHelpText(AnyFaceOnSkinHelp);
	AnyFaceOnSkinCheck.SetFont(F_Normal);
	AnyFaceOnSkinCheck.Align = TA_Right;
	ControlOffset += 21;

	ControlOffset += 10;

	// LZMAServer //2L
	LZMAServerCheck = UWindowCheckbox(CreateControl(class'UWindowCheckbox', ControlLeft, ControlOffset, ControlWidth, 1));
	LZMAServerCheck.bChecked = bool(GetPlayerOwner().ConsoleCommand("get XC_GameEngine bAutoCompressLZMA"));
	LZMAServerCheck.SetText(LZMAServerText);
	LZMAServerCheck.SetHelpText(LZMAServerHelp);
	LZMAServerCheck.SetFont(F_Normal);
	LZMAServerCheck.Align = TA_Right;
	// Any face on Skin // 2R
	SaveGameCheck = UWindowCheckbox(CreateControl(class'UWindowCheckbox', ControlRight, ControlOffset, ControlWidth, 1));
	SaveGameCheck.bChecked = bool(GetPlayerOwner().ConsoleCommand("get XC_GameEngine bEnhancedSaveGames"));
	SaveGameCheck.SetText(SaveGameText);
	SaveGameCheck.SetHelpText(SaveGameHelp);
	SaveGameCheck.SetFont(F_Normal);
	SaveGameCheck.Align = TA_Right;
	ControlOffset += 21;
	
	
	//=======
	// Debug
	ControlOffset += 10;

	// Developer logs //3L
	DevLogsCheck = UWindowCheckbox(CreateControl(class'UWindowCheckbox', ControlLeft, ControlOffset, ControlWidth, 1));
	DevLogsCheck.bChecked = bool(GetPlayerOwner().ConsoleCommand("get XC_GameEngine bEnableDebugLogs"));
	DevLogsCheck.SetText(DevLogsText);
	DevLogsCheck.SetHelpText(DevLogsHelp);
	DevLogsCheck.SetFont(F_Normal);
	DevLogsCheck.Align = TA_Right;
	// Event Chain System //3R
	EventChainCheck = UWindowCheckbox(CreateControl(class'UWindowCheckbox', ControlLeft, ControlOffset, ControlWidth, 1));
	if ( GetScriptConfig() )
		EventChainCheck.bChecked = ConfigModule.bEventChainAddon;
	EventChainCheck.SetText(EventChainText);
	EventChainCheck.SetHelpText(EventChainHelp);
	EventChainCheck.SetFont(F_Normal);
	EventChainCheck.Align = TA_Right;
	// 3R = nothing here
	ControlOffset += 25;
	
	
	//==========
	// Map List
	ControlOffset += 10;
	MapListTitle = UMenuLabelControl(CreateControl(class'UMenuLabelControl', CenterPos, ControlOffset, CenterWidth, 1)); 
	MapListTitle.SetText(MapListText);
	MapListTitle.Align = TA_Center;
	ControlOffset += 15;
	
	// Sort by Folder //Left
	MapSortFolderCheck = UWindowCheckbox(CreateControl(class'UWindowCheckbox', ControlLeft, ControlOffset, ControlWidth, 1));
	MapSortFolderCheck.bChecked = bool(GetPlayerOwner().ConsoleCommand("get XC_GameEngine bSortMaplistByFolder"));
	MapSortFolderCheck.SetText(MapSortFolderText);
	MapSortFolderCheck.SetHelpText(MapSortFolderHelp);
	MapSortFolderCheck.SetFont(F_Normal);
	MapSortFolderCheck.Align = TA_Right;
	// Sort Inverted //Right
	MapSortInvertCheck = UWindowCheckbox(CreateControl(class'UWindowCheckbox', ControlRight, ControlOffset, ControlWidth, 1));
	MapSortInvertCheck.bChecked = bool(GetPlayerOwner().ConsoleCommand("get XC_GameEngine bSortMaplistInvert"));
	MapSortInvertCheck.SetText(MapSortInvertText);
	MapSortInvertCheck.SetHelpText(MapSortInvertHelp);
	MapSortInvertCheck.SetFont(F_Normal);
	MapSortInvertCheck.Align = TA_Right;
	ControlOffset += 21;
	
	//==================
	// Settings buttons
	ControlOffset += 10;
	DynamicSettingsTitle = UMenuLabelControl(CreateControl(class'UMenuLabelControl', CenterPos, ControlOffset, CenterWidth, 1)); 
	DynamicSettingsTitle.SetText(DynamicSettingsText);
	DynamicSettingsTitle.Align = TA_Center;
	ControlOffset += 15;
	
	// Renderer //1Left
	RendererSettingsButton = UWindowSmallButton(CreateControl(class'UWindowSmallButton', ControlLeft, ControlOffset, ControlWidth, 16));
	RendererSettingsButton.SetText(RendererSettingsText);
	RendererSettingsButton.SetHelpText(RendererSettingsHelp);
	// AudioSub //1Right
	AudioSubSettingsButton = UWindowSmallButton(CreateControl(class'UWindowSmallButton', ControlLeft, ControlOffset, ControlWidth, 16));
	AudioSubSettingsButton.SetText(AudioSubSettingsText);
	AudioSubSettingsButton.SetHelpText(AudioSubSettingsHelp);
	ControlOffset += 21;
	// NetDrv //2Left
	NetDrvSettingsButton = UWindowSmallButton(CreateControl(class'UWindowSmallButton', ControlLeft, ControlOffset, ControlWidth, 16));
	NetDrvSettingsButton.SetText(NetDrvSettingsText);
	NetDrvSettingsButton.SetHelpText(NetDrvSettingsHelp);
}

function AfterCreate()
{
	DesiredWidth = 220;
	DesiredHeight = ControlOffset;
}

function BeforePaint(Canvas C, float X, float Y)
{
	local int ControlWidth, ControlLeft, ControlRight;
	local int CenterWidth, CenterPos;

	ControlWidth = WinWidth/2.5;
	ControlLeft = (WinWidth/2 - ControlWidth)/2;
	ControlRight = WinWidth/2 + ControlLeft;

	CenterWidth = (WinWidth/4)*3;
	CenterPos = (WinWidth - CenterWidth)/2;

	LanPlayerHostCheck.SetSize(ControlWidth, 1);
	LanPlayerHostCheck.WinLeft = ControlLeft;

	AnyFaceOnSkinCheck.SetSize(ControlWidth, 1);
	AnyFaceOnSkinCheck.WinLeft = ControlRight;

	LZMAServerCheck.SetSize(ControlWidth, 1);
	LZMAServerCheck.WinLeft = ControlLeft;

	SaveGameCheck.SetSize(ControlWidth, 1);
	SaveGameCheck.WinLeft = ControlRight;
	
	DevLogsCheck.SetSize(ControlWidth, 1);
	DevLogsCheck.WinLeft = ControlLeft;
	
	EventChainCheck.SetSize(ControlWidth, 1);
	EventChainCheck.WinLeft = ControlRight;

	
	MapListTitle.SetSize(CenterWidth, 1);
	MapListTitle.WinLeft = CenterPos;
	MapSortFolderCheck.SetSize(ControlWidth, 1);
	MapSortFolderCheck.WinLeft = ControlLeft;
	MapSortInvertCheck.SetSize(ControlWidth, 1);
	MapSortInvertCheck.WinLeft = ControlRight;
	
	DynamicSettingsTitle.SetSize(CenterWidth, 1);
	DynamicSettingsTitle.WinLeft = CenterPos;
	RendererSettingsButton.SetSize(ControlWidth, 16);
	RendererSettingsButton.WinLeft = ControlLeft;
	AudioSubSettingsButton.SetSize(ControlWidth, 16);
	AudioSubSettingsButton.WinLeft = ControlRight;
	NetDrvSettingsButton.SetSize(ControlWidth, 16);
	NetDrvSettingsButton.WinLeft = ControlLeft;
}

function Notify(UWindowDialogControl C, byte E)
{
	local XCGEDynamicOptionsWindow DW;

	Super.Notify(C, E);
	switch(E)
	{
	case DE_Change:
		switch(C)
		{
		case LanPlayerHostCheck:
			LanPlayerHostChecked();
			break;
		case AnyFaceOnSkinCheck:
			AnyFaceOnSkinChecked();
			break;
		case LZMAServerCheck:
			LZMAServerChecked();
			break;
		case SaveGameCheck:
			SaveGameChecked();
			break;
		case DevLogsCheck:
			DevLogsChecked();
			break;
		case EventChainCheck:
			EventChainChecked();
			break;
		case MapSortFolderCheck:
			MapSortFolderChecked();
			break;
		case MapSortInvertCheck:
			MapSortInvertChecked();
			break;
		}
		break;
	case DE_Click:
		switch (C)
		{
			case RendererSettingsButton:
				class'XCGEDynamicOptionsClientWindow'.default.DynamicClassString = GetPlayerOwner().ConsoleCommand("get ini:Engine.Engine.GameRenderDevice Class");
				DW = XCGEDynamicOptionsWindow(Root.CreateWindow(class'XCGEDynamicOptionsWindow', 100, 100, 100, 100));
				DW.WindowTitle = DW.WindowTitle @ "-" @ class'XCGEDynamicOptionsClientWindow'.default.DynamicClassString;
				break;
			case AudioSubSettingsButton:
				class'XCGEDynamicOptionsClientWindow'.default.DynamicClassString = GetPlayerOwner().ConsoleCommand("get ini:Engine.Engine.AudioDevice Class");
				DW = XCGEDynamicOptionsWindow(Root.CreateWindow(class'XCGEDynamicOptionsWindow', 100, 100, 100, 100));
				DW.WindowTitle = DW.WindowTitle @ "-" @ class'XCGEDynamicOptionsClientWindow'.default.DynamicClassString;
				break;
			case NetDrvSettingsButton:
				DynamicLoadObject( "ini:Engine.Engine.NetworkDevice", class'class', true); //Bind
				class'XCGEDynamicOptionsClientWindow'.default.DynamicClassString = GetPlayerOwner().ConsoleCommand("get ini:Engine.Engine.NetworkDevice Class");
				DW = XCGEDynamicOptionsWindow(Root.CreateWindow(class'XCGEDynamicOptionsWindow', 100, 100, 100, 100));
				DW.WindowTitle = DW.WindowTitle @ "-" @ class'XCGEDynamicOptionsClientWindow'.default.DynamicClassString;
				break;
		}
		break;

	}
}


function MapSortFolderChecked()
{
	GetPlayerOwner().ConsoleCommand("set XC_GameEngine bSortMaplistByFolder " $ int(MapSortFolderCheck.bChecked));
	SaveEngineConfig();
}

function MapSortInvertChecked()
{
	GetPlayerOwner().ConsoleCommand("set XC_GameEngine bSortMaplistInvert " $ int(MapSortInvertCheck.bChecked));
	SaveEngineConfig();
}

function LanPlayerHostChecked()
{
	if ( GetScriptConfig() )
	{
		ConfigModule.bListenServerPlayerRelevant = LanPlayerHostCheck.bChecked;
		ConfigModule.SaveConfig();
	}
}

function AnyFaceOnSkinChecked()
{
	if ( GetScriptConfig() )
	{
		ConfigModule.bAnyFaceOnSkin = AnyFaceOnSkinCheck.bChecked;
		ConfigModule.SaveConfig();
		ConfigModule.default.bAnyFaceOnSkin = ConfigModule.bAnyFaceOnSkin;
	}
}

function EventChainChecked()
{
	if ( GetScriptConfig() )
	{
		ConfigModule.bEventChainAddon = EventChainCheck.bChecked;
		ConfigModule.SaveConfig();
	}
}

function LZMAServerChecked()
{
	GetPlayerOwner().ConsoleCommand("set XC_GameEngine bAutoCompressLZMA " $ int(LZMAServerCheck.bChecked));
	SaveEngineConfig();
}

function SaveGameChecked()
{
	GetPlayerOwner().ConsoleCommand("set XC_GameEngine bEnhancedSaveGames " $ int(SaveGameCheck.bChecked));
	SaveEngineConfig();
}

function DevLogsChecked()
{
	GetPlayerOwner().ConsoleCommand("ToggleDebugLogs");
	DevLogsCheck.bChecked = bool(GetPlayerOwner().ConsoleCommand("get XC_GameEngine bEnableDebugLogs"));
}




defaultproperties
{
	DevLogsText="Developer log"
	DevLogsHelp="If checked, XC_Engine will print additional information to the game/server log."
	LanPlayerHostText="LAN Host Skin"
	LanPlayerHostHelp="If checked, LAN games hosts will have their player, skin and voice automatically setup for download."
	AnyFaceOnSkinText="Any face on skin"
	AnyFaceOnSkinHelp="If checked, it'll be possible to select any compatible face on a given skin."
	LZMAServerText="LZMA compressor"
	LZMAServerHelp="If checked, XC_Engine server will generate a temporary LZMA package cache for download by XC_Engine clients"
	SaveGameText="New Save system"
	SaveGameHelp="Replaces the SaveGame system with an enhanced version. Commands: SaveGame, LoadGame, LoadGameServer + [save name]"
	MapListText="Map List Sorting"
	MapSortFolderText="By Directory"
	MapSortFolderHelp="Sorts the map list by directories instead of globally."
	MapSortInvertText="Inverted"
	MapSortInvertHelp="Reverses the map list order."
	EventChainText="Event Chain System"
	EventChainHelp="Improves bot AI by making triggers and other mechanisms interact with the path network."
	DynamicSettingsText="Other Settings"
	RendererSettingsText="Renderer Settings"
	RendererSettingsHelp="Allows the user to modify the render device's options."
	AudioSubSettingsText="Audio Driver Settings"
	AudioSubSettingsHelp="Allows the user to modify the audio subsystem's options."
	NetDrvSettingsText="Network Driver Settings"
	NetDrvSettingsHelp="Allows the user to modify the network driver's options."
}


