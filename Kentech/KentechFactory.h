#pragma once
#include "DelayBoxes.h"
#include <string>
#include <vector>


class KentechFactory
{
public:
	static AbstractDelayBox * MakeDelayBox(std::string boxId);
	static std::vector<std::string> populateBoxTypes();
};