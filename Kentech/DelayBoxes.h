#pragma once

#include <string>
#include <vector>
#include <iterator>
#include <algorithm>
#include <boost/thread/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <stdlib.h>

#include "../../MMDevice/ModuleInterface.h"
#include "../../MMDevice/DeviceBase.h"
#include "../../MMDevice/ImgBuffer.h"
#include "../../MMDevice/DeviceThreads.h"
#include "../../MMDevice/MMDevice.h"

class SetupParameters {
public:
	bool usemono_;
	bool polarityPositive_;

	SetupParameters(bool usemono = false, bool polarityPositive = true)
	{
		usemono_ = usemono;
		polarityPositive_ = polarityPositive;
	}
	~SetupParameters(void) {};
};

//class ScanCommands {
//public:
//	//protected:
//	bool scanAvailable;
//	std::string scancmd;
//	std::string nextdelcmd;
//	std::string prevdelcmd;
//	std::string savescancmd;
//	std::string loadscancmd;
//	std::string setdelcmd;
//	char escapescan;
//
//	// Default for case without scan mode (i.e. first bool false)
//	// in order that there is no need to overload this function when
//	// scan modes are not available. 
//	// Init other commands for HDG800 so that in that case all 
//	// that needs to be done in derived class is modify bool
//	//ScanCommands( bool avail = false, std::string scan = "scan", 
//	//	std::string next = "+", std::string prev = "-", 
//	//	std::string load = "ee@s", std::string save = "ee!s", 
//	//	std::string del = "de",	char esc = 27) : 
//	//	scanAvailable(avail), 
//	//	scancmd(scan), 
//	//	nextdelcmd(next),
//	//	prevdelcmd(prev),
//	//	savescancmd(save), 
//	//	loadscancmd(load),
//	//	setdelcmd(del),
//	//	escapescan(esc)
//	//	{};
//	ScanCommands(bool avail = false, std::string scan = "scan", 
//		std::string next = "+", std::string prev = "-", 
//		std::string load = "ee@s", std::string save = "ee!s", 
//		std::string del = "de",	char esc = 27)
//	{
//		scanAvailable = avail;
//		scancmd = scan; 
//		nextdelcmd = next;
//		prevdelcmd = prev;
//		savescancmd = save; 
//		loadscancmd = load;
//		setdelcmd = del;
//		escapescan = esc;
//	}
//	~ScanCommands(void) {};
//};

class AbstractDelayBox : public CGenericBase<AbstractDelayBox> //Necessary to inherit CGenericBase to support easy serial comms
{
public:
	AbstractDelayBox(void) {};
	~AbstractDelayBox(void) {};

	virtual int AbstractDelayBox::Initialize() {return DEVICE_OK;}
	virtual int AbstractDelayBox::Shutdown() {return DEVICE_OK;}
	virtual void AbstractDelayBox::GetName(char *) const = 0;
	virtual bool AbstractDelayBox::Busy() {return false;}

	virtual std::string description() = 0;	// Use pure virtual to force definition for all derived classes. 
	virtual std::string getcmdstr() {return ".";};
	virtual std::string setcmdstr() {return " !";};
	virtual std::string delstr() = 0;	//Could use a default value - but so diverse that a pure virtual is ok
	virtual std::string termstr() {return "\r";};
	virtual std::string polstr() {return "";}
	virtual std::string monostr() {return "";}
	//virtual ScanCommands scanCmds() {return ScanCommands::ScanCommands();};
	virtual int Setup(MM::Device& device, MM::Core& core, std::string port, SetupParameters sp) {return DEVICE_OK;};
	virtual double maxmimumDelay() {return 20000;};

};

class SingleEdge : public AbstractDelayBox, ScanCommands
{
public:
	SingleEdge(void) {};
	~SingleEdge(void);
	std::string description() {return "SingleEdge";}
	std::string delstr() {std::string delstr = "delay"; return delstr;}
};

class HDG : public AbstractDelayBox
{
public:
	HDG(void) {};
	~HDG(void) {};
	void HDG::GetName(char * name) const {CDeviceUtils::CopyLimitedString(name, "HDG");};
	std::string description() {return "HDG";}
	std::string delstr() {return "DEL";}
	std::string polstr() {return "TPL";}
	//ScanCommands scanCmds() {ScanCommands sp = ScanCommands::ScanCommands(); sp.scanAvailable = true; return sp;}
};

class HDG800 : public AbstractDelayBox
{
public:
	HDG800(void) {};
	~HDG800(void) {};

	void HDG800::GetName(char * name) const {CDeviceUtils::CopyLimitedString(name, "HDG");};
	std::string description() {return "HDG800";}
	std::string delstr() {return "ps";}
	std::string polstr() {return "pol";}
	std::string monostr() {return "usemono";}
	//ScanCommands scanCmds() {ScanCommands sp = ScanCommands(); sp.scanAvailable = true; return sp;}
	int Setup(MM::Device& device, MM::Core& core, std::string port, SetupParameters sp) 
	{

		//this->SetCallback(&core);
		//std::string answer;
		//std::vector<int> thrgraph;
		//std::vector<int> opgraph;
		//std::string val_str;
		//std::string cmd;

		//int ret = PurgeComPort(port.c_str());
		//if (ret != DEVICE_OK)
		//	return ret;

		//if (sp.usemono_)
		//	cmd = "+usemono";
		//else
		//	cmd = "-usemono";

		//ret = SendSerialCommand(port.c_str(), cmd.c_str(), "\r");
		//if (ret != DEVICE_OK)
		//	return ret;
		//ret = GetSerialAnswer(port.c_str(), "\r", answer);
		//if (ret != DEVICE_OK)
		//	return ret;
		//if (answer.compare(cmd + "  ok") !=0)
		//	return DEVICE_SERIAL_COMMAND_FAILED;
		//ret = PurgeComPort(port.c_str());
		//if (ret != DEVICE_OK)
		//	return ret;

		//if (sp.polarityPositive_)
		//	cmd = "+pol";
		//else
		//	cmd = "-pol";

		//ret = SendSerialCommand(port.c_str(), cmd.c_str(), "\r");
		//if (ret != DEVICE_OK)
		//	return ret;
		//ret = GetSerialAnswer(port.c_str(), "\r", answer);
		//if (ret != DEVICE_OK)
		//	return ret;
		//if (answer.compare(cmd + "  ok") !=0)
		//	return DEVICE_SERIAL_COMMAND_FAILED;
		//ret = PurgeComPort(port.c_str());
		//if (ret != DEVICE_OK)
		//	return ret;

		//int minmaxthr[2] = {1500, 3500};	// based crudely on graphthr limits
		//int minmaxop[2];
		//int val;

		//for (int i = 0; i < 2; i++)
		//{
		//	// set threshold
		//	cmd = (boost::lexical_cast<std::string>(minmaxthr[i]) + " !thr");
		//	ret = SendSerialCommand(port.c_str(), cmd.c_str(), "\r");
		//	if (ret != DEVICE_OK)
		//		return ret;
		//	ret = GetSerialAnswer(port.c_str(), "\r", answer);
		//	if (ret != DEVICE_OK)
		//		return ret;
		//	if (answer.compare(cmd + "  ok") !=0)
		//		return DEVICE_SERIAL_COMMAND_FAILED;
		//	ret = PurgeComPort(port.c_str());
		//	if (ret != DEVICE_OK)
		//		return ret;

		//	// get op
		//	cmd = ".oplevel";
		//	ret = SendSerialCommand(port.c_str(), cmd.c_str(), "\r");
		//	if (ret != DEVICE_OK)
		//		return ret;

		//	// block/wait for acknowledge, or until we time out;
		//	ret = GetSerialAnswer(port.c_str(), "\r", answer);
		//	if (ret != DEVICE_OK)
		//		return ret;

		//	if (answer.substr(0, cmd.length() ) != (cmd))
		//		return DEVICE_SERIAL_INVALID_RESPONSE;

		//	val_str = answer.substr(cmd.length(), std::string::npos);
		//	std::istringstream ( val_str ) >> val;
		//	minmaxop[i] = val;

		//	ret = GetSerialAnswer(port.c_str(), "\r", answer);
		//	if (ret != DEVICE_OK)
		//		return ret;

		//	if (answer.substr(answer.size() - 3) != " ok")
		//		return DEVICE_SERIAL_INVALID_RESPONSE;

		//	ret = PurgeComPort(port.c_str());
		//	if (ret != DEVICE_OK)
		//		return ret;
		//}

		//float midop = (minmaxop[1] - minmaxop[0])/2 + minmaxop[0];

		//int currentthr = minmaxthr[1];
		//int currentop = minmaxop[1];
		//float diff = currentop - midop;
		//int t;

		//while (abs(diff) > 5 ) 
		//{
		//	if (abs(diff) > 50)	// tweak value?
		//		t = 50;		// tweak value?
		//	else
		//		t = 1;

		//	if (diff > 0)
		//		t = (-1)*t;

		//	currentthr = currentthr + t;

		//	// set threshold
		//	cmd = (boost::lexical_cast<std::string>(currentthr) + " !thr");
		//	ret = SendSerialCommand(port.c_str(), cmd.c_str(), "\r");
		//	if (ret != DEVICE_OK)
		//		return ret;
		//	ret = GetSerialAnswer(port.c_str(), "\r", answer);
		//	if (ret != DEVICE_OK)
		//		return ret;
		//	if (answer.compare(cmd + "  ok") !=0)
		//		return DEVICE_SERIAL_COMMAND_FAILED;
		//	ret = PurgeComPort(port.c_str());
		//	if (ret != DEVICE_OK)
		//		return ret;

		//	// get current output
		//	cmd = ".oplevel";
		//	ret = SendSerialCommand(port.c_str(), cmd.c_str(), "\r");
		//	if (ret != DEVICE_OK)
		//		return ret;

		//	// block/wait for acknowledge, or until we time out;
		//	ret = GetSerialAnswer(port.c_str(), "\r", answer);
		//	if (ret != DEVICE_OK)
		//		return ret;

		//	if (answer.substr(0, cmd.length() ) != (cmd))
		//		return DEVICE_SERIAL_INVALID_RESPONSE;

		//	val_str = answer.substr(cmd.length(), std::string::npos);
		//	std::istringstream ( val_str ) >> val;
		//	currentop = val;

		//	ret = GetSerialAnswer(port.c_str(), "\r", answer);
		//	if (ret != DEVICE_OK)
		//		return ret;

		//	if (answer.substr(answer.size() - 3) != " ok")
		//		return DEVICE_SERIAL_INVALID_RESPONSE;

		//	ret = PurgeComPort(port.c_str());
		//	if (ret != DEVICE_OK)
		//		return ret;

		//	diff = currentop - midop;
		//}

		//ret = PurgeComPort(port.c_str());
		//if (ret != DEVICE_OK)
		//	return ret;
		
		return DEVICE_OK;

	}

};

class SlowDelayBox : public AbstractDelayBox
{
public:
	SlowDelayBox(void) {};
	~SlowDelayBox(void) {};
	void SlowDelayBox::GetName(char * name) const {CDeviceUtils::CopyLimitedString(name, "HDG");};
	std::string description() {return "SlowDelayBox";}
	std::string getcmdstr() {return "?";}
	std::string setcmdstr() {return " ";}
	std::string delstr() {return "PS";}
};

