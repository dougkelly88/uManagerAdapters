///////////////////////////////////////////////////////////////////////////////
// FILE:          FianiumSC.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Prototype implementation of Fianium SCxxx supercontiuum sources. 
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
//-----------------------------------------------------------------------------
// Command set, for reference:
//-----------------------------------------------------------------------------
//Command	Data	Description
//-----------------------------------------------------------------------------
//A?				Get Alarms
//A=		0		Clear all alarms
//B?				Get back reflection photodiode value
//H?				Display list of commands
//I?				Get status display interval
//I=		1		Set status display interval
//J?				Get laser serial number
//M?				Get laser control mode
//M=		0,1,2	Set laser control mode - pot control, usb control, external 
//					voltage control. Modified with key in off position. 
//P?				Get preamplifier photodiode value
//Q?				Get amplifier control DAC value
//Q=				Set amplifier current control DAC value in USB mode
//S?				Get maximum DAC value
//V?				Get control software version and release date
//W?				Get laser operating time counter
//X?				Get status display mode
//X=		0,1		Set status display mode
//-----------------------------------------------------------------------------
//
//-------
//
//#ifndef _FianiumSC_H_
//#define _FianiumSC_H_

#include "../../MMDevice/DeviceBase.h"
#include "../../MMDevice/ImgBuffer.h"
#include "../../MMDevice/DeviceThreads.h"
#include "../../MMDevice/MMDevice.h"
#include "../../MMDevice/ModuleInterface.h"

#include <cstdio>
#include <string>
#include <iostream>
#include <math.h>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <map>
#include <algorithm>
#include <fstream>
#include <boost/algorithm/string.hpp>
#include <vector>

class FianiumSC : public CGenericBase<FianiumSC>
{
public:
	FianiumSC();
	~FianiumSC();

	std::string port_;

	// MMDevice API
	// ------------
	int Initialize();
	int Shutdown();

	void GetName(char* name) const;
	bool Busy() {return false;};

	// action interface
	// ----------------
	int OnPort(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnSerialNumber(MM::PropertyBase* pProp, MM::ActionType eAct);	
	int OnOperatingTime(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPowerOutput(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnToggleOnOff(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnRepRate(MM::PropertyBase* pProp, MM::ActionType eAct);

	// instrument interface
	// --------------------
	int GetSerialNumber(std::string &serial);
	int GetRunTimeMins(long &mins);
	
	// Utils
	// ----------------
	int NumericSet(std::string cmd, long val);
	int NumericGet(std::string cmd, long &val);
	std::string trim(const std::string& str, const std::string& whitespace);

private:
	bool initialized_;
	long serial_;
	long reprate_;
	long answerTimeoutMs_;

	long percentOutput_;
	long operatingTime_;
	long maxDAC_;
	bool toggleOn_;

	// Command set vars
	// -----------------
	std::string serialstr_;
	std::string freqstr_;
	std::string maxDACstr_;
	std::string DACstr_;
	std::string optimestr_;
	std::string getcmdstr_;
	std::string setcmdstr_;
	std::string termstr_;
};

//#endif //_FianiumSC_H_
