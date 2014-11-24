///////////////////////////////////////////////////////////////////////////////
// FILE:          HDG800.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Implementation of Kentech HDG800 delay generator device. 
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

#include "HDG800.h"

///////////////////////////////////////////////////////////////////////////////
// KHDG800 implementation
///////////////////////////////////////////////////////////////////////////////

KHDG800::KHDG800() :
CGenericBase<KHDG800> (), 
	initialized_(false), 
	port_("Undefined"),
	polarityPositive_(true),
	monostable_(false),
	maxdelay_(20000),
	answerTimeoutMs_(1000)
{
	InitializeDefaultErrorMessages();

	CPropertyAction *pAct = new CPropertyAction (this, &KHDG800::OnMonostable);
	int nRet = CreateProperty("Monostable", "False", MM::String, false, pAct, true);
	nRet = AddAllowedValue("Monostable", "False");
	nRet = AddAllowedValue("Monostable", "True");

	pAct = new CPropertyAction (this, &KHDG800::OnPolarity);
	nRet = CreateProperty("Polarity", "Positive", MM::String, false, pAct, true);
	nRet = AddAllowedValue("Polarity", "Positive");
	nRet = AddAllowedValue("Polarity", "Negative");

	pAct = new CPropertyAction(this, &KHDG800::OnPort);
	CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);

	// Command set vars
	// -----------------
	delstr_ = "ps";
	getcmdstr_ = ".";
	setcmdstr_ = " !";
	termstr_ = "\r";
	polstr_ = "pol";
	monostr_ = "usemono";
	threshstr_ = "thr";
	trigopstr_ = "oplevel";
	ScanCommands sc = ScanCommands();
}

KHDG800::~KHDG800()
{

}

void KHDG800::GetName(char* name) const
{
	// Return the name used to refer to this device adapter
	CDeviceUtils::CopyLimitedString(name, g_HDG800DeviceName);
}

int KHDG800::Initialize()
{
	if (initialized_)
		return DEVICE_OK;

	//box_ = KentechFactory::MakeDelayBox("HDG800");

	int nRet = CreateStringProperty(MM::g_Keyword_Name, g_HDG800DeviceName, true);
	if (DEVICE_OK != nRet)
		return nRet;

	// Description
	nRet = CreateStringProperty(MM::g_Keyword_Description, boxType_.c_str(), true);
	if (DEVICE_OK != nRet)
		return nRet;

	// Delay
	CPropertyAction *pAct = new CPropertyAction (this, &KHDG800::OnDelay);
	nRet = CreateIntegerProperty("Delay (ps)", 0, false, pAct, false);
	if (DEVICE_OK != nRet)
		return nRet;
	delay_ = 0;
	nRet = SetPropertyLimits("Delay (ps)", 0, double(maxdelay_));
	if (nRet != DEVICE_OK)
		return nRet;

	//Calibration path
	CPropertyAction* pActCalibrationPath = new CPropertyAction (this, &KHDG800::OnCalibrationPath);
	nRet = CreateProperty("CalibrationPath", default_calib_path, MM::String, false, pActCalibrationPath);
	if (DEVICE_OK != nRet)
		return nRet;
	calibPath_ = default_calib_path ;

	//Calibrated
	CPropertyAction* pActCalibrate = new CPropertyAction (this, &KHDG800::OnCalibrate);
	nRet = CreateProperty("Calibrated", "No", MM::String, false, pActCalibrate);
	if (DEVICE_OK != nRet)
		return nRet;
	calibrated_ = false;
	AddAllowedValue("Calibrated","Yes");
	AddAllowedValue("Calibrated","No");

	// Command set vars
	// -----------------
	//delstr_ = box_->delstr();
	//getcmdstr_ = box_->getcmdstr();
	//setcmdstr_ = box_->setcmdstr();
	//termstr_ = box_->termstr();
	//polstr_ = box_->polstr();
	//monostr_ = box_->monostr();
	//ScanCommands sc = box_->scanCmds();

	// Add scan controls if scan is supported on delay box
	pAct = new CPropertyAction (this, &KHDG800::OnScanMode);
	nRet = CreateProperty("Scan Mode On", "No", MM::String, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;
	AddAllowedValue("Scan Mode On","Yes");
	AddAllowedValue("Scan Mode On","No");

	pAct = new CPropertyAction (this, &KHDG800::OnAddScanPos);
	nRet = CreateProperty(g_addScanPos, "-", MM::String, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;
	AddAllowedValue(g_addScanPos,"-");
	AddAllowedValue(g_addScanPos,"Do it");

	pAct = new CPropertyAction (this, &KHDG800::OnScanPos);
	nRet = CreateIntegerProperty("Scan position", 0, false, pAct, false);
	if (DEVICE_OK != nRet)
		return nRet;
	SetPropertyLimits("Scan position", 0, 255);

	k_ = KUtils(port_, getcmdstr_, setcmdstr_, termstr_);
	k_.SetCallback(GetCoreCallback());

	// Run intitialisation methods
	nRet = SetupHDG800();
	if (nRet != DEVICE_OK)
		return nRet;

	initialized_= true;

	return DEVICE_OK;
}

int KHDG800::Shutdown()
{
	initialized_ = false;
	return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// KHDG800 action handlers
///////////////////////////////////////////////////////////////////////////////

int KHDG800::OnBoxType(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	std::string val = "HDG800";
	if (eAct == MM::BeforeGet)
	{
		val = boxType_;
		pProp->Set(val.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		pProp->Get(val);
		boxType_ = val;
	}

	return DEVICE_OK;
}

int KHDG800::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
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

int KHDG800::OnPolarity(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		if (polarityPositive_)
			pProp->Set("Positive");
		else
			pProp->Set("Negative");
	}
	else if (eAct == MM::AfterSet)
	{
		std::string state;
		int ret = DEVICE_INVALID_PROPERTY_VALUE;
		pProp->Get(state);
		if (state == "Positive")
			polarityPositive_ = true;
		else if (state == "Negative")
			polarityPositive_ = false;
		else
			return DEVICE_INVALID_PROPERTY_VALUE;
	}

	return DEVICE_OK;	
}

int KHDG800::OnMonostable(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		if (monostable_)
			pProp->Set("True");
		else
			pProp->Set("False");
	}
	else if (eAct == MM::AfterSet)
	{
		std::string state;
		int ret = ERR_UNRECOGNISED_PARAM_VALUE;
		pProp->Get(state);
		if (state == "True")
			monostable_ = true;
		else if (state == "False")
			monostable_ = false;
		else
			return ERR_UNRECOGNISED_PARAM_VALUE;
	}

	return DEVICE_OK;	
}

int KHDG800::OnDelay(MM::PropertyBase* pProp, MM::ActionType eAct)
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
		int ret = k_.NumericSet(k_, delstr_, delay_setting);
		if (ret == DEVICE_OK)
		{
			delay_ = delay;
			pProp->Set(delay_);
		}


		return ret;
	}
	return DEVICE_OK;
}

int KHDG800::OnScanMode(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	scanModeOn_;

	return DEVICE_OK;
}

int KHDG800::OnCalibrationPath(MM::PropertyBase* pProp, MM::ActionType eAct)
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

int KHDG800::OnCalibrate(MM::PropertyBase* pProp, MM::ActionType eAct)
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

int KHDG800::OnAddScanPos(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return DEVICE_OK;
}

int KHDG800::OnScanPos(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return DEVICE_OK;
}


///////////////////////////////////////////////////////////////////////////////
// KHDG800 device interface
///////////////////////////////////////////////////////////////////////////////

int KHDG800::PopulateCalibrationVectors(std::string path)
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

int KHDG800::SetupHDG800()
{
	std::string answer;
	std::vector<int> thrgraph;
	std::vector<int> opgraph;
	std::string val_str;
	std::string cmd;

	int ret = PurgeComPort(port_.c_str());
	if (ret != DEVICE_OK)
		return ret;

	if (monostable_)
		cmd = "+" + monostr_;
	else
		cmd = "-" + monostr_;

	ret = k_.ToggleSet(k_, cmd);
	if (ret != DEVICE_OK)
		return ret;

	if (polarityPositive_)
		cmd = "+" + polstr_;
	else
		cmd = "-" + polstr_;

	ret = k_.ToggleSet(k_, cmd);
	if (ret != DEVICE_OK)
		return ret;

	long minmaxthr[2] = {1500, 3500};	// based crudely on graphthr limits
	long minmaxop[2];
	long val;

	for (int i = 0; i < 2; i++)
	{
		// set threshold
		ret = k_.NumericSet(k_, threshstr_, minmaxthr[i]);
		if (ret != DEVICE_OK)
			return ret;

		ret = PurgeComPort(port_.c_str());
		if (ret != DEVICE_OK)
			return ret;

		// get op
		ret = k_.NumericGet(k_, trigopstr_, val);
		if (ret != DEVICE_OK)
			return ret;
		minmaxop[i] = val;

		ret = PurgeComPort(port_.c_str());
		if (ret != DEVICE_OK)
			return ret;
	}

	float midop = (minmaxop[1] - minmaxop[0])/2 + minmaxop[0];

	int currentthr = minmaxthr[1];
	int currentop = minmaxop[1];
	float diff = currentop - midop;
	int t;

	while (abs(diff) > 5 ) 
	{
		if (abs(diff) > 50)	// tweak value?
			t = 50;		// tweak value?
		else
			t = 1;
		if (diff > 0)
			t = (-1)*t;

		currentthr = currentthr + t;

		// set threshold
		ret =  k_.NumericSet(k_, threshstr_, currentthr);
		if (ret != DEVICE_OK)
			return ret;

		// get op level
		ret = k_.NumericGet(k_, trigopstr_, val);
		if (ret != DEVICE_OK)
			return ret;
		currentop = val;
		diff = currentop - midop;
	}

	ret = PurgeComPort(port_.c_str());
	if (ret != DEVICE_OK)
		return ret;

	return DEVICE_OK;

}
