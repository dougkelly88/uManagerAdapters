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

#include "FianiumSC.h"

const char* g_FianiumDeviceName = "FianiumSC";

///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////

MODULE_API void InitializeModuleData()
{
	RegisterDevice(g_FianiumDeviceName, MM::GenericDevice, "Fianium Supercontinuum Source");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
	if (deviceName == 0)
		return 0;

	if (strcmp(deviceName, g_FianiumDeviceName) == 0)
	{
		return new FianiumSC();
	}
	// ...supplied name not recognized
	return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
	delete pDevice;
}

///////////////////////////////////////////////////////////////////////////////
// FianiumSC implementation
///////////////////////////////////////////////////////////////////////////////

FianiumSC::FianiumSC() :
CGenericBase<FianiumSC> (), 
	initialized_(false), 
	port_("Undefined"),
	toggleOn_(false),
	answerTimeoutMs_(1000)
{
	InitializeDefaultErrorMessages();
		
	CPropertyAction * pAct = new CPropertyAction(this, &FianiumSC::OnPort);
	CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);

	// Command set vars
	// -----------------
	maxDACstr_ = "s";
	freqstr_ = "r";
	serialstr_ = "j";
	DACstr_ = "q";
	optimestr_ = "w";
	getcmdstr_ = "?";
	setcmdstr_ = "=";
	termstr_ = "\n";
	
}

FianiumSC::~FianiumSC()
{

}

void FianiumSC::GetName(char* name) const
{
	// Return the name used to refer to this device adapter
	CDeviceUtils::CopyLimitedString(name, g_FianiumDeviceName);
}

int FianiumSC::Initialize()
{
	if (initialized_)
		return DEVICE_OK;

	long val;
	// Get laser serial number
	NumericGet(serialstr_, val);
	serial_ = val;
	//// Get operating time
	GetRunTimeMins(val);
	operatingTime_ = val;
	// Get maximum DAC value
	NumericGet(maxDACstr_, val);
	maxDAC_ = val;
	// Get reprate
	NumericGet(freqstr_, val);
	reprate_ = val;

	int nRet = CreateStringProperty(MM::g_Keyword_Name, g_FianiumDeviceName, true);
	if (DEVICE_OK != nRet)
		return nRet;

	// Power output
	CPropertyAction *pAct = new CPropertyAction (this, &FianiumSC::OnPowerOutput);
	nRet = CreateIntegerProperty("Power output (%)", 0, false, pAct, false);
	if (DEVICE_OK != nRet)
		return nRet;
	percentOutput_ = 0;
	nRet = SetPropertyLimits("Power output (%)", 0, 100);
	if (nRet != DEVICE_OK)
		return nRet;

	// Toggle power to zero/back again
	pAct = new CPropertyAction (this, &FianiumSC::OnToggleOnOff);
	nRet = CreateProperty("LaserOn?", "Off", MM::String, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;
	AddAllowedValue("LaserOn?","On");
	AddAllowedValue("LaserOn?","Off");

	// Operating time
	pAct = new CPropertyAction (this, &FianiumSC::OnOperatingTime);
	nRet = CreateIntegerProperty("OperatingTime(Mins)", operatingTime_, true, pAct);
	if (DEVICE_OK != nRet)
		return nRet;

	// Laser serial
	pAct = new CPropertyAction (this, &FianiumSC::OnSerialNumber);
	nRet = CreateIntegerProperty("LaserSerialNumber", serial_, true, pAct);
	if (DEVICE_OK != nRet)
		return nRet;

	// Rep rate
	pAct = new CPropertyAction (this, &FianiumSC::OnRepRate);
	nRet = CreateIntegerProperty("RepRate", reprate_, true, pAct);
	if (DEVICE_OK != nRet)
		return nRet;

	initialized_= true;

	return DEVICE_OK;
}

int FianiumSC::Shutdown()
{
	// note that sending "LOCAL" command returns control straight 
	// away, i.e. no response is delivered, so standard ToggleSet
	// cannot be used...
	int ret = SetProperty("LaserOn?", "Off");
	return ret;
	
}

///////////////////////////////////////////////////////////////////////////////
// FianiumSC action handlers
///////////////////////////////////////////////////////////////////////////////

int FianiumSC::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(port_.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		if (initialized_)
		{
			// revert
			pProp->Set(port_.c_str());
			return DEVICE_CAN_NOT_SET_PROPERTY; 
		}

		pProp->Get(port_);
	}

	return DEVICE_OK;
}

int  FianiumSC::OnPowerOutput(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	long val = 0;
	if (eAct == MM::BeforeGet)
	{
		val = percentOutput_;
		pProp->Set(val);
	}
	else if (eAct == MM::AfterSet)
	{
		int ret = DEVICE_OK;
		long percentOutput;
		long DACval;
		std::ostringstream os;
		pProp->Get(percentOutput);
		DACval = (long) (((float) percentOutput/100)*maxDAC_);

		if (toggleOn_)
			ret = NumericSet(DACstr_, DACval);
		if (ret == DEVICE_OK)
		{
			percentOutput_ = percentOutput;
			pProp->Set(percentOutput_);
			os << percentOutput_;
			OnPropertyChanged("Power output (%)", os.str().c_str());
		}
		return ret;
	}
	return DEVICE_OK;
}

int FianiumSC::OnToggleOnOff(MM::PropertyBase* pProp, MM::ActionType eAct)
{

	if (eAct == MM::BeforeGet)
	{
		if (toggleOn_)
			pProp->Set("On");
		else
			pProp->Set("Off");
	}
	else if (eAct == MM::AfterSet)
	{
		std::string state;
		int ret = DEVICE_INVALID_INPUT_PARAM;
		pProp->Get(state);
		OnPropertyChanged("LaserOn?",state.c_str());	// can we access the name of the property programmatically such that this line can be copied without changing names/variables?
		if (state == "On")
		{
			toggleOn_ = true;
			NumericSet(DACstr_, ((long) (((float) percentOutput_/100)*maxDAC_)));
		}
		else if (state == "Off")
		{
			toggleOn_ = false;
			NumericSet(DACstr_, 0);
		}

		else
			return DEVICE_INVALID_INPUT_PARAM;
	}


	return DEVICE_OK;	
}

int  FianiumSC::OnOperatingTime(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	long val = 0;
	if (eAct == MM::BeforeGet)
	{
		val = operatingTime_;
		pProp->Set(val);
	}
	return DEVICE_OK;
}

int  FianiumSC::OnSerialNumber(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	long val = 0;
	if (eAct == MM::BeforeGet)
	{
		val = serial_;
		pProp->Set(val);
	}
	return DEVICE_OK;
}

int  FianiumSC::OnRepRate(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	long val = 0;
	if (eAct == MM::BeforeGet)
	{
		val = reprate_;
		pProp->Set(val);
	}
	return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// FianiumSC interface functions
///////////////////////////////////////////////////////////////////////////////
//These are necessary because of non-uniform way in which Fianium returns values. 

int FianiumSC::GetRunTimeMins(long &mins)
{
	std::string cmd = optimestr_;

	int ret = PurgeComPort(port_.c_str());
	if (ret != DEVICE_OK)
		return ret;

	// send command
	ret = SendSerialCommand(port_.c_str(), (cmd + getcmdstr_).c_str(), termstr_.c_str());
	if (ret != DEVICE_OK)
		return ret;

	// block/wait for acknowledge, or until we time out;
	std::string answer;
	ret = GetSerialAnswer(port_.c_str(), termstr_.c_str(), answer);
	if (ret != DEVICE_OK)
		return ret;
	answer = trim(answer, " \t\n");

	if ( answer.substr(0,cmd.length()) != cmd )
		return DEVICE_SERIAL_INVALID_RESPONSE;

	// TODO: parse answer approptiately
	std::string val_str = answer.substr((cmd.length()), std::string::npos);
	char * pch;
	char* temp;
	long val;
	temp = const_cast<char*>(val_str.c_str());
	pch = strtok(temp, " ,");
	int i = 0;
	long time[2];
	while (pch != NULL)
	{
		std::istringstream ( pch ) >> val;
		time[i] = val;
		pch = strtok(NULL, " ,.-");
		++i;
	}
	mins = time[1] + 60 * time[0];
	return DEVICE_OK;
}


///////////////////////////////////////////////////////////////////////////////
// FianiumSC utility functions
///////////////////////////////////////////////////////////////////////////////

int FianiumSC::NumericSet(std::string cmd, long val)
{
	std::string command = cmd + setcmdstr_ + boost::lexical_cast<std::string>(val);

	int ret = SendSerialCommand((port_).c_str(), command.c_str(), (termstr_).c_str());
	if (ret != DEVICE_OK)
		return ret;

	std::string answer;
	ret = GetSerialAnswer(port_.c_str(), termstr_.c_str(), answer);
	if (ret != DEVICE_OK)
		return ret;

	return DEVICE_OK;  
}

int FianiumSC::NumericGet(std::string cmd, long &val)
{
	//k.SetCallback(&core);

	int ret = PurgeComPort(port_.c_str());
	if (ret != DEVICE_OK)
		return ret;

	// send command
	ret = SendSerialCommand(port_.c_str(), (cmd + getcmdstr_).c_str(), termstr_.c_str());
	if (ret != DEVICE_OK)
		return ret;

	// block/wait for acknowledge, or until we time out;
	std::string answer;
	ret = GetSerialAnswer(port_.c_str(), termstr_.c_str(), answer);
	if (ret != DEVICE_OK)
		return ret;
	answer = trim(answer, " \t\n");

	if ( answer.substr(0,cmd.length()) != cmd )
		return DEVICE_SERIAL_INVALID_RESPONSE;;

	std::string val_str = answer.substr((cmd.length()), std::string::npos);
	std::istringstream ( val_str ) >> val;

	//ret = GetSerialAnswer(port_.c_str(), termstr_.c_str(), answer);
	//if (ret != DEVICE_OK)
	//	return ret;

	return DEVICE_OK;  

}

std::string FianiumSC::trim(const std::string& str, const std::string& whitespace)
{
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos)
        return ""; // no content

    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}

