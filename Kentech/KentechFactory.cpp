//For reference: http://stackoverflow.com/questions/12573816/what-is-an-undefined-reference-unresolved-external-symbol-error-and-how-do-i-fix

#include "DelayBoxes.h"
#include "KentechFactory.h"

AbstractDelayBox * KentechFactory::MakeDelayBox(std::string boxId)
{
	if ( boxId.compare("HDG") == 0 )
		return new HDG;
	else if ( boxId.compare("HDG800") == 0 )
		return new HDG800;
	else if ( boxId.compare("SlowDelayBox") == 0 )
		return new SlowDelayBox;
	else
		return new HDG; // return HDG by default - better to return an error?
}

std::vector<std::string> KentechFactory::populateBoxTypes()
{
	std::vector<std::string> boxTypes;
	boxTypes.push_back("HDG");
	boxTypes.push_back("HDG800");
	boxTypes.push_back("SlowDelayBox");

	return boxTypes;	//careful! - optimisation might cause undefined behaviour?!
}