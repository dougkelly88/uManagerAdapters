///////////////////////////////////////////////////////////////////////////////
// FILE:          HDG.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Implementation of Kentech HDG delay generator device. 
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

#ifndef _HDG_H_
#define _HDG_H_

#include "../../MMDevice/DeviceBase.h"
#include "../../MMDevice/ImgBuffer.h"
#include "../../MMDevice/DeviceThreads.h"
#include "../../MMDevice/MMDevice.h"

#include <cstdio>
#include <string>
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

#include "Kentech.h"
#include "Utilities.h"

class KHDG : public CGenericBase<KHDG>
{
public:
	KHDG();
	~KHDG();

	std::string port_;

	// MMDevice API
	// ------------
	int Initialize();
	int Shutdown();

	void GetName(char* name) const;
	bool Busy() {return false;};

	// action interface
	// ----------------
	int OnBoxType(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnDelay(MM::PropertyBase* pProp, MM::ActionType eAct);
	//int OnDummy(MM::PropertyBase* pProp, MM::ActionType eAct);	//DEBUG
	int OnPort(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnCalibrationPath(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnCalibrate(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnScanMode(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnAddScanPos(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnScanPos(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPolarity(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnTrigCoupling(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnTrigImpedance(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnTrigAttenuation(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnFrequency(MM::PropertyBase* pProp, MM::ActionType eAct);


	// Utils
	// ----------------
	int PopulateCalibrationVectors(std::string path);

	int NextDelScan();
	int SetupHDG();

private:
	KUtils k_;

	bool initialized_;
	long answerTimeoutMs_;
	std::string boxType_;
	std::string calibPath_;

	long delay_;
	bool calibrated_;
	bool scanModeOn_;
	bool polarityPositive_;
	bool fiftyOhmInput_;
	bool triggerAttenuated_;
	bool triggerDC_;
	bool eightyMhz_;

	long maxdelay_;

	std::vector<int> delay_settings;
	std::vector<int> real_delays;

	/*ScanProperties scanProps_;*/

	int SetDelay(long delay);
	int GetDelay(long &delay);

	int SetPolarity(bool polarityPositive);
	int SetUseMonostable(bool mono);


	// Command set vars
	// -----------------
	std::string delstr_;
	std::string getcmdstr_;
	std::string setcmdstr_;
	std::string termstr_;
	std::string polstr_;
	std::string couplingstr_;
	std::string impedancestr_;
	std::string attenuationstr_;
	std::string threshstr_;
	std::string trigopstr_;
	std::string onoffstr_;
};

#endif //_HDG_H_