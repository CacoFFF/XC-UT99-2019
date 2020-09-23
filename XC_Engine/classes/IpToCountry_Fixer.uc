//=============================================================================
// IpToCountry_Fixer
// Script patcher fixer to prevent IpToCountry from crashing.
//=============================================================================
class IpToCountry_Fixer expands FV_Addons;
	
//Modify code here
function ScriptPatcherInit()
{
	local class<InternetInfo> HTTPClientClass;
	
	HTTPClientClass = class<InternetInfo>(FindObject( "IpToCountry.HTTPClient", class'Class'));
	if ( HTTPClientClass != None )
	{
		ReplaceFunction( class'IpToCountry_Fixer', HTTPClientClass, 'SetError_Original', 'SetError');
		ReplaceFunction( HTTPClientClass, class'IpToCountry_Fixer', 'SetError', 'SetError');
	}
}


// Original function
final function SetError_Original( int Code);

// Proxy to backup of original function.
function SetError( int Code)
{
	// Stop execution if port fails to bind.
	if ( Code == -2 )
	{
		Log("[IpToCountry] Error binding local port.");
		return;
	}
	
	SetError_Original( Code);
}
