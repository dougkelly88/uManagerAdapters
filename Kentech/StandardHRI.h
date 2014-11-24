///////////////////////////////////////////////////////////////////////////////
// FILE:          StandardHRI.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Implementation of Kentech HRI delay generator device. 
//                
// AUTHOR:        Doug Kelly, dk1109@ic.ac.uk, 01/09/2014
//
// COPYRIGHT:     
// LICENSE:       This file is distributed under the BSD license.
//                License text is included with the source distribution.
//
//                This file is distributed in the hope that it will be useful,
//                but WITHOUT ANY WARRANTY; without even the implied warranty
//                of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
//                IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//                CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//                INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.
//

#ifndef _HRI_H_
#define _HRI_H_

#include "../../MMDevice/DeviceBase.h"
#include "../../MMDevice/ImgBuffer.h"
#include "../../MMDevice/DeviceThreads.h"
#include "../../MMDevice/MMDevice.h"

#include <cstdio>
#include <string>
#include <iostream>
#include <math.h>
#include "../../MMDevice/ModuleInterface.h"
#include <sstream>
#include <algorithm>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <map>
#include <algorithm>
#include <fstream>
#include <boost/algorithm/string.hpp>
#include <vector>

#include "Kentech.h"
#include "Utilities.h"

class KHRI : public CGenericBase<KHRI>
{
public:
	KHRI();
	~KHRI();

	std::string port_;

	// MMDevice API
	// ------------
	int Initialize();
	int Shutdown();

	void GetName(char* name) const;
	bool Busy() {return false;};

	// action interface
	// ----------------
	//int OnDummy(MM::PropertyBase* pProp, MM::ActionType eAct);	//DEBUG
	int OnPort(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnMode(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnGain(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPolarity(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnTrigLogic(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnTrigImpedance(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnInhibit(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnWidth(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnDC(MM::PropertyBase* pProp, MM::ActionType eAct);


	// Utils
	// ----------------
	int PopulateModeVector(std::vector<std::string> &modeDescriptions, std::vector<int> &modeNumbers);
	int SetupHRI();

private:
	KUtils k_;

	bool initialized_;
	long answerTimeoutMs_;

	std::string modeString_;
	long modeNumber_;
	long width_;
	bool polarityPositive_;
	bool fiftyOhmInput_;
	bool eclTrigger_;
	//bool triggerDC_;
	long gain_;
	bool inhibited_;
	bool dcMode_;

	std::vector<std::string> modeDescriptions_;
	std::vector<int> modeNumbers_;
	std::vector<std::string> widths_;


	// Command set vars
	// -----------------
	std::string modestr_;
	std::string getcmdstr_;
	std::string setcmdstr_;
	std::string termstr_;
	std::string polstr_;
	std::string mcpstr_;
	std::string rfstr_;
	std::string onoffstr_;
	std::string trigstr_;
	

	// Mode selection
	// --------------
	#define INHIBIT		0
	#define RF			21
	#define LDC			22
	#define HDC			23
	#define DC			24
	// N.B. comb modes 200 - 1000 ps are modes 2-10. 

};

#endif //_HRI_H_