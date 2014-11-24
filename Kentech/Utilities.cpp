#include "Utilities.h"

// General utility functions:
bool KUtils::is_number(const std::string& s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

int KUtils::fill_vectors(std::vector<int> &setting, std::vector<int> &real_var, std::ifstream &file)
{
	//std::vector<int> real_var;
	int pos = 0;
	std::string line = "";
	std::vector<std::string> temp;
	std::getline(file, line);
	boost::split(temp, line, boost::is_any_of(","));
	while ((is_number(temp[0].c_str())) && (!file.eof()))
	{
		real_var.push_back(atoi(temp[0].c_str()));
		setting.push_back(atoi(temp[1].c_str()));
		pos = file.tellg();
		getline(file,line);
		boost::split(temp, line, boost::is_any_of(","));
	}
	
	return pos;
}

int KUtils::doCalibration(bool do_calibration, long &input, std::vector<int> in_type_vector, std::vector<int> out_type_vector)
{
	if (do_calibration)
	{
		std::vector<int> lookup_val (in_type_vector.size(), input);
		std::vector<int> answer;
		std::transform(in_type_vector.begin(), in_type_vector.end(), lookup_val.begin(), std::back_inserter(answer), 
			[](int in_type_vector, int lookup_val) { return std::abs(in_type_vector - lookup_val); });
		int index = std::distance(answer.begin(), min_element(answer.begin(), answer.end()));
		int closest_available_input = in_type_vector.at(index);
		int corresponding_output = out_type_vector.at(index);
		input = closest_available_input;
		return corresponding_output;
	}
	else
		input = 25 * floor(((double) input)/25 + 0.5);
		return input;
}

int KUtils::NumericSet(KUtils k, std::string cmd, long val)
{
	//k.SetCallback(&core);

	std::string command = boost::lexical_cast<std::string>(val) + k.setcmdstr_ + cmd;

	int ret = k.SendSerialCommand((k.port_).c_str(), command.c_str(), (k.termstr_).c_str());
	if (ret != DEVICE_OK)
		return ret;

	std::string answer;
	ret = k.GetSerialAnswer(k.port_.c_str(), k.termstr_.c_str(), answer);
	if (ret != DEVICE_OK)
		return ret;

	return DEVICE_OK;  
}

int KUtils::NumericGet(KUtils k, std::string cmd, long &val)
{
	//k.SetCallback(&core);

	int ret = k.PurgeComPort(k.port_.c_str());
	if (ret != DEVICE_OK)
		return ret;

	// send command
	ret = k.SendSerialCommand(k.port_.c_str(), (k.getcmdstr_ + cmd).c_str(), k.termstr_.c_str());
	if (ret != DEVICE_OK)
		return ret;

	// block/wait for acknowledge, or until we time out;
	std::string answer;
	ret = k.GetSerialAnswer(k.port_.c_str(), k.termstr_.c_str(), answer);
	if (ret != DEVICE_OK)
		return ret;
	answer = trim(answer, " \t\n");

	if (answer.substr(0, (cmd.length() + k.getcmdstr_.length()) ) != (k.getcmdstr_ + cmd))
		return DEVICE_SERIAL_INVALID_RESPONSE;;

	std::string val_str = answer.substr((cmd.length() + k.getcmdstr_.length()), std::string::npos);
	std::istringstream ( val_str ) >> val;

	//ret = k.PurgeComPort(k.port_.c_str());
	//if (ret != DEVICE_OK)
	//	return ret;

	ret = k.GetSerialAnswer(k.port_.c_str(), k.termstr_.c_str(), answer);
	if (ret != DEVICE_OK)
		return ret;

	if (answer.substr(answer.size() - 3) != " ok")
		return DEVICE_SERIAL_INVALID_RESPONSE;;

	return DEVICE_OK;  

}

int KUtils::ToggleSet(KUtils k, std::string cmd)
{
	std::string answer;
	int ret = k.SendSerialCommand(port_.c_str(), cmd.c_str(), "\r");
	if (ret != DEVICE_OK)
		return ret;
	ret = k.GetSerialAnswer(port_.c_str(), "\r", answer);
	if (ret != DEVICE_OK)
		return ret;
	if (answer.compare(cmd + "  ok") !=0)
		return DEVICE_SERIAL_COMMAND_FAILED;
	ret = k.PurgeComPort(port_.c_str());
	if (ret != DEVICE_OK)
		return ret;

	return DEVICE_OK;
}

std::string KUtils::trim(const std::string& str, const std::string& whitespace)
{
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos)
        return ""; // no content

    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}