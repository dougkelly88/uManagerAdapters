///////////////////////////////////////////////////////////////////////////////
// FILE:          SingleEdge.cpp
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

#include "SingleEdge.h"


///////////////////////////////////////////////////////////////////////////////
// KSE implementation
///////////////////////////////////////////////////////////////////////////////

KSE::KSE() :
CGenericBase<KSE> (), 
	initialized_(false), 
	port_("Undefined"),
	polarityPositive_(true),
	monostable_(false),
	maxdelay_(20000),
	answerTimeoutMs_(1000)
{
	InitializeDefaultErrorMessages();

	CPropertyAction *pAct = new CPropertyAction(this, &KSE::OnPort);
	CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);

	// Command set vars
	// -----------------
	delstr_ = "delay";
	getcmdstr_ = ".";
	setcmdstr_ = " !";
	termstr_ = "\r";
	gainstr_ = "mcp";
	widthstr_ = "width";
	ScanCommands sc = ScanCommands();
}

KSE::~KSE()
{

}

void KSE::GetName(char* name) const
{
	// Return the name used to refer to this device adapter
	CDeviceUtils::CopyLimitedString(name, g_SEDeviceName);
}

int KSE::Initialize()
{
	if (initialized_)
		return DEVICE_OK;

	//box_ = KentechFactory::MakeDelayBox("SE");

	int nRet = CreateStringProperty(MM::g_Keyword_Name, g_SEDeviceName, true);
	if (DEVICE_OK != nRet)
		return nRet;

	// Description
	nRet = CreateStringProperty(MM::g_Keyword_Description, boxType_.c_str(), true);
	if (DEVICE_OK != nRet)
		return nRet;

	// Delay
	CPropertyAction *pAct = new CPropertyAction (this, &KSE::OnDelay);
	nRet = CreateIntegerProperty("Delay (ps)", 0, false, pAct, false);
	if (DEVICE_OK != nRet)
		return nRet;
	delay_ = 0;
	nRet = SetPropertyLimits("Delay (ps)", 0, double(maxdelay_));
	if (nRet != DEVICE_OK)
		return nRet;

	// Calibration path
	pAct = new CPropertyAction (this, &KSE::OnCalibrationPath);
	nRet = CreateProperty("CalibrationPath", default_calib_path, MM::String, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;
	calibPath_ = default_calib_path ;

	// Calibrated
	pAct = new CPropertyAction (this, &KSE::OnCalibrate);
	nRet = CreateProperty("Calibrated", "No", MM::String, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;
	calibrated_ = false;
	AddAllowedValue("Calibrated","Yes");
	AddAllowedValue("Calibrated","No");

	// Inhibit
	pAct = new CPropertyAction (this, &KSE::OnInhibit);
	nRet = CreateProperty("Inhibit", "Inhibited", MM::String, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;
	AddAllowedValue("Inhibit","Inhibited");
	AddAllowedValue("Inhibit","Running");
	inhibited_ = true;

	// Width
	pAct = new CPropertyAction (this, &KSE::OnWidth);
	nRet = CreateProperty("Width", "511", MM::Float, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;
	SetPropertyLimits("Width", 135, 722);		//True for 80MHz rep rate, can go up to ~10.5 ns for 40MHz.
	//TODO deal with varying maximum width depending on laser rep rate - current implementation is conservative. 
	width_ = 511;

	// Gain
	pAct = new CPropertyAction (this, &KSE::OnGain);
	nRet = CreateProperty("Gain", "50", MM::Float, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;
	SetPropertyLimits("Gain", 50, 864);
	gain_ = 50;

	// TODO: bias

	k_ = KUtils(port_, getcmdstr_, setcmdstr_, termstr_);
	k_.SetCallback(GetCoreCallback());

	if (nRet != DEVICE_OK)
		return nRet;

	initialized_= true;

	return DEVICE_OK;
}

int KSE::Shutdown()
{
	initialized_ = false;
	return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// KSE action handlers
///////////////////////////////////////////////////////////////////////////////

int KSE::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
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

int KSE::OnGain(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	long val = 0;
	if (eAct == MM::BeforeGet)
	{
		val = gain_;
		pProp->Set(val);
	}
   else if (eAct == MM::AfterSet)
   {
      long gain;
	  long gain_setting;
      pProp->Get(gain);

	  gain_setting = k_.doCalibration(calibrated_, gain, real_mcps, mcp_settings);
	  int ret = k_.NumericSet(k_, gainstr_, gain_setting);
	  if (ret == DEVICE_OK)
	  {
		  gain_ = gain;
		  pProp->Set(gain_);
	  }
      

      return ret;
   }

   return DEVICE_OK;
}

int KSE::OnDelay(MM::PropertyBase* pProp, MM::ActionType eAct)
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

int KSE::OnWidth(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	//TODO add capability to access gates wide < 1300 ps using fixed width setting, varing bias. 
	//TODO add capability to access gates >
    int ret = DEVICE_OK;
	if (eAct == MM::BeforeGet)
   {
      long width;
	  width = width_;
      ret = k_.NumericGet(k_, widthstr_, width);
	  width_ = k_.doCalibration(calibrated_, width, width_settings, real_widths);
	  pProp->Set(width_);
   }
   else if (eAct == MM::AfterSet)
   {
      long width;
	  long width_setting;
      pProp->Get(width);

	  width_setting = k_.doCalibration(calibrated_, width, real_widths, width_settings);
      ret = k_.NumericGet(k_, widthstr_, width);
	  if (ret == DEVICE_OK)
	  {
		  width_ = width;
		  pProp->Set(width_);
	  }
      

      return ret;
   }

   return DEVICE_OK;
}

int KSE::OnBias(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	//TODO bias
	int ret = DEVICE_OK;
	return ret;
}

int KSE::OnInhibit(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    int ret = DEVICE_OK;
	if (eAct == MM::BeforeGet)
   {
      long gain;
	  std::string inhibited_str = "Inhibited";
      ret = k_.NumericGet(k_, gainstr_, gain);
	  if (gain == 0)
		  inhibited_ = true;
	  else
	  {
		  inhibited_ = false;
		  dummy_gain_ = gain;
		  inhibited_str = "Running";
	  }
	  //if (inhibited_)
		 // inhibited_str = "Inhibited";
	  //else
		 // inhibited_str = "Running";
	  pProp->Set((char *) inhibited_str.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
	  // If inhibit, set gain setting to zero. 
	  // If !inhibit, set gain to gain value. 
      std::string inhibited_str;
	  bool inhibit;
	  long gain_setting;
	  pProp->Get(inhibited_str);
	  if (inhibited_str.compare("Inhibited") == 0)
		  inhibit = true;
	  else
		  inhibit = false;
	  if (inhibit)
	  {
		  ret = k_.NumericSet(k_, gainstr_, 0);
		  if (ret == DEVICE_OK)
		  {
			  inhibited_ = inhibit;
			  SetPropertyLimits("Gain", 0, 1);
			  //Possible to set property to read only here?
			  pProp->Set((char *) inhibited_str.c_str());
			  dummy_gain_ = gain_;
		  }
	  }
	  else
	  {
		  gain_setting = k_.doCalibration(calibrated_, dummy_gain_, real_mcps, mcp_settings);
		  int ret = k_.NumericSet(k_, gainstr_, gain_setting);
		  {
			  inhibited_ = inhibit;
			  pProp->Set((char *) inhibited_str.c_str());
			  if (calibrated_)
				  SetPropertyLimits("Gain", 237, 850);
			  else
				  SetPropertyLimits("Gain", 50, 864);

		  }
	  }

      return ret;
   }

   return DEVICE_OK;
}

int KSE::OnCalibrationPath(MM::PropertyBase* pProp, MM::ActionType eAct)
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

int KSE::OnCalibrate(MM::PropertyBase* pProp, MM::ActionType eAct)
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
		  SetPropertyLimits("Gain", 237, 850);
		  SetPropertyLimits("Delay", 0, 20000);
		  SetPropertyLimits("Width", 1300, 8200);
		  calibrated_ = true;
	  }

      else
      {
		  SetPropertyLimits("Gain", 50, 864);
		  SetPropertyLimits("Delay", 0, 2046);
		  SetPropertyLimits("Width", 135, 722);
		  calibrated_ = false;
	  }
   }

   return DEVICE_OK;	
}

///////////////////////////////////////////////////////////////////////////////
// KSE device interface
///////////////////////////////////////////////////////////////////////////////

int KSE::PopulateCalibrationVectors(std::string path)
{
	std::vector<std::string> temp;
	std::string line = "";

	std::ifstream file(path.c_str());
	int fpos = 0;
	if (!file.is_open()) 
		return ERR_OPENFILE_FAILED;
	while (getline(file,line)){
		boost::split(temp, line, boost::is_any_of(","));
		//cout << temp[0].c_str();
		if (!strcmp(temp[0].c_str(), "Delay (ps)"))
		{
			fpos = KUtils::fill_vectors(delay_settings, real_delays, file);
			file.seekg(fpos);
		}
		else if (!strcmp(temp[0].c_str(), "Width (ps)"))
		{
			fpos = KUtils::fill_vectors(width_settings, real_widths, file);
			file.seekg(fpos);
		}
		//else if (strcmp(temp[1].c_str(), "BiasWidth (ps)"))
		//{
		//}
		else if (!strcmp(temp[0].c_str(), "MCP (V)"))
		{
			fpos = KUtils::fill_vectors(mcp_settings, real_mcps, file);
			file.seekg(fpos);
		}
		else
		{
			/*cout << "NOTHING TO DO!!!" << endl;*/
		}
	}

}