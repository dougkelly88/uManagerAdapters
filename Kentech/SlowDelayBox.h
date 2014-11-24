///////////////////////////////////////////////////////////////////////////////
// FILE:          SlowDelayBox.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Implementation of Kentech Precision Programmable delay generator. 
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


#ifndef _KSDB_H_
#define _KSDB_H_

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

#include "Kentech.h"
#include "Utilities.h"

class KSDB : public CGenericBase<KSDB>
{
public:
	KSDB();
	~KSDB();

	std::string port_;

	// MMDevice API
	// ------------
	int Initialize();
	int Shutdown();

	void GetName(char* name) const;
	bool Busy() {return false;};

	// action interface
	// ----------------
	int OnDelay(MM::PropertyBase* pProp, MM::ActionType eAct);
	//int OnDummy(MM::PropertyBase* pProp, MM::ActionType eAct);	//DEBUG
	int OnPort(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnCalibrationPath(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnCalibrate(MM::PropertyBase* pProp, MM::ActionType eAct);
	
	// Utils
	// ----------------
	int PopulateCalibrationVectors(std::string path);
	int SDBNumericSet(std::string cmd, long val);
	int SDBNumericGet(std::string cmd, long &val);
	std::string trim(const std::string& str, const std::string& whitespace);

private:
	KUtils k_;

	bool initialized_;
	long answerTimeoutMs_;
	std::string calibPath_;

	long delay_;
	bool calibrated_;
	long maxdelay_;

	std::vector<int> delay_settings;
	std::vector<int> real_delays;

	// Command set vars
	// -----------------
	std::string delstr_;
	std::string getcmdstr_;
	std::string setcmdstr_;
	std::string termstr_;
	std::string onoffstr_;
};

#endif //_KSDB_H_