

#include <string>
#include <vector>
#include <ios>
#include <iostream>
#include <iomanip>

enum NagiosState {
	NS_OK = 0,
	NS_UNKNOWN = 1,
	NS_WARNING = 2,
	NS_CRITICAL = 3,
};

class NagiosPerfData {
	private:
			std::string	name;
			double		value;
			int		precision;
	public:

	NagiosPerfData(std::string name, int precision, double value) : name(name), precision(precision), value(value) {};

	friend std::ostream & operator<<(std::ostream &os, NagiosPerfData pd) {
		os << pd.name << "=" << std::fixed << std::setprecision(pd.precision) << pd.value;
		return os;
	}
};

class NagiosPlugin {
	private:

	std::vector<std::pair<NagiosState, std::string>>	msglist;
	std::vector<std::string>	statestring={ "OK", "UNKNOWN", "WARNING", "CRITICAL" };
	std::vector<int>		stateexit={ 0, 3, 1, 2 };
	std::vector<NagiosPerfData>	perfdatalist;

	public:

	void addmsg(NagiosState ns, std::string message) {
		msglist.push_back(std::make_pair(ns, message));
	}

	void addperfdata(NagiosPerfData pd) {
		perfdatalist.push_back(pd);
	}

	int exit(void ) {
		NagiosState	higheststate=NS_OK;

		for(auto &msg : msglist) {
			if (msg.first == NS_UNKNOWN) {
				std::cout << "IF " << statestring[NS_UNKNOWN] << " - " << msg.second << std::endl;
				return stateexit[NS_UNKNOWN];
			}
		}

		for(auto &msg : msglist) {
			if (msg.first > higheststate)
				higheststate=msg.first;
		}

		std::cout << "IF " << statestring[higheststate] << " - ";

		for(auto &msg : msglist) {
			if (msg.first == higheststate)
				std::cout << msg.second << " ";
		}

		if (!perfdatalist.empty()) {
			std::cout << " | ";
			for(auto &pd : perfdatalist) {
				std::cout << pd << " ";
			}
		}

		std::cout << std::endl;

		return stateexit[higheststate];
	}
};


