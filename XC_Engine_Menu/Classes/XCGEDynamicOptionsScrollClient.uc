class XCGEDynamicOptionsScrollClient expands UWindowScrollingDialogClient;


function Created()
{
	ClientClass = class'XCGEDynamicOptionsClientWindow';
	FixedAreaClass = None;
	Super.Created();
}
