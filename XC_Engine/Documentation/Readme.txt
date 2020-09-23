XC_Engine - XC_GameEngine extension for UT99 v469a by Higor.


===========
Setting up:
===========
Place XC_Engine files in your ~UT/System/ directory.

/** Auto-installer scripts

Run XC_Enable.bat/XC_Enable_nohome.sh scripts in order to auto-config XC_Engine stuff
The scripts will enable the engine, net driver and editor addons.

See "XC_Setup.txt" for more info.
**/

In case the above fails, or a different setup is needed, follow these steps.
The new GameEngine we want to load has to be specified in UnrealTournament.ini (or it's server equivalent) as follows.

[Engine.Engine]
;GameEngine=Engine.GameEngine
GameEngine=XC_Engine.XC_GameEngine
;NetworkDevice=IpDrv.TcpNetDriver
NetworkDevice=XC_IpDrv.XC_TcpNetDriver

Be adviced, when editing ServerPackages and ServerActors in a XC_Engine server, find the [XC_Engine.XC_GameEngine] entry!!!
Either remove it (and apply on GameEngine), or apply the changes on said (XC_GameEngine) entry instead.

Safe to use in v469, and on ACE servers since most hacks are reverted during online sessions.


=================
Features:
=================

- Global
Makes several properties from native only classes visible to UnrealScript, player commands and edit window (win32).
* See "Object properties.txt" for a list of newly visible properties.
Collision Grid replacing the old hash, loaded from CollisionGrid (.dll/.so)
Log file size reduction by grouping log spam and displaying how much log messages repeat.
UnrealScript patcher for servers and offline play, allows replacement of code in runtime.
IPv6 support through XC_IpDrv.

- Server
Moving Brush Tracker in Dedicated servers (movers block visibility checks), specific maps can be ignored.
* See "Server Exploits" for a list of patched exploits.
* See "Enhanced Netcode" for changes in relevancy netcode.
* See "TravelManager" for info on coop server enhancements.
Ability to send maps marked as 'no download' (Unreal SP content for example).

- Client / Player:
Prevents servers from using 'Open' and 'ClientTravel' commands to open local files on the client.
Clients no longer send options 'Game' and 'Mutator' in their login string.
More info displayed during file download: amount of files, data pending installation.
* See "AutoCacheConverter.txt" for info on the ingame cache converter.


====================
Other documentation:
====================
- LZMA
- Editor
- S3TC in Editor
- Paths Builder
- Object properties
- Self Dynamic Loading
- Script Compiler


================
Extra commands.
Check other documentation files for more commands.
================
- EditObject Name=Objectname Skip=skipcount
Client, Win32 only.
Brings up a property editor dialog of an object with a specified name.
Skip= is optional and can be used to bring up a newer objects with said name.

Example: "EditObject Name=MyLevel Skip=1" Brings up play level's XLevel properties.
Example: "EditObject Name=MyLevel" Brings up Entry level's XLevel properties.

- DumpObject Name=Objectname
Dumps object in question's memory block into a file (with the object's name), only dumps the first object with matching name.
If the object is a UFunction, then it will also save a file name FUNCTIONDATA.bin with the script code (serialized TArray<BYTE>).

- LogFields Name=classname
Logs all of the UnrealScript visible properties of the specified class, with property flags, offset, size and array count.
Boolean properties have their bitmask info logged instead of array size.

- LogClassSizes Outer=packagename(optional)
Prints in log a huge list of classes and their size in memory.
If the Outer=packagename parameter isn't used (or fails), it will print all classes's sizes.

- ToggleDebugLogs - DebugLogs
Toggles additional logging, for developers.
Disabled by default, saved in [XC_Engine.XC_GameEngine] config entry.

- ToggleRelevancy - ToggleRelevant
Requires bUseLevelHook.
Toggles XC_Level relevancy loop on net servers, see "Relevancy loop.txt" for details.


====================================
Functions patched/hooked in runtime:
====================================
See XC_Engine_Actor and XC_Engine_UT99_Actor for a full list of script patches.


=================
Credits:
=================
I would like to thank my fellow betatesters
- Chamberly
- ~V~
- Nelsona
- SC]-[LONG_{HoF}
- $carface (and the legions of Siege apes)
- AnthRAX
- SicilianKill

And all of Cham's development server visitors for the help in bugfixing this.