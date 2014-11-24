///////////////////////////////////////////////////////////////////////////////
// FILE:          SingleEdge.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Implementation of Kentech single edge prototype high rate imager. 
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

#ifndef _SE_H_
#define _SE_H_

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



//////////////////////////////////////////////////////////////////////////////
// Error codes
//
#define ERR_UNKNOWN_POSITION 101
#define ERR_INITIALIZE_FAILED 102
#define ERR_WRITE_FAILED 103
#define ERR_CLOSE_FAILED 104
#define ERR_BOARD_NOT_FOUND 105
#define ERR_PORT_OPEN_FAILED 106
#define ERR_COMMUNICATION 107
#define ERR_NO_PORT_SET 108
#define ERR_VERSION_MISMATCH 109
#define ERR_UNRECOGNIZED_ANSWER 110
#define ERR_PORT_CHANGE_FORBIDDEN 111
#define ERR_OPENFILE_FAILED 112
#define ERR_CALIBRATION_FAILED 113
#define ERR_UNRECOGNISED_PARAM_VALUE 114

class KSE : public CGenericBase<KSE>
{
public:
	KSE();
	~KSE();

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
	int OnWidth(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnGain(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnBias(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnInhibit(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnRepRate(MM::PropertyBase* pProp, MM::ActionType eAct);

	// Utils
	// ----------------
	int PopulateCalibrationVectors(std::string path);


	int NextDelScan();
	int SetupSE();

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
	bool monostable_;
	long maxdelay_;
	long gain_;
	long dummy_gain_;
	long width_;
	long bias_;
	bool inhibited_;

	std::vector<int> delay_settings;
	std::vector<int> real_delays;
	std::vector<int> width_settings;
	std::vector<int> real_widths;
	std::vector<int> mcp_settings;
	std::vector<int> real_mcps;

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
	std::string gainstr_;
	std::string widthstr_;

};

#endif //_SE_H_