/***********************************************************************
* 
* 
* Tsinghua Univ, 2016
*
***********************************************************************/

#include "Configuration.hpp"
#include "debug.hpp"

using namespace std;

std::string get_working_path()
{
    char temp [ 200 ];

    if ( getcwd(temp, 200) != 0) 
        return std::string ( temp );

    int error = errno;

    switch ( error ) {
        // EINVAL can't happen - size argument > 0

        // PATH_MAX includes the terminating nul, 
        // so ERANGE should not be returned

        case EACCES:
            throw std::runtime_error("Access denied");

        case ENOMEM:
            // I'm not sure whether this can happen or not 
            throw std::runtime_error("Insufficient storage");

        default: {
            std::ostringstream str;
            str << "Unrecognised error" << error;
            throw std::runtime_error(str.str());
        }
    }
}

Configuration::Configuration() {
	//cout << get_working_path() << endl;
	ServerCount = 0;
	read_xml("conf.xml", pt);
	ptree child = pt.get_child("address");
	for(BOOST_AUTO(pos,child.begin()); pos != child.end(); ++pos) 
    {  
        id2ip[(uint16_t)(pos->second.get<int>("id"))] = pos->second.get<string>("ip");
        ip2id[pos->second.get<string>("ip")] = pos->second.get<int>("id");
        ServerCount += 1;
    }
}

Configuration::~Configuration() {
	Debug::notifyInfo("Configuration is closed successfully.");
}

string Configuration::getIPbyID(uint16_t id) {
	return id2ip[id];
}

uint16_t Configuration::getIDbyIP(string ip) {
	return ip2id[ip];
}

unordered_map<uint16_t, string> Configuration::getInstance() {
	return id2ip;
}

int Configuration::getServerCount() {
	return ServerCount;
}
