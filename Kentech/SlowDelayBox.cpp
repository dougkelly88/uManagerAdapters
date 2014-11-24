///////////////////////////////////////////////////////////////////////////////
// FILE:          SlowDelayBox.cpp
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

#include "SlowDelayBox.h"

///////////////////////////////////////////////////////////////////////////////
// KSDB implementation
///////////////////////////////////////////////////////////////////////////////

KSDB::KSDB() :
CGenericBase<KSDB> (), 
	initialized_(false), 
	port_("Undefined"),
	maxdelay_(20000),
	answerTimeoutMs_(1000)
{
	InitializeDefaultErrorMessages();
		
	CPropertyAction * pAct = new CPropertyAction(this, &KSDB::OnPort);
	CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);

	// Command set vars
	// -----------------
	delstr_ = "PS";
	getcmdstr_ = "?";
	setcmdstr_ = " ";
	termstr_ = "\r";
	onoffstr_ = "LOCAL";

	
}

KSDB::~KSDB()
{

}

void KSDB::GetName(char* name) const
{
	// Return the name used to refer to this device adapter
	CDeviceUtils::CopyLimitedString(name, g_PPDGDeviceName);
}

int KSDB::Initialize()
{
	if (initialized_)
		return DEVICE_OK;

	int nRet = CreateStringProperty(MM::g_Keyword_Name, g_PPDGDeviceName, true);
	if (DEVICE_OK != nRet)
		return nRet;

	// Delay
	CPropertyAction *pAct = new CPropertyAction (this, &KSDB::OnDelay);
	nRet = CreateIntegerProperty("Delay (ps)", 0, false, pAct, false);
	if (DEVICE_OK != nRet)
		return nRet;
	delay_ = 0;
	nRet = SetPropertyLimits("Delay (ps)", 0, double(maxdelay_));
	if (nRet != DEVICE_OK)
		return nRet;

	//Calibration path
	pAct = new CPropertyAction (this, &KSDB::OnCalibrationPath);
	nRet = CreateProperty("CalibrationPath", default_calib_path, MM::String, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;
	calibPath_ = default_calib_path ;

	//Calibrated
	pAct = new CPropertyAction (this, &KSDB::OnCalibrate);
	nRet = CreateProperty("Calibrated", "No", MM::String, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;
	calibrated_ = false;
	AddAllowedValue("Calibrated","Yes");
	AddAllowedValue("Calibrated","No");

	k_ = KUtils(port_, getcmdstr_, setcmdstr_, termstr_);
	k_.SetCallback(GetCoreCallback());

	initialized_= true;

	return DEVICE_OK;
}

int KSDB::Shutdown()
{
	// note that sending "LOCAL" command returns control straight 
	// away, i.e. no response is delivered, so standard ToggleSet
	// cannot be used...
	int ret = SendSerialCommand(port_.c_str(), "LOCAL", "\r");
	if (ret != DEVICE_OK)
		return ret;
	initialized_ = false;
	return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// KSDB action handlers
///////////////////////////////////////////////////////////////////////////////

int KSDB::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
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
			return ERR_PORT_CHANGE_FORBIDDEN; 
		}

		pProp->Get(port_);
	}

	return DEVICE_OK;
}

int KSDB::OnCalibrationPath(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		std::string calibPath;
		calibPath = calibPath_;
		pProp->Set( (char *) calibPath_.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		std::string calibPath;
		pProp->Get(calibPath);
		calibPath_ = calibPath;
		//int ret = SetDelay(delay);
		pProp->Set( (char *) calibPath_.c_str());

		return DEVICE_OK;
	}

	return DEVICE_OK;
}

int KSDB::OnCalibrate(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		if (calibrated_)
			pProp->Set("Yes");
		else
			pProp->Set("No");
	}
	else if (eAct == MM::AfterSet)
	{
		std::string state;
		int ret = ERR_CALIBRATION_FAILED;
		pProp->Get(state);
		if (state == "Yes")
			ret = PopulateCalibrationVectors(calibPath_);

		if ((state == "Yes") && (ret == DEVICE_OK))
		{
			int max = *std::max_element(real_delays.begin(), real_delays.end());
			ret = SetPropertyLimits("Delay (ps)", 0, max);
			if (ret != DEVICE_OK)
				return ret;

			calibrated_ = true;

		}

		else
		{
			SetPropertyLimits("Delay", 0, maxdelay_);
			calibrated_ = false;
		}

		std::string valstr = boost::lexical_cast<std::string>(delay_);
		ret = SetProperty("Delay (ps)", valstr.c_str());
		if (ret != DEVICE_OK)
			return ret;
	}

	return DEVICE_OK;	
}

int KSDB::OnDelay(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	long val = 0;
	if (eAct == MM::BeforeGet)
	{
		val = delay_;
		pProp->Set(val);
	}
	else if (eAct == MM::AfterSet)
	{
		long delay;
		long delay_setting;
		pProp->Get(delay);

		delay_setting = k_.doCalibration(calibrated_, delay, real_delays, delay_settings);
		int ret = SDBNumericSet(delstr_, delay_setting);
		if (ret == DEVICE_OK)
		{
			delay_ = delay;
			pProp->Set(delay_);
		}


		return ret;
	}
	return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// KSDB utility functions
///////////////////////////////////////////////////////////////////////////////

// Note that non-standard return following set/get commands mean we need to 
// reimplement these here...
int KSDB::SDBNumericSet(std::string cmd, long val)
{

	std::string command = boost::lexical_cast<std::string>(val) + setcmdstr_ + cmd;

	int ret = SendSerialCommand(port_.c_str(), command.c_str(), termstr_.c_str());
	if (ret != DEVICE_OK)
		return ret;

	std::string answer;
	//for (int i = 0; i < 2; i++)
	//{
		ret = GetSerialAnswer(port_.c_str(), termstr_.c_str(), answer);
		if (ret != DEVICE_OK)
			return ret;
	//}

	return DEVICE_OK;  
}

int KSDB::SDBNumericGet(std::string cmd, long &val)
{
	int ret = PurgeComPort(port_.c_str());
	if (ret != DEVICE_OK)
		return ret;

	// send command
	ret = SendSerialCommand(port_.c_str(), (getcmdstr_ + cmd).c_str(), termstr_.c_str());
	if (ret != DEVICE_OK)
		return ret;

	// block/wait for acknowledge, or until we time out;
	std::string answer;
	ret = GetSerialAnswer(port_.c_str(), termstr_.c_str(), answer);
	if (ret != DEVICE_OK)
		return ret;
	
	ret = GetSerialAnswer(port_.c_str(), termstr_.c_str(), answer);
	if (ret != DEVICE_OK)
		return ret;
	answer = trim(answer, " \t\n");

	std::string template1 = "Delay setting = ";
	std::string template2 = " psecs";

	if (answer.substr(0, template1.length()) != template1)
		return DEVICE_SERIAL_INVALID_RESPONSE;

	std::string val_str = answer.substr(template1.length(), answer.length() - template2.length());
	std::istringstream ( val_str ) >> val;

	ret = GetSerialAnswer(port_.c_str(), termstr_.c_str(), answer);
	if (ret != DEVICE_OK)
		return ret;

	if (answer.substr(answer.size() - 3) != " ok")
		return DEVICE_SERIAL_INVALID_RESPONSE;;

	return DEVICE_OK;  

}

std::string KSDB::trim(const std::string& str, const std::string& whitespace)
{
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos)
        return ""; // no content

    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}

int KSDB::PopulateCalibrationVectors(std::string path)
{
	std::vector<std::string> temp;
	std::string line = "";

	std::ifstream file(path.c_str());
	int fpos = 0;
	if (!file.is_open()) 
		return ERR_OPENFILE_FAILED;
	while (getline(file,line)){
		boost::split(temp, line, boost::is_any_of(","));
		if (!strcmp(temp[0].c_str(), "Delay (ps)"))
		{
			fpos = KUtils::fill_vectors(delay_settings, real_delays, file);
			file.seekg(fpos);
		}

	}

	return DEVICE_OK;

}