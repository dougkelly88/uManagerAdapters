///////////////////////////////////////////////////////////////////////////////
// FILE:          StandardHRI.cpp
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

#include "StandardHRI.h"

///////////////////////////////////////////////////////////////////////////////
// KHRI implementation
///////////////////////////////////////////////////////////////////////////////

KHRI::KHRI() :
CGenericBase<KHRI> (), 
	initialized_(false), 
	port_("Undefined"),
	polarityPositive_(false),
	eclTrigger_(true),
	fiftyOhmInput_(true),
	answerTimeoutMs_(1000)
{
	InitializeDefaultErrorMessages();

	CPropertyAction * pAct = new CPropertyAction (this, &KHRI::OnTrigLogic);
	int nRet = CreateProperty("TriggerLogic", "ECL", MM::String, false, pAct, true);
	nRet = AddAllowedValue("CalibrationTableFrequency", "ECL");
	nRet = AddAllowedValue("CalibrationTableFrequency", "TTL");

	pAct = new CPropertyAction (this, &KHRI::OnTrigImpedance);
	nRet = CreateProperty("TriggerImpedance", "50 Ohm", MM::String, false, pAct, true);
	nRet = AddAllowedValue("TriggerImpedance", "50 Ohm");
	nRet = AddAllowedValue("TriggerImpedance", "High");

	pAct = new CPropertyAction (this, &KHRI::OnPolarity);
	nRet = CreateProperty("Polarity", "Positive", MM::String, false, pAct, true);
	nRet = AddAllowedValue("Polarity", "Positive");
	nRet = AddAllowedValue("Polarity", "Negative");

	pAct = new CPropertyAction (this, &KHRI::OnMode);
	nRet = CreateProperty("Mode", "Comb", MM::String, false, pAct, true);
	nRet = PopulateModeVector(modeDescriptions_, modeNumbers_);
	nRet = SetAllowedValues("Mode", modeDescriptions_);

	pAct = new CPropertyAction(this, &KHRI::OnPort);
	CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);

	// Command set vars
	// -----------------
	getcmdstr_ = ".";
	setcmdstr_ = " !";
	termstr_ = "\r";
	polstr_ = "VETRIG";
	rfstr_ = "RFGAIN";
	mcpstr_ = "MCPVOLTS";
	modestr_ = "MODE";
	trigstr_ = "TRIG";
	onoffstr_ = "LOCAL";

	ScanCommands sc = ScanCommands();
}

KHRI::~KHRI()
{

}

void KHRI::GetName(char* name) const
{
	// Return the name used to refer to this device adapter
	CDeviceUtils::CopyLimitedString(name, g_HRIDeviceName);
}

int KHRI::Initialize()
{
	if (initialized_)
		return DEVICE_OK;

	int nRet = CreateStringProperty(MM::g_Keyword_Name, g_HRIDeviceName, true);
	if (DEVICE_OK != nRet)
		return nRet;

	// Gain
	gain_ = 0;
	CPropertyAction* pAct= new CPropertyAction (this, &KHRI::OnGain);
	nRet = CreateIntegerProperty("Gain", gain_, false, pAct, false);
	if (DEVICE_OK != nRet)
		return nRet;
	
	// Gate width
	if ((modeNumber_ < 11) && (modeNumber_ >1))
	{
		width_ = 200;
		pAct = new CPropertyAction (this, &KHRI::OnWidth);
		nRet = CreateProperty("Gate width (ps)", "200", MM::String, false, pAct);
		if (DEVICE_OK != nRet)
			return nRet;
		std::vector<std::string> widths;
		for (int i = 2; i < 11; i++)
			widths.push_back(boost::lexical_cast<std::string>(100 * i));
		SetAllowedValues("Gate width (ps)", widths);
	}

	// Inhibit
	pAct = new CPropertyAction (this, &KHRI::OnInhibit);
	nRet = CreateProperty("Inhibit", "Inhibited", MM::String, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;
	AddAllowedValue("Inhibit","Inhibited");
	AddAllowedValue("Inhibit","Running");
	inhibited_ = true;

	// DC mode
	pAct = new CPropertyAction (this, &KHRI::OnDC);
	nRet = CreateProperty("DC Mode", "Off", MM::String, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;
	AddAllowedValue("DC Mode","On");
	AddAllowedValue("DC Mode","Off");
	dcMode_ = false;

	k_ = KUtils(port_, getcmdstr_, setcmdstr_, termstr_);
	k_.SetCallback(GetCoreCallback());

	// Run intitialisation methods
	nRet = SetupHRI();
	if (nRet != DEVICE_OK)
		return nRet;

	initialized_= true;

	return DEVICE_OK;
}

int KHRI::Shutdown()
{
	int ret = k_.NumericSet(k_, modestr_, INHIBIT);
	if (ret != DEVICE_OK)
		return ret;

	ret = k_.ToggleSet(k_, onoffstr_);
	if (ret != DEVICE_OK)
		return ret;
	initialized_ = false;
	return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// KHRI action handlers
///////////////////////////////////////////////////////////////////////////////

int KHRI::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
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

int KHRI::OnTrigLogic(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		if (eclTrigger_)
			pProp->Set("ECL");
		else
			pProp->Set("TTL");
	}
	else if (eAct == MM::AfterSet)
	{
		std::string state;
		int ret = DEVICE_INVALID_PROPERTY_VALUE;
		pProp->Get(state);
		if (state == "ECL")
			eclTrigger_ = true;
		else if (state == "TTL")
			eclTrigger_ = false;
		else
			return DEVICE_INVALID_PROPERTY_VALUE;
	}

	return DEVICE_OK;	
}

int KHRI::OnTrigImpedance(MM::PropertyBase* pProp, MM::ActionType eAct)
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

int KHRI::OnPolarity(MM::PropertyBase* pProp, MM::ActionType eAct)
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

int KHRI::OnMode(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	std::vector<int>::iterator it;
	std::vector<std::string>::iterator itstr;
	if (eAct == MM::BeforeGet)
	{
		it = std::find(modeNumbers_.begin(), modeNumbers_.end(), modeNumber_);
		if (it != modeNumbers_.end())
			pProp->Set((modeDescriptions_.at(it - modeNumbers_.begin())).c_str());
		else
			return DEVICE_INVALID_PROPERTY_VALUE;
	}
	else if (eAct == MM::AfterSet)
	{
		std::string state;
		pProp->Get(state);
		itstr = std::find(modeDescriptions_.begin(), modeDescriptions_.end(), state);
		if (itstr != modeDescriptions_.end())
			modeNumber_ = modeNumbers_.at(itstr - modeDescriptions_.begin());
		// do with iterators/search? for futureproofing/tidiness
		//if (state == "Comb")
		//	modeNumber_ = 2;
		//else if (state == "RF")
		//	modeNumber_ = 21;
		//else if (state == "Logic - Low Duty Cycle")
		//	modeNumber_ = 22;
		//else if (state == "Logic - High Duty Cycle")
		//	modeNumber_ = 23;
		else
			return DEVICE_INVALID_PROPERTY_VALUE;
	}

	return DEVICE_OK;	
}

int KHRI::OnDC(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		if (dcMode_)
			pProp->Set("On");
		else
			pProp->Set("Off");
	}
	else if (eAct == MM::AfterSet)
	{
		std::string state;
		long number;
		int ret = DEVICE_INVALID_PROPERTY_VALUE;
		pProp->Get(state);
		if (state == "On")
		{
			dcMode_ = true;
			//cmd = boost::lexical_cast<std::string>(DC) + setcmdstr_ + modestr_;
			number = DC;
		}
		else if (state == "Off")
		{
			dcMode_ = false;
			//cmd = boost::lexical_cast<std::string>(modeNumber_) + setcmdstr_ + modestr_;
			number = modeNumber_;
		}
		else
			return DEVICE_INVALID_PROPERTY_VALUE;

		ret = k_.NumericSet(k_, modestr_, number);
		if (ret != DEVICE_OK)
			return ret;
	}

	return DEVICE_OK;	
}

int KHRI::OnInhibit(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		if (inhibited_)
			pProp->Set("Inhibited");
		else
			pProp->Set("Running");
	}
	else if (eAct == MM::AfterSet)
	{
		std::string state;
		long number;
		int ret = DEVICE_INVALID_PROPERTY_VALUE;
		pProp->Get(state);
		if (state == "Inhibited")
		{
			inhibited_ = true;
			number = INHIBIT;
			//cmd = boost::lexical_cast<std::string>(INHIBIT) + setcmdstr_ + modestr_;
		}
		else if (state == "Running")
		{
			inhibited_ = false;
			number = modeNumber_;
			//cmd = boost::lexical_cast<std::string>(modeNumber_) + setcmdstr_ + modestr_;
		}
		else
			return DEVICE_INVALID_PROPERTY_VALUE;

		ret = k_.NumericSet(k_, modestr_, number);
		if (ret != DEVICE_OK)
			return ret;
	}

	return DEVICE_OK;	
}

int KHRI::OnWidth(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(boost::lexical_cast<std::string>(width_).c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		std::string state;
		std::string cmd;
		int ret = DEVICE_INVALID_PROPERTY_VALUE;
		pProp->Get(state);
		width_ = atoi(state.c_str());
		ret = k_.NumericSet(k_, modestr_, ((long) (width_/10)) );
		if (ret != DEVICE_OK)
			return ret;
	}

	return DEVICE_OK;	
}

int KHRI::OnGain(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	std::string gstr;
	if (modeNumber_ < 11)	// Comb mode
		gstr = mcpstr_;
	else					// RF mode
		gstr = rfstr_;

	if (eAct == MM::BeforeGet)
	{
		pProp->Set(boost::lexical_cast<std::string>(gain_).c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		std::string state;
		std::string cmd;
		int ret = DEVICE_INVALID_PROPERTY_VALUE;
		pProp->Get(state);
		gain_ = atoi(state.c_str());
		ret = k_.NumericSet(k_, gstr, gain_);
		if (ret != DEVICE_OK)
			return ret;
	}

	return DEVICE_OK;	
}


///////////////////////////////////////////////////////////////////////////////
// KHRI device interface
///////////////////////////////////////////////////////////////////////////////

int KHRI::PopulateModeVector(std::vector<std::string> &modeDescriptions, std::vector<int> &modeNumbers)
{
	// hard code this for time being
	long mN[4] = {2, 21, 22, 23};
	for (int i = 0; i  < 4; i++)
	{
		modeNumbers.push_back(mN[i]);
	}
	modeDescriptions.push_back("Comb");
	modeDescriptions.push_back("RF");
	modeDescriptions.push_back("Logic - Low Duty Cycle");
	modeDescriptions.push_back("Logic - High Duty Cycle");

	return DEVICE_OK;
}

int KHRI::SetupHRI()
{
	std::string cmd;

	int ret = PurgeComPort(port_.c_str());
	if (ret != DEVICE_OK)
		return ret;

	// Set mode
	ret = k_.NumericSet(k_, modestr_, modeNumber_);
	if (ret != DEVICE_OK)
		return ret;

	// Set trigger termination
	if (fiftyOhmInput_)
		cmd = "50" +  trigstr_;
	else
		cmd = "HI" + trigstr_;
	
	ret = k_.ToggleSet(k_, cmd);
	if (ret != DEVICE_OK)
		return ret;

	// Set trigger logic type
	if (eclTrigger_)
		cmd = "ECL" + trigstr_;
	else
		cmd = "TTL" + trigstr_ ;

	ret = k_.ToggleSet(k_, cmd);
	if (ret != DEVICE_OK)
		return ret;

	// Set trigger polarity
	if (polarityPositive_)
		cmd = "+" + polstr_;
	else
		cmd = "-" + polstr_;

	ret = k_.ToggleSet(k_, cmd);
	if (ret != DEVICE_OK)
		return ret;

	// WHAT ABOUT VOLTAGE OFFSET!?!?

	ret = PurgeComPort(port_.c_str());
	if (ret != DEVICE_OK)
		return ret;
	
	return DEVICE_OK;
}