#ifndef SMI_HPP
#define SMI_HPP

#include <string>
#include <unordered_map>
#include <stdexcept>

class SMI {
private:
	std::unordered_map<std::string,std::string>	name2oidmap;
	std::unordered_map<std::string,std::string>	oid2namemap;

	std::string lpmatch(const std::string	&match, const std::unordered_map<std::string,std::string> &map) const {
		std::string	substr=match;
		std::string	name;

		while(42) {

			try {
				name=map.at(substr);
			} catch(const std::out_of_range &e) {
				name.clear();
			}

			if (!name.empty()) {
				name.append(match.substr(substr.size()));
				return name;
			}

			size_t	pos=substr.find_last_of('.');

			if (pos > substr.size())
				break;

			substr.erase(pos);
		}

		return match;
	}
public:
	void addsmimap(const char *name, const char *oid) {
		name2oidmap[name]=oid;
		oid2namemap[oid]=name;
	}

	std::string oid2name(std::string &oid) const {
		return lpmatch(oid, oid2namemap);
	}

	std::string oid2name(const char *oid) const {
		std::string	oidstring=oid;
		return oid2name(oidstring);
	}

	std::string name2oid(std::string &nameoid) const {
		return lpmatch(nameoid, name2oidmap);
	}

	std::string name2oid(const char *nameoid) const {
		std::string	nameoidstring=nameoid;
		return lpmatch(nameoidstring, name2oidmap);
	}
};

#endif
