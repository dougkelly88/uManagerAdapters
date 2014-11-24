///////////////////////////////////////////////////////////////////////////////
// FILE:          HDG.cpp
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

#include "HDG.h"

///////////////////////////////////////////////////////////////////////////////
// KHDG implementation
///////////////////////////////////////////////////////////////////////////////

KHDG::KHDG() :
CGenericBase<KHDG> (), 
	initialized_(false), 
	port_("Undefined"),
	polarityPositive_(false),
	maxdelay_(20000),
	fiftyOhmInput_(true),
	triggerAttenuated_(false),
	triggerDC_(true),
	eightyMhz_(true),
	answerTimeoutMs_(1000)
{
	InitializeDefaultErrorMessages();

	CPropertyAction * pAct = new CPropertyAction (this, &KHDG::OnFrequency);
	int nRet = CreateProperty("CalibrationTableFrequency", "40 MHz", MM::String, false, pAct, true);
	nRet = AddAllowedValue("CalibrationTableFrequency", "80 MHz");
	nRet = AddAllowedValue("CalibrationTableFrequency", "40 MHz");

	pAct = new CPropertyAction (this, &KHDG::OnTrigCoupling);
	nRet = CreateProperty("TriggerCoupling", "AC", MM::String, false, pAct, true);
	nRet = AddAllowedValue("TriggerCoupling", "AC");
	nRet = AddAllowedValue("TriggerCoupling", "DC");

	pAct = new CPropertyAction (this, &KHDG::OnTrigImpedance);
	nRet = CreateProperty("TriggerImpedance", "50 Ohm", MM::String, false, pAct, true);
	nRet = AddAllowedValue("TriggerImpedance", "50 Ohm");
	nRet = AddAllowedValue("TriggerImpedance", "High");

	pAct = new CPropertyAction (this, &KHDG::OnPolarity);
	nRet = CreateProperty("Polarity", "Positive", MM::String, false, pAct, true);
	nRet = AddAllowedValue("Polarity", "Positive");
	nRet = AddAllowedValue("Polarity", "Negative");

	pAct = new CPropertyAction (this, &KHDG::OnTrigAttenuation);
	nRet = CreateProperty("TriggerAttenuation", "Attenuated", MM::String, false, pAct, true);
	nRet = AddAllowedValue("TriggerImpedance", "Attenuated");
	nRet = AddAllowedValue("TriggerImpedance", "Direct");

	pAct = new CPropertyAction(this, &KHDG::OnPort);
	CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);

	// Command set vars
	// -----------------
	delstr_ = "DEL";
	getcmdstr_ = ".";
	setcmdstr_ = " !";
	termstr_ = "\r";
	polstr_ = "TPL";
	couplingstr_ = "TDC";
	impedancestr_ = "T50";
	attenuationstr_ = "TAT";
	threshstr_ = "TTH";
	trigopstr_ = "TFB";
	onoffstr_ = "OUT";

	ScanCommands sc = ScanCommands();
}

KHDG::~KHDG()
{

}

void KHDG::GetName(char* name) const
{
	// Return the name used to refer to this device adapter
	CDeviceUtils::CopyLimitedString(name, g_HDGDeviceName);
}

int KHDG::Initialize()
{
	if (initialized_)
		return DEVICE_OK;
	
	int nRet = CreateStringProperty(MM::g_Keyword_Name, g_HDGDeviceName, true);
	if (DEVICE_OK != nRet)
		return nRet;

	// Delay
	CPropertyAction *pAct = new CPropertyAction (this, &KHDG::OnDelay);
	nRet = CreateIntegerProperty("Delay (ps)", 0, false, pAct, false);
	if (DEVICE_OK != nRet)
		return nRet;
	delay_ = 0;
	nRet = SetPropertyLimits("Delay (ps)", 0, double(maxdelay_));
	if (nRet != DEVICE_OK)
		return nRet;

	//Calibration path
	pAct = new CPropertyAction (this, &KHDG::OnCalibrationPath);
	nRet = CreateProperty("CalibrationPath", default_calib_path, MM::String, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;
	calibPath_ = default_calib_path ;

	//Calibrated
	pAct = new CPropertyAction (this, &KHDG::OnCalibrate);
	nRet = CreateProperty("Calibrated", "No", MM::String, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;
	calibrated_ = false;
	AddAllowedValue("Calibrated","Yes");
	AddAllowedValue("Calibrated","No");

	// Add scan controls if scan is supported on delay box
	pAct = new CPropertyAction (this, &KHDG::OnScanMode);
	nRet = CreateProperty("Scan Mode On", "No", MM::String, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;
	AddAllowedValue("Scan Mode On","Yes");
	AddAllowedValue("Scan Mode On","No");

	pAct = new CPropertyAction (this, &KHDG::OnAddScanPos);
	nRet = CreateProperty(g_addScanPos, "-", MM::String, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;
	AddAllowedValue(g_addScanPos,"-");
	AddAllowedValue(g_addScanPos,"Do it");

	pAct = new CPropertyAction (this, &KHDG::OnScanPos);
	nRet = CreateIntegerProperty("Scan position", 0, false, pAct, false);
	if (DEVICE_OK != nRet)
		return nRet;
	SetPropertyLimits("Scan position", 0, 255);

	k_ = KUtils(port_, getcmdstr_, setcmdstr_, termstr_);
	k_.SetCallback(GetCoreCallback());

	// Run intitialisation methods
	nRet = SetupHDG();
	if (nRet != DEVICE_OK)
		return nRet;

	initialized_= true;

	return DEVICE_OK;
}

int KHDG::Shutdown()
{
	int ret = k_.ToggleSet(k_, ("-" + onoffstr_));
	if (ret != DEVICE_OK)
		return ret;
	initialized_ = false;
	return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// KHDG action handlers
///////////////////////////////////////////////////////////////////////////////

int KHDG::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
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

int KHDG::OnTrigAttenuation(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		if (triggerAttenuated_)
			pProp->Set("Attenuated");
		else
			pProp->Set("Direct");
	}
	else if (eAct == MM::AfterSet)
	{
		std::string state;
		int ret = DEVICE_INVALID_PROPERTY_VALUE;
		pProp->Get(state);
		if (state == "Attenuated")
			triggerAttenuated_ = true;
		else if (state == "Direct")
			triggerAttenuated_ = false;
		else
			return DEVICE_INVALID_PROPERTY_VALUE;
	}

	return DEVICE_OK;	
}

int KHDG::OnTrigImpedance(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		if (fiftyOhmInput_)
			pProp->Set("50 Ohm");
		else
			pProp->Set("High");
	}
	else if (eAct == MM::AfterSet)
	{
		std::string state;
		int ret = DEVICE_INVALID_PROPERTY_VALUE;
		pProp->Get(state);
		if (state == "50 Ohm")
			fiftyOhmInput_ = true;
		else if (state == "High")
			fiftyOhmInput_ = false;
		else
			return DEVICE_INVALID_PROPERTY_VALUE;
	}

	return DEVICE_OK;	
}

int KHDG::OnTrigCoupling(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		if (triggerDC_)
			pProp->Set("DC");
		else
			pProp->Set("AC");
	}
	else if (eAct == MM::AfterSet)
	{
		std::string state;
		int ret = DEVICE_INVALID_PROPERTY_VALUE;
		pProp->Get(state);
		if (state == "DC")
			triggerDC_ = true;
		else if (state == "AC")
			triggerDC_ = false;
		else
			return DEVICE_INVALID_PROPERTY_VALUE;
	}

	return DEVICE_OK;	
}

int KHDG::OnPolarity(MM::PropertyBase* pProp, MM::ActionType eAct)
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

int KHDG::OnFrequency(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		if (eightyMhz_)
			pProp->Set("80 MHz");
		else
			pProp->Set("40 MHz");
	}
	else if (eAct == MM::AfterSet)
	{
		std::string state;
		int ret = DEVICE_INVALID_PROPERTY_VALUE;
		pProp->Get(state);
		if (state == "80 MHz")
			eightyMhz_ = true;
		else if (state == "40 MHz")
			eightyMhz_ = false;
		else
			return DEVICE_INVALID_PROPERTY_VALUE;
	}

	return DEVICE_OK;	
}

int KHDG::OnDelay(MM::PropertyBase* pProp, MM::ActionType eAct)
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

int KHDG::OnScanMode(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	scanModeOn_;

	return DEVICE_OK;
}

int KHDG::OnCalibrationPath(MM::PropertyBase* pProp, MM::ActionType eAct)
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

int KHDG::OnCalibrate(MM::PropertyBase* pProp, MM::ActionType eAct)
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

int KHDG::OnAddScanPos(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return DEVICE_OK;
}

int KHDG::OnScanPos(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return DEVICE_OK;
}


///////////////////////////////////////////////////////////////////////////////
// KHDG device interface
///////////////////////////////////////////////////////////////////////////////

int KHDG::PopulateCalibrationVectors(std::string path)
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

int KHDG::SetupHDG()
{
	std::string answer;
	std::vector<int> thrgraph;
	std::vector<int> opgraph;
	std::string val_str;
	std::string cmd;

	int ret = PurgeComPort(port_.c_str());
	if (ret != DEVICE_OK)
		return ret;

	if (eightyMhz_)
		cmd = "80MHZ";
	else
		cmd = "40MHZ";

	ret = k_.ToggleSet(k_, cmd);
	if (ret != DEVICE_OK)
		return ret;

	if (fiftyOhmInput_)
		cmd = "+" + impedancestr_;
	else
		cmd = "-" + impedancestr_;

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

	if (triggerAttenuated_)
		cmd = "+" + attenuationstr_;
	else
		cmd = "-" + attenuationstr_;

	ret = k_.ToggleSet(k_, cmd);
	if (ret != DEVICE_OK)
		return ret;
	
	if (triggerDC_)
		cmd = "+" + triggerDC_;
	else
		cmd = "-" + triggerDC_;

	ret = k_.ToggleSet(k_, cmd);
	if (ret != DEVICE_OK)
		return ret;

	long minmaxthr[2] = {0, 250};	// based crudely on graphthr limits
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

	ret = k_.ToggleSet(k_, ("+" + onoffstr_));
	if (ret != DEVICE_OK)
		return ret;

	ret = PurgeComPort(port_.c_str());
	if (ret != DEVICE_OK)
		return ret;

	return DEVICE_OK;

}
