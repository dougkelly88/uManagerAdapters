///////////////////////////////////////////////////////////////////////////////
// KUtils utility methods that can be used for all Kentech devices
///////////////////////////////////////////////////////////////////////////////

//#pragma once

#ifndef _UTILS_H_
#define _UTILS_H_



#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <boost/algorithm/string.hpp>
#include <math.h>
#include <boost/lexical_cast.hpp>

#include "../../MMDevice/ModuleInterface.h"
#include "../../MMDevice/DeviceBase.h"
#include "../../MMDevice/ImgBuffer.h"
#include "../../MMDevice/DeviceThreads.h"
#include "../../MMDevice/MMDevice.h"

class ScanCommands {
public:
	//protected:
	bool scanAvailable;
	std::string scancmd;
	std::string nextdelcmd;
	std::string prevdelcmd;
	std::string savescancmd;
	std::string loadscancmd;
	std::string setdelcmd;
	char escapescan;

	ScanCommands(bool avail = false, std::string scan = "scan", 
		std::string next = "+", std::string prev = "-", 
		std::string load = "ee@s", std::string save = "ee!s", 
		std::string del = "de",	char esc = 27)
	{
		scanAvailable = avail;
		scancmd = scan; 
		nextdelcmd = next;
		prevdelcmd = prev;
		savescancmd = save; 
		loadscancmd = load;
		setdelcmd = del;
		escapescan = esc;
	}
	~ScanCommands(void) {};
};

class KUtils : public CGenericBase<KUtils> //Necessary to inherit CGenericBase to support easy serial comms
{
public:
	std::string port_;
	std::string termstr_;
	std::string getcmdstr_;
	std::string setcmdstr_;

	KUtils(std::string port = "COM1", std::string getcmdstr = ".", std::string setcmdstr = " !", std::string termstr = "\r")
	{
		port_ = port;
		termstr_ = termstr;
		getcmdstr_ = getcmdstr;
		setcmdstr_ = setcmdstr;
		
	};
	~KUtils(void) {};

	virtual int KUtils::Initialize() {return DEVICE_OK;}
	virtual int KUtils::Shutdown() {return DEVICE_OK;}
	virtual void KUtils::GetName(char *) const {};
	virtual bool KUtils::Busy() {return false;}

	std::string trim(const std::string& str, const std::string& whitespace = " \t\n");

	static bool is_number(const std::string& s);
	static int fill_vectors(std::vector<int> &setting, std::vector<int> &real_var, std::ifstream &file);
	int doCalibration(bool do_calibration, long &input, std::vector<int> in_type_vector, std::vector<int> out_type_vector);
	int NumericSet(KUtils k, std::string cmd, long val);
	int NumericGet(KUtils k, std::string cmd, long &val);
	int ToggleSet(KUtils k, std::string cmd);

};



#endif _UTILS_H_