class XCGEDynamicOptionsClientWindow expands UMenuPageWindow;

struct DynamicOption
{
	var() UWindowDialogControl Control;
	var() name PropertyName;
	var() bool bNumericOnly;
	var() bool bNumericFloat;
	var() float RangeMin;
	var() float RangeMax;
	var() float RangeStep;
};

var bool bChanging;
var int LastFix;
var string DynamicClassString;
var class<Object> Prototype;
var array<DynamicOption> Options;

var int ControlOffset;
var Object ObjEnum;

function Created()
{
	local int CenterWidth, CenterPos;

	local Property P;
	local DynamicOption NewOption;
	local int i, j;
	local string ControlName, ControlHelp;

	local array<Property> ConfigProps;
	
	Super.Created();

	CenterWidth = (WinWidth/5)*4;
	CenterPos = (WinWidth - CenterWidth)/2;
	ControlOffset = 10;
	
	if ( Prototype == None )
	{
		Log("Getting window prototype "$default.DynamicClassString$"...");
		SetPropertyText( "Prototype", default.DynamicClassString);
		if ( Prototype == None )
			return;
		default.DynamicClassString = string(Prototype);
	}
	
	DynamicClassString = string(Prototype);

	// Field iterator goes in reverse order, fix it here
	ForEach class'XC_CoreStatics'.static.AllObjects( class'Property', P, Prototype)
		if ( IsConfigProperty(P) )
		{
			ConfigProps.Insert( 0, 1);
			ConfigProps[0] = P;
		}
	
	For ( i=0; i<ConfigProps.Length; i++)
	{
		P = ConfigProps[i];
		if ( HandleProperty(P,NewOption,CenterPos,ControlOffset,CenterWidth,16) )
		{
			// Setup controls
			ControlName = LocalizePropertyName(P);
			ControlHelp = "Changes the"@ControlName@"option for"@string(Prototype.Name)$"."; // TODO: Custom help!
			NewOption.Control.SetText( ControlName );
			NewOption.Control.SetHelpText( ControlHelp );
			NewOption.Control.SetFont(F_Normal);
			NewOption.PropertyName = P.Name;
			
			// Setup checkboxes
			if ( UWindowCheckBox(NewOption.Control) != None )
			{
				NewOption.Control.Align = TA_Left;
			}
			// Setup combo controls
			else if ( UWindowComboControl(NewOption.Control) != None )
			{
				UWindowComboControl(NewOption.Control).SetEditable(false);
				UWindowComboControl(NewOption.Control).EditBoxWidth *= 3.0;
			}
			// Setup slider controls
			else if ( UWindowHSliderControl(NewOption.Control) != None )
			{
				UWindowHSliderControl(NewOption.Control).SetRange( NewOption.RangeMin, NewOption.RangeMax, NewOption.RangeStep);
				UWindowHSliderControl(NewOption.Control).SliderWidth *= 3.0;
			}
			// Setup edit controls
			else if ( UWindowEditControl(NewOption.Control) != None )
			{
				UWindowEditControl(NewOption.Control).SetNumericOnly( NewOption.bNumericOnly );
				UWindowEditControl(NewOption.Control).SetNumericFloat( NewOption.bNumericFloat );
				UWindowEditControl(NewOption.Control).SetDelayedNotify(True);
				UWindowEditControl(NewOption.Control).EditBoxWidth *= 3.0;
				UWindowEditControl(NewOption.Control).EditBox.Align = TA_Left;
			}

			UpdateValue( NewOption.Control, GetPlayerOwner().ConsoleCommand("get"@DynamicClassString@string(P.Name)));
			
			ControlOffset += 21;

			j = Options.Length;
			Options.Insert( j, 1);
			Options[j] = NewOption;
		}
	}
	ControlOffset += 10;
}

function AfterCreate()
{
	DesiredWidth = 220;
	DesiredHeight = ControlOffset + 24; //Account for close button in parent
}

function BeforePaint( Canvas C, float X, float Y)
{
	local int i;
	local int CenterWidth, CenterPos;

	CenterWidth = (WinWidth/4)*3;
	CenterPos = (WinWidth - CenterWidth)/2;

	for ( i=0; i<Options.Length; i++)
		if ( Options[i].Control != None )
		{
			Options[i].Control.SetSize( CenterWidth, 16);
			Options[i].Control.WinLeft = CenterPos;
		}
		
	// Fix one option at a time
	if ( ++LastFix >= Options.Length )
		LastFix = 0;
	if ( Options[LastFix].Control != None )
		UpdateValue( Options[LastFix].Control, GetPlayerOwner().ConsoleCommand("get"@DynamicClassString@string(Options[LastFix].PropertyName)));
}

function bool HandleProperty( Property P, out DynamicOption Op, int X, int Y, int XL, int YL)
{
	local name EnumName;
	local int i, ArrayDim;
	
	ArrayDim = int(P.GetPropertyText("ArrayDim"));
	if ( ArrayDim > 1 ) //Handle later
		return false;
		
	Op.Control = None;
	
	// Special cases here
	if ( (P.Name == 'MaxAnisotropy') && (P.IsA('IntProperty') || P.IsA('FloatProperty')) )
	{
		Op.Control = CreateControl(class'UWindowComboControl', X, Y, XL, YL);
		UWindowComboControl(Op.Control).AddItem( "Disabled", "0");
		UWindowComboControl(Op.Control).AddItem( "2x", "2");
		UWindowComboControl(Op.Control).AddItem( "4x", "4");
		UWindowComboControl(Op.Control).AddItem( "8x", "8");
		UWindowComboControl(Op.Control).AddItem( "16x", "16");
		return true;
	}
	
	if ( P.IsA('BoolProperty') )
		Op.Control = CreateControl(class'UWindowCheckBox', X, Y, XL, YL);
	else if ( P.IsA('ByteProperty') )
	{
		SetPropertyText("ObjEnum",P.GetPropertyText("Enum"));
		if ( ObjEnum != None )
		{
			Op.Control = CreateControl(class'UWindowComboControl', X, Y, XL, YL);
			for ( i=0; i<256; i++)
			{
				EnumName = GetEnum( ObjEnum, i);
				if ( EnumName == '' )
					break;
				UWindowComboControl(Op.Control).AddItem( string(EnumName), string(EnumName)); //TODO: Enum name translation
			}
		}
		else
		{
			Op.Control = CreateControl(class'UWindowHSliderControl', X, Y, XL, YL);
			Op.RangeMin = 0;
			Op.RangeMax = 255;
			Op.RangeStep = 0;
		}
	}
	else if ( P.IsA('IntProperty') || P.IsA('FloatProperty') || P.IsA('StringProperty') )
	{
		Op.Control = CreateControl(class'UWindowEditControl', X, Y, XL, YL);
		Op.bNumericOnly = P.IsA('IntProperty');
		Op.bNumericFloat = P.IsA('FloatProperty');
	}
	
	return Op.Control != None;
}

function Notify(UWindowDialogControl C, byte E)
{
	local int i;

	Super.Notify(C, E);
	
	// Internal correction in progress, ignore
	if ( bChanging )
		return;
	
	switch(E)
	{
	case DE_Change:
		
		for ( i=0; i<Options.Length; i++)
		{
			if ( C == Options[i].Control )
			{
				UpdateOption( C, Options[i].PropertyName);
				break;
			}
		}
		break;
	}
}

// Menu -> Object
function UpdateOption( UWindowDialogControl Control, name PropertyName)
{
	local string Value;
	
	if ( UWindowCheckBox(Control) != None )
		Value = Control.GetPropertyText("bChecked");
	else if ( UWindowComboControl(Control) != None )
		Value = UWindowComboControl(Control).GetValue2();
	else if ( UWindowHSliderControl(Control) != None )
		Value = Control.GetPropertyText("Value");
	else if ( UWindowEditControl(Control) != None )
		Value = UWindowEditControl(Control).GetValue();

	GetPlayerOwner().ConsoleCommand("set"@DynamicClassString@PropertyName@Value);
	
	// Fix
	UpdateValue( Control, GetPlayerOwner().ConsoleCommand("get"@DynamicClassString@PropertyName) );
}

// Object -> Menu
function UpdateValue( UWindowDialogControl Control, string Value)
{
	local int i;

	bChanging = true;
	
	// Update checkbox
	if ( UWindowCheckBox(Control) != None )
		Control.SetPropertyText("bChecked",Value);
	// Update combo control
	else if ( UWindowComboControl(Control) != None )
	{
		i = UWindowComboControl(Control).FindItemIndex2( Value, true);
		if ( (i == -1) && (string(float(Value)) == Value) && (float(int(Value)) == float(Value)) ) //Try float -> int
			i = UWindowComboControl(Control).FindItemIndex2( string(int(Value)), true);
		if ( i == -1 )
			UWindowComboControl(Control).SetValue( Value, Value);
		else
			UWindowComboControl(Control).SetSelectedIndex(i);
	}
	// Update slider control
	else if ( UWindowHSliderControl(Control) != None )
		UWindowHSliderControl(Control).SetValue( float(Value) );
	// Update edit control
	else if ( UWindowEditControl(Control) != None )
	{
		if ( !UWindowEditControl(Control).EditBox.bChangePending )
			UWindowEditControl(Control).SetValue(Value);
	}
		
	bChanging = false;
}

function string LocalizePropertyName( Property P)
{
	if ( P.IsA('BoolProperty') && (Left(string(P.Name),1) == "b") )
		return Mid(string(P.Name),1);
	return string(P.Name);
}

static function bool IsConfigProperty( Property P)
{
	return (int(P.GetPropertyText("PropertyFlags")) & 0x00004000) != 0;
}


