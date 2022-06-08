
#define STDCXX_98_HEADERS

#include <ctime>
#include <math.h>
#include <fstream>
#include <string>
#include <iomanip>
#include <iostream>
#include <unordered_map>
#include <sys/types.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <snmp_pp/snmp_pp.h>

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>

#include <syslog.h>

#include "IfState.pb.h"

namespace po = boost::program_options;
namespace bfs = boost::filesystem;
using namespace Snmp_pp;

#define BULK_MAX	10

#define CAP_RECHECK		(3600*12)
#define CAP_RECHECK_RAND	3600
#define CAP_RECHECK_SHORT	600
#define CAP_RECHECK_SHORT_RAND	60
#define DATA_STALE		600

typedef std::vector<std::pair<std::string,std::string>>	SNMPresult;

enum NagiosState {
	NS_OK = 0,
	NS_UNKNOWN = 1,
	NS_WARNING = 2,
	NS_CRITICAL = 3,
};

class PerfData {
	private:
			std::string	name;
			double		value;
			int		precision;
	public:

	PerfData(std::string name, int precision, double value) : name(name), precision(precision), value(value) {};

	friend std::ostream & operator<<(std::ostream &os, PerfData pd) {
		os << pd.name << "=" << std::fixed << std::setprecision(pd.precision) << pd.value;
		return os;
	}
};

class NagiosPlugin {
	private:

	std::vector<std::pair<NagiosState, std::string>>	msglist;
	std::vector<std::string>	statestring={ "OK", "UNKNOWN", "WARNING", "CRITICAL" };
	std::vector<int>		stateexit={ 0, 3, 1, 2 };
	std::vector<PerfData>		perfdatalist;

	public:

	void addmsg(NagiosState ns, std::string message) {
		msglist.push_back(std::make_pair(ns, message));
	}

	void addperfdata(PerfData pd) {
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

class SNMPResultList {
	Pdu	*pdu;
	SMI	 smi;
	int	status;

	public:
	SNMPResultList(Pdu *pdu, SMI &smi, int status) : pdu(pdu), smi(smi), status(status) {}

	class const_iterator {
		int			index;
		const SNMPResultList	*srl;
		Vb			vb;
		std::string		smistring;

		private:
		void update(void ) {
			srl->pdu->get_vb(vb, index);
			smistring=srl->smi.oid2name(vb.get_printable_oid());
		}

		public:

		const_iterator(const SNMPResultList *srl, int index) : index(index), srl(srl) {
			update();
		}

		const const_iterator &operator*() {
			return *this;
		}

		bool operator!=(const const_iterator &rhs) {
			return rhs.index != index;
		}

		const_iterator operator++(int junk) {
			const_iterator	i=*this;
			index++;
			update();
			return i;
		}

		const_iterator operator++() {
			const_iterator	i=*this;
			index++;
			update();
			return i;
		}

		std::string printable_value() const {
			return vb.get_printable_value();
		}

		int smivalue_int() const {
			int	val=0;
			vb.get_value(val);
			return val;
		}

		uint32_t smivalue_uint32() const {
			unsigned int	val=0;
			int rc=vb.get_value(val);
			return val;
		}

		uint64_t smivalue_uint64() const {
			pp_uint64	val=0;
			int rc=vb.get_value(val);
			return val;
		}

		bool valid() const {
			if (vb.get_syntax() == sNMP_SYNTAX_NOSUCHOBJECT ||
				vb.get_syntax() == sNMP_SYNTAX_NOSUCHINSTANCE) {
				return false;
			}
			return true;
		}

		std::string sminame(void ) const {
			return smistring;
		}

		bool sminame(const char *name) const {
			return boost::starts_with(smistring, name);
		}
	};

	const_iterator begin() {
		return const_iterator(this, 0);
	}

	const_iterator end() {
		return const_iterator(this, pdu->get_vb_count());
	}

	int size(void ) {
		return pdu->get_vb_count();
	}

	bool valid(void ) {
		return (status == SNMP_CLASS_SUCCESS);
	}

};

class SNMP {
private:
	std::string		addrstring;
	std::string		communitystring;
	std::unique_ptr<Snmp>	snmp;
	CTarget			target;
	UdpAddress		snmpaddress;
	SMI			smi;

public:
	SNMP(const std::string &a, const std::string &c, SMI &smi) : addrstring(a),communitystring(c),smi(smi) {
		int	status;

		DefaultLog::log()->set_filter(ERROR_LOG, 0);
		DefaultLog::log()->set_filter(WARNING_LOG, 0);
		DefaultLog::log()->set_filter(EVENT_LOG, 0);
		DefaultLog::log()->set_filter(INFO_LOG, 0);
		DefaultLog::log()->set_filter(DEBUG_LOG, 0);

		snmp=std::unique_ptr<Snmp>(new Snmp(status, 0, (snmpaddress.get_ip_version() == Address::version_ipv6)));

		if (status != SNMP_CLASS_SUCCESS) {
			std::cout << "SNMP++ Session Create Fail, " << snmp->error_msg(status) << "\n";
		}

		snmpaddress=addrstring.c_str();
		target=snmpaddress;

		target.set_version(version2c);
		/* Retrys and timeout in 10 ms - 100 -> 1 Second */
		target.set_retry(2);
		target.set_timeout(150);
		target.set_readcommunity(communitystring.c_str());
	}

	std::string snmpget(std::string &oidstring) {
		std::vector<std::string>	list;
		SNMPResultList			*resultlist;
		std::string			result;

		list.push_back(oidstring);

		resultlist=snmpget(list);

		if (resultlist->size() > 0)
			result=resultlist->begin().printable_value();

		delete(resultlist);
		return result;
	}

	SNMPResultList *snmpget(std::vector<std::string> &oidstrings) {
		Oid		oid;
		Pdu		*pdu=new(Pdu);
		Vb		vb;
		int		status;

		for(auto oidstring : oidstrings) {
			oid=smi.name2oid(oidstring).c_str();
			if (!oid.valid())
				continue;
			vb.set_oid(oid);
			*pdu+=vb;
		}

		target.set_retry(2);
		target.set_timeout(150);
		status=snmp->get(*pdu, target);

		if(status != SNMP_CLASS_SUCCESS) {
			std::ostringstream	msg;
			msg << addrstring << " " << " SNMP Error: " << snmp->error_msg(status)
					<< " Request id: " << pdu->get_request_id() << std::endl;
			syslog(LOG_INFO, "%s", msg.str().c_str());
		}

		return new SNMPResultList(pdu, smi, status);
	}

	SNMPresult *walk(const char *oidstring) {
		Oid		oid(smi.name2oid(oidstring).c_str());
		Pdu		pdu;
		Vb		vb;
		int		status;
		SNMPresult	*result=new SNMPresult;

		vb.set_oid(oid);
		pdu+=vb;

		while ((status=snmp->get_bulk(pdu, target, 0, BULK_MAX))== SNMP_CLASS_SUCCESS) {
			for(int z=0;z<pdu.get_vb_count();z++) {
				pdu.get_vb(vb,z);
				Oid tmp;
				vb.get_oid(tmp);
				if (oid.nCompare(oid.len(), tmp) != 0) {
					return result;
				}
				if (vb.get_syntax() != sNMP_SYNTAX_ENDOFMIBVIEW) {
					result->push_back(
						std::make_pair(
							tmp.get_printable(oid.len()+1,tmp.len()-oid.len()),
							vb.get_printable_value()
						)
					);
				} else {
					return result;
				}
			}
			pdu.set_vblist(&vb, 1);
		}

		return result;
	}
};

class EntitySensor {
private:
	SNMP	*snmp;
	int	snmpinstance;

	int	entSensorScale;
	int	entSensorPrecision;
	int	entSensorStatus;
	int	entSensorValue;
	int	entSensorType;

	bool	isvalid;
public:
	EntitySensor(SNMP *snmp, int instance) : snmp(snmp),snmpinstance(instance),
		entSensorScale(0),entSensorPrecision(0),entSensorStatus(0),entSensorValue(0),entSensorType(0) {
		std::vector<std::string>	oidstrings;

		oidstrings.push_back("entSensorScale");
		oidstrings.push_back("entSensorPrecision");
		oidstrings.push_back("entSensorStatus");
		oidstrings.push_back("entSensorValue");
		oidstrings.push_back("entSensorType");

		for(auto &s : oidstrings) {
			s.append(".");
			s.append(std::to_string(snmpinstance));
		}

		SNMPResultList *resultlist=snmp->snmpget(oidstrings);
		isvalid=resultlist->valid();

		if (!isvalid)
			return;

		for(auto &r : *resultlist) {
			if (!r.valid())
				continue;

			if (r.sminame("entSensorScale")) { entSensorScale=r.smivalue_int(); continue; }
			if (r.sminame("entSensorPrecision")) { entSensorPrecision=r.smivalue_int(); continue; }
			if (r.sminame("entSensorStatus")) { entSensorStatus=r.smivalue_int(); continue; }
			if (r.sminame("entSensorValue")) { entSensorValue=r.smivalue_int(); continue; }
			if (r.sminame("entSensorType")) { entSensorType=r.smivalue_int(); continue; }
		}
	}

	double scalepow() const {
		switch(entSensorScale) {
			case(6): { return -9; };
			case(7): { return -6; };
			case(8): { return -3; };
			case(9): { return 0; };
		}
	}

	bool valid() const {
		return isvalid;
	}

	double value() const {
		return entSensorValue*pow(10,-entSensorPrecision);
	}

	std::string printable() const {
		std::ostringstream	msg;

		double value=this->value();
		msg << value;

		switch(entSensorScale) {
			case(6): { msg << "n"; break; }
			case(7): { msg << "µ"; break; }
			case(8): { msg << "m"; break; }
		}

		/* CISCO-ENTITY-SENSOR-MIB */
		switch(entSensorType) {
			case(4): { msg << 'V'; break; }
			case(5): { msg << 'A'; break; }
			case(6): {
				msg << 'W';
				msg << " " << (double) (log10(value/1)*10) << "dBm";
				break;
			}
			case(8): { msg << "°C"; break; }
			case(14): { msg << "dBm"; break; }
		}

		return msg.str();
	}

	double dBm() const {
		double value=this->value();

		/* We have a sensor returning Watt - convert it to dBm */
		if (entSensorType == 6) {
			return (log10(value/1)*10);
		}
		/* Sensor is in dBm */
		if (entSensorType == 14) {
			return value;
		}

		return 0;
	}

	friend std::ostream &operator<<(std::ostream &os, const EntitySensor &es) {

		os << "entSensorType=" << es.entSensorType << " ";
		os << "entSensorStatus=" << es.entSensorStatus << " ";
		os << "entSensorValue=" << es.entSensorValue << " ";
		os << "entSensorScale=" << es.entSensorScale << " ";
		os << "entSensorPrecision=" << es.entSensorPrecision << " ";

		double value=pow(10, (-es.entSensorScale))*es.entSensorValue;

		//os << std::fixed;
		os << "Value: " << es.printable() << " ";

		return os;
	}
};

class Entity {
	private:
		SNMP	*snmp;
		int	snmpinstance;

		std::string	entPhysicalName;
		int		entPhysicalClass;

	public:
		Entity(SNMP *snmp, int instance) : snmp(snmp),snmpinstance(instance),
				entPhysicalClass(0) {
			std::vector<std::string>	oidstrings;

			oidstrings.push_back("entPhysicalName");
			oidstrings.push_back("entPhysicalClass");

			for(auto &s : oidstrings) {
				s.append(".");
				s.append(std::to_string(snmpinstance));
			}

			SNMPResultList *resultlist=snmp->snmpget(oidstrings);
			for(auto &r : *resultlist) {
				if (!r.valid())
					continue;

				if (r.sminame("entPhysicalName")) { entPhysicalName=r.printable_value(); continue; }
				if (r.sminame("entPhysicalClass")) { entPhysicalClass=r.smivalue_int(); continue; }
			}
		}

		std::string &PhysicalName(void ) {
			return entPhysicalName;
		}

		int PhysicalClass(void ) {
			return entPhysicalClass;
		}

		int instance(void ) {
			return snmpinstance;
		}

		std::vector<Entity> childs(void ) {
			std::vector<Entity>	childentitys;

			std::string	oid="entPhysicalChildIndex.";
			oid.append(std::to_string(snmpinstance));

			SNMPresult	*r=snmp->walk(oid.c_str());

			for(auto i : *r) {
				Entity	ce(snmp, std::stoi(i.second));
				childentitys.push_back(ce);
			}

			return childentitys;
		}

		friend std::ostream &operator<<(std::ostream &os, const Entity &e) {

			os << "entPhysicalName=" << e.entPhysicalName << " ";
			os << "entPhysicalClass=" << e.entPhysicalClass << " ";

			return os;
		}
};


class CheckIf {
	private:
		po::variables_map	vm;
		std::string		ifname;
		std::string		address;
		std::string		cachedir;
		SNMP			*snmp;
		IfCapabilities		*ifcap=nullptr;
		IfState			*ifs;
		IfState			*ifsl=nullptr;
		NagiosPlugin		*np;
	public:

		bool findinstance_in(const char *table, unsigned int *instance) {
			SNMPresult	*r=snmp->walk(table);

			for(auto i : *r) {
				if (!boost::iequals(i.second, ifname))
					continue;

				*instance=std::stoi(i.first);
				delete(r);
				return true;
			}
			delete(r);
			return false;
		}

		int findinstance(void) {
			unsigned int	instance=0;

			if (!findinstance_in("ifName", &instance))
				if (!findinstance_in("ifDescr", &instance))
					return 0;

			return instance;
		}

		bool has_snmp_variable(const char *eoidstring, int instance) {
			std::string	oidstring=eoidstring;
			std::string	value;

			oidstring.append(".");
			oidstring.append(std::to_string(instance));

			value=snmp->snmpget(oidstring);

			if (value.size() == 0)
				return false;

			return true;
		}

		EntityTransceiver *transceiver_check(void ) {
			unsigned int			instance;
			std::string			oid;
			std::vector<Entity>		entitys;

			if (!findinstance_in("entPhysicalName", &instance))
				return nullptr;

			EntityTransceiver	*transceiver=new EntityTransceiver;

			Entity	e(snmp, instance);
			entitys.push_back(e);

			int	i=0;
			while(i < entitys.size() && i < 20) {
				Entity	e=entitys.at(i);
#if 0
				2960S
				ENTITY-MIB::entPhysicalName.1038 = STRING: Te1/0/1 Module Temperature Sensor
				ENTITY-MIB::entPhysicalName.1039 = STRING: Te1/0/1 Supply Voltage Sensor
				ENTITY-MIB::entPhysicalName.1040 = STRING: Te1/0/1 Bias Current Sensor
				ENTITY-MIB::entPhysicalName.1041 = STRING: Te1/0/1 Transmit Power Sensor
				ENTITY-MIB::entPhysicalName.1042 = STRING: Te1/0/1 Receive Power Sensor
#endif

				/* Sensor available */
				if (e.PhysicalClass() == 8) {
					if (boost::starts_with(e.PhysicalName(), "voltage ") ||
						boost::ends_with(e.PhysicalName(), "Supply Voltage Sensor")) {
						transceiver->set_voltage(e.instance());
					}
					if (boost::starts_with(e.PhysicalName(), "temperature ") ||
						boost::ends_with(e.PhysicalName(), "Module Temperature Sensor")) {
						transceiver->set_temp(e.instance());
					}
					if (boost::starts_with(e.PhysicalName(), "power Tx ") ||
						boost::ends_with(e.PhysicalName(), "Transmit Power Sensor")) {
						transceiver->set_powertx(e.instance());
					}
					if (boost::starts_with(e.PhysicalName(), "power Rx ") ||
						boost::ends_with(e.PhysicalName(), "Receive Power Sensor")) {
						transceiver->set_powerrx(e.instance());
					}
					if (boost::starts_with(e.PhysicalName(), "current ") ||
						boost::ends_with(e.PhysicalName(), "Bias Current Sensor")) {
						transceiver->set_current(e.instance());
					}
				}

				std::vector<Entity>	childs=e.childs();
				entitys.insert(entitys.end(), childs.begin(), childs.end());

				i++;
			}

			if (transceiver->has_powerrx() && transceiver->has_powertx()) {
				return transceiver;
			}

			return nullptr;
		}

		void transceiver_read(void ) {
			if (!ifcap->has_transceiver())
				return;

			EntityTransceiver	*transceiver=ifcap->mutable_transceiver();

			EntitySensor	powertx(snmp, transceiver->powertx());
			EntitySensor	powerrx(snmp, transceiver->powerrx());
			EntitySensor	voltage(snmp, transceiver->voltage());
			EntitySensor	current(snmp, transceiver->current());
			EntitySensor	temp(snmp, transceiver->temp());

			std::ostringstream	msg;
			msg << std::setprecision(2) << std::fixed << "Optical Tx:" << powertx.dBm() << "dBm Rx:" << powerrx.dBm() << "dBm Temp: " << temp.printable();
			np->addmsg(NS_OK, msg.str());

			if (powertx.valid())
				np->addperfdata(PerfData("transceiver_tx_dbm", 2, (double) powertx.dBm()));
			if (powerrx.valid())
				np->addperfdata(PerfData("transceiver_rx_dbm", 2, (double) powerrx.dBm()));
			if (temp.valid())
				np->addperfdata(PerfData("transceiver_temp", 2, (double) temp.value()));
			if (current.valid())
				np->addperfdata(PerfData("transceiver_current", 2, (double) current.value()));
		}

		IfCapabilities *if_capabilities_read(void ) {
			IfCapabilities	*ifcap=new(IfCapabilities);

			std::ostringstream	msg;
			msg << address << " " << ifname << " Reading interface capabilities";
			syslog(LOG_NOTICE, "%s", msg.str().c_str());

			ifcap->set_lastchecked(time(nullptr));

			int instance=findinstance();
			if (instance)
				ifcap->set_instance(instance);

			ifcap->set_cap_errors(has_snmp_variable("ifInErrors", instance));
			ifcap->set_cap_hc_counter(has_snmp_variable("ifHCInUcastPkts", instance));
			ifcap->set_cap_lc_counter(has_snmp_variable("ifInUcastPkts", instance));
			ifcap->set_cap_ipv6_status(has_snmp_variable("ipv6IfAdminStatus", instance));
			ifcap->set_cap_hc_mcast(has_snmp_variable("ifHCInMulticastPkts", instance));
			ifcap->set_cap_cisco_errdisable(has_snmp_variable("cErrDisableRecoveryInterval", 0));
			ifcap->set_cap_dot3stats(has_snmp_variable("dot3StatsDuplexStatus", instance));

			EntityTransceiver *tr=transceiver_check();
			if (tr)
				ifcap->set_allocated_transceiver(tr);

			return ifcap;
		}

		void if_capabilities_check(void ) {
			/* Recheck if data is old */
			if (ifcap) {
				time_t	now=time(nullptr);
				int	randoffset=std::rand();

				/*
				 * If we have found the interface recheck
				 * capabilities every CAP_RECHECK seconds
				 *
				 * If the interface is unknown use CAP_RECHECK_SHORT
				 *
				 */
				if (ifcap->has_instance() && ifcap->instance() > 0) {
					if ((ifcap->lastchecked()+CAP_RECHECK
							+randoffset % CAP_RECHECK_RAND) > now)
						return;
				} else {
					if ((ifcap->lastchecked()+CAP_RECHECK_SHORT
							+randoffset % CAP_RECHECK_SHORT_RAND) > now)
						return;
				}
			}

			if (!ifcap)
				ifcap=new(IfCapabilities);

			IfCapabilities *ic=if_capabilities_read();
			if (ic)
				ifcap=ic;
		}


		bool if_read_snmp(void ) {
			std::vector<std::string>	oidstrings;

			if (!ifcap->has_instance() || ifcap->instance() == 0)
				return false;

			oidstrings.push_back("ifName");
			oidstrings.push_back("ifAlias");
			oidstrings.push_back("ifDescr");
			oidstrings.push_back("ifAdminStatus");
			oidstrings.push_back("ifOperStatus");

			if (ifcap->cap_errors()) {
				oidstrings.push_back("ifInDiscards");
				oidstrings.push_back("ifInErrors");
				oidstrings.push_back("ifOutDiscards");
				oidstrings.push_back("ifOutErrors");
				//oidstrings.push_back("ifInUnknownProtos");
			}

			if (ifcap->cap_ipv6_status()) {
				oidstrings.push_back("ipv6IfOperStatus");
				oidstrings.push_back("ipv6IfAdminStatus");
			}

			if (ifcap->cap_hc_counter()) {
				oidstrings.push_back("ifHCInOctets");
				oidstrings.push_back("ifHCInUcastPkts");
				oidstrings.push_back("ifHCOutOctets");
				oidstrings.push_back("ifHCOutUcastPkts");
				if (ifcap->cap_hc_mcast()) {
					oidstrings.push_back("ifHCInMulticastPkts");
					oidstrings.push_back("ifHCInBroadcastPkts");
					oidstrings.push_back("ifHCOutMulticastPkts");
					oidstrings.push_back("ifHCOutBroadcastPkts");
				}
			} else if (ifcap->cap_lc_counter()) {
				oidstrings.push_back("ifInOctets");
				oidstrings.push_back("ifOutOctets");
				oidstrings.push_back("ifInUcastPkts");
				oidstrings.push_back("ifInNUcastPkts");
				oidstrings.push_back("ifOutUcastPkts");
				oidstrings.push_back("ifOutNUcastPkts");
			}

			if (ifcap->cap_dot3stats()) {
				oidstrings.push_back("dot3StatsDuplexStatus");
			}

			/* Append Instance FIXME - Most like very inefficient - a lot of memmove */
			for(auto &s : oidstrings) {
				s.append(".");
				s.append(std::to_string(ifcap->instance()));
			}

			/* cErrDisableIfStatusCause needs <ifindex>.0 appended */
			if (ifcap->cap_cisco_errdisable()) {
				std::string	oid="cErrDisableIfStatusCause";
				oid.append(".");
				oid.append(std::to_string(ifcap->instance()));
				oid.append(".0");
				oidstrings.push_back(oid);
			}

			SNMPResultList *resultlist=snmp->snmpget(oidstrings);

			if (!resultlist->valid())
				return false;

			ifs->set_time(time(nullptr));
			for(auto &r : *resultlist) {
				if (!r.valid()) {
					/* This OID does not exist if there is no err-disable cause */
					if (r.sminame("cErrDisableIfStatusCause"))
						continue;

					std::ostringstream	msg;
					msg << address << " " << ifname << " Invalid value: " << r.sminame() << " = " << r.printable_value() << std::endl;
					syslog(LOG_INFO, "%s", msg.str().c_str());
					continue;
				}

				if (r.sminame("ifName")) { ifs->set_ifname(r.printable_value()); continue; }
				if (r.sminame("ifDescr")) { ifs->set_ifdescr(r.printable_value()); continue; }
				if (r.sminame("ifAlias")) { ifs->set_ifalias(r.printable_value()); continue; }

				if (r.sminame("ifAdminStatus")) { ifs->set_ifadminstatus(r.smivalue_int()); continue; }
				if (r.sminame("ifOperStatus")) { ifs->set_ifoperstatus(r.smivalue_int()); continue; }

				if (r.sminame("ipv6IfOperStatus")) { ifs->set_ipv6ifoperstatus(r.smivalue_int()); continue; }
				if (r.sminame("ipv6IfAdminStatus")) { ifs->set_ipv6ifadminstatus(r.smivalue_int()); continue; }

				if (r.sminame("ifInDiscards")) { ifs->set_ifindiscards(r.smivalue_uint32()); continue; }
				if (r.sminame("ifInErrors")) { ifs->set_ifinerrors(r.smivalue_uint32()); continue; }
				if (r.sminame("ifOutErrors")) { ifs->set_ifouterrors(r.smivalue_uint32()); continue; }
				if (r.sminame("ifOutDiscards")) { ifs->set_ifoutdiscards(r.smivalue_uint32()); continue; }
				//if (r.sminame("ifInUnknownProtos")) { ifs->set_ifinunknownprotos(r.smivalue_uint32()); continue; }

				if (r.sminame("ifInOctets")) { ifs->set_ifinoctets(r.smivalue_uint32()); continue; }
				if (r.sminame("ifOutOctets")) { ifs->set_ifoutoctets(r.smivalue_uint32()); continue; }
				if (r.sminame("ifInUcastPkts")) { ifs->set_ifinucastpkts(r.smivalue_uint32()); continue; }
				if (r.sminame("ifInNUcastPkts")) { ifs->set_ifinnucastpkts(r.smivalue_uint32()); continue; }
				if (r.sminame("ifOutUcastPkts")) { ifs->set_ifoutucastpkts(r.smivalue_uint32()); continue; }
				if (r.sminame("ifOutNUcastPkts")) { ifs->set_ifoutnucastpkts(r.smivalue_uint32()); continue; }

				if (r.sminame("ifHCInOctets")) { ifs->set_ifhcinoctets(r.smivalue_uint64()); continue; }
				if (r.sminame("ifHCInUcastPkts")) { ifs->set_ifhcinucastpkts(r.smivalue_uint64()); continue; }
				if (r.sminame("ifHCInMulticastPkts")) { ifs->set_ifhcinmulticastpkts(r.smivalue_uint64()); continue; }
				if (r.sminame("ifHCInBroadcastPkts")) { ifs->set_ifhcinbroadcastpkts(r.smivalue_uint64()); continue; }

				if (r.sminame("ifHCOutOctets")) { ifs->set_ifhcoutoctets(r.smivalue_uint64()); continue; }
				if (r.sminame("ifHCOutUcastPkts")) { ifs->set_ifhcoutucastpkts(r.smivalue_uint64()); continue; }
				if (r.sminame("ifHCOutMulticastPkts")) { ifs->set_ifhcoutmulticastpkts(r.smivalue_uint64()); continue; }
				if (r.sminame("ifHCOutBroadcastPkts")) { ifs->set_ifhcoutbroadcastpkts(r.smivalue_uint64()); continue; }

				if (r.sminame("cErrDisableIfStatusCause")) { ifs->set_cisco_errdisableifstatuscause(r.smivalue_int()); continue; }

				if (r.sminame("dot3StatsDuplexStatus")) { ifs->set_dot3statsduplexstatus(r.smivalue_int()); continue; }
			}

			return true;
		}

		const char *state_filename() {
	                static bfs::path	filename;
			std::string		ifnamenew=boost::replace_all_copy(ifname, "/", "-");

			if (cachedir.length() > 0) {
				filename=bfs::path(cachedir);
			} else {
				filename=bfs::current_path();
			}

                        filename=filename / bfs::path("checkif-" + address + "-" + ifnamenew + ".bin");

			return filename.c_str();
		}

		IfState *state_read() {
			IfState *is=new(IfState);

			std::ifstream in(state_filename(), std::ios::in | std::ios::binary);

			if (in.fail())
				return nullptr;

			try {
				if (!is->ParseFromIstream(&in))
					return nullptr;
			} catch (google::protobuf::FatalException) {
				in.close();

				std::ostringstream	msg;
				msg << "Unable to read ifstate - protobuf FatalException " << address << " " << ifname;
				syslog(LOG_NOTICE, "%s", msg.str().c_str());

				return nullptr;
			}
			in.close();

			return is;
		}

		void state_write() {
			ifs->set_allocated_ifcapabilities(ifcap);
			std::ofstream out(state_filename(), std::ios::out | std::ios::trunc | std::ios::binary);
			ifs->SerializeToOstream(&out);
			out.close();
		}

		uint64_t pkt_in(IfState *ifs) {
			if (ifcap->cap_hc_counter()) {
				return ifs->ifhcinucastpkts()+
					ifs->ifhcinmulticastpkts()+
					ifs->ifhcinbroadcastpkts()+
					ifs->ifindiscards();
			}

			return ifs->ifoutucastpkts()+
				ifs->ifoutnucastpkts()+
				ifs->ifoutdiscards();
		}

		uint64_t pkt_out(IfState *ifs) {
			if (ifcap->cap_hc_counter()) {
				return ifs->ifhcoutucastpkts()+
					ifs->ifhcoutmulticastpkts()+
					ifs->ifhcoutbroadcastpkts()+
					ifs->ifoutdiscards();
			}
			return ifs->ifoutucastpkts()+
				ifs->ifoutnucastpkts()+
				ifs->ifoutdiscards();
		}

		bool octets_out_has(IfState *ifs) {
			if (ifcap->cap_hc_counter()) {
				return ifs->has_ifhcoutoctets();
			}
			return ifs->has_ifoutoctets();
		}

		uint64_t octets_out(IfState *ifs) {
			if (ifcap->cap_hc_counter()) {
				return ifs->ifhcoutoctets();
			}
			return ifs->ifoutoctets();
		}

		bool octets_in_has(IfState *ifs) {
			if (ifcap->cap_hc_counter()) {
				return ifs->has_ifhcinoctets();
			}
			return ifs->has_ifinoctets();
		}

		uint64_t octets_in(IfState *ifs) {
			if (ifcap->cap_hc_counter()) {
				return ifs->ifhcinoctets();
			}
			return ifs->ifinoctets();
		}

		uint64_t delta_wrap_correct(uint64_t current, uint64_t last, bool sixtyfour, const std::string &what) {
			/* Did it wrap */
			if (last <= current)
				return current-last;

			uint64_t result=current+(sixtyfour ? UINT64_MAX : UINT32_MAX)-last;

			std::ostringstream	msg;

			msg << address << " " << ifname << " " << what << " delta_wrap_correct - current: " << current
				<< " last: " << last << " 64bit " << sixtyfour
				<< " result: " << result << std::endl;

			syslog(LOG_INFO, "%s", msg.str().c_str());

		}

		uint64_t octets_delta_in(void ) {
			if (!octets_in_has(ifs) || !octets_in_has(ifsl)) {
				std::ostringstream	msg;

				msg << address << " " << ifname << " " << " does not have in octets" << std::endl;

				syslog(LOG_INFO, "%s", msg.str().c_str());

				return 0;
			}
			return delta_wrap_correct(octets_in(ifs), octets_in(ifsl), ifcap->cap_hc_counter(), "octets in");
		}

		uint64_t octets_delta_out(void ) {
			if (!octets_out_has(ifs) || !octets_out_has(ifsl)) {
				std::ostringstream	msg;

				msg << address << " " << ifname << " " << " does not have out octets" << std::endl;

				syslog(LOG_INFO, "%s", msg.str().c_str());

				return 0;
			}
			return delta_wrap_correct(octets_out(ifs), octets_out(ifsl), ifcap->cap_hc_counter(), "octets out");
		}

		uint64_t pkt_delta_in(void ) {
			return pkt_in(ifs)-pkt_in(ifsl);
		}

		uint64_t pkt_delta_out(void ) {
			return pkt_out(ifs)-pkt_out(ifsl);
		}

		uint64_t delta_fault(const char *fvalue) {
			if (strcmp(fvalue, "ifInDiscards") == 0) {
				return ifs->ifindiscards()-ifsl->ifindiscards();
			}
			if (strcmp(fvalue, "ifOutDiscards") == 0) {
				return ifs->ifoutdiscards()-ifsl->ifoutdiscards();
			}
			if (strcmp(fvalue, "ifInErrors") == 0) {
				return ifs->ifinerrors()-ifsl->ifinerrors();
			}
			if (strcmp(fvalue, "ifOutErrors") == 0) {
				return ifs->ifouterrors()-ifsl->ifouterrors();
			}
		}

		/*
		 * 0.1%
		 * 2s
		 */
		uint64_t delta_max(const std::string &thresh, uint64_t delta_t, uint64_t deltapkts) {

			if (thresh.back() == '%') {
				std::string num=thresh.substr(0,thresh.size()-1);
				double perc=std::stod(num)/100;
				return (double) deltapkts*perc;
			}

			if (thresh.back() == 's') {
				std::string num=thresh.substr(0,thresh.size()-1);
				double hertz=std::stod(num);
				return (double) hertz*delta_t;
			}

			/*
			 * FIXME - Returning deltapkts on unparsable threshhold string
			 * will produce no errors/alarms - we should probably throw an error or something
			 */
			return deltapkts;
		}

		time_t time_delta(void ) {
			return ifs->time()-ifsl->time();
		}

		typedef std::vector<std::string>	threshholds;

		// 1s:2s or 0.1% or 1s:0.1% return an array of strings
		threshholds parse_threshhold(const std::string &threshhold) {
			threshholds result;

			boost::split(result, threshhold, boost::is_any_of(":"));

			if (result.size() == 1) {
				result.push_back(result.front());
			}

			return result;
		}

		void delta_alarm(const char *faultvalue, const std::string &tstring, uint64_t deltapkts) {
			uint64_t	delta_t=time_delta();
			uint64_t	fdelta=delta_fault(faultvalue);
			threshholds	threshs=parse_threshhold(tstring);
			uint64_t	maxfault;

			maxfault=delta_max(threshs[1], delta_t, deltapkts);
			if (fdelta > maxfault) {
				std::ostringstream	msg;

				msg << faultvalue << " "
					<< fdelta << " of " << deltapkts << "pkts in " << delta_t << " seconds"
					<< " - limit " <<  threshs[1] << " / " << maxfault << "pkts";

				np->addmsg(NS_CRITICAL, msg.str());

				return;
			}

			maxfault=delta_max(threshs[0], delta_t, deltapkts);
			if (fdelta > maxfault) {
				std::ostringstream	msg;

				msg << faultvalue << " "
					<< fdelta << " of " << deltapkts << "pkts in " << delta_t << " seconds"
					<< " - limit " <<  threshs[0] << " / " << maxfault << "pkts";

				np->addmsg(NS_WARNING, msg.str());
			}

			return;
		}

		std::string octets_to_mbit(uint64_t octets, time_t delta_t) {
			std::ostringstream	msg;

			double bit=octets*8/delta_t;

			msg << std::setprecision(2) << std::fixed << std::setw(7);

			if (bit < 1024) {
				msg << bit << " Bit/s";
			} else if (bit < 1024*1024) {
				msg << bit/1024 << " KBit/s";
			} else if (bit < 1024*1024*1024) {
				msg << bit/1024/1024 << " MBit/s";
			} else {
				msg << bit/1024/1024/1024 << " GBit/s";
			}

			return msg.str();
		}

		void delta_ok_string(void ) {
			std::ostringstream	msg;

			uint64_t ppsin=pkt_delta_in();
			uint64_t ppsout=pkt_delta_out();
			time_t	delta_t=time_delta();

			/* Nothing to divide - will only cause fp exceptions */
			if (delta_t == 0)
				return;

			ppsin/=time_delta();
			ppsout/=time_delta();

			msg << "in " << octets_to_mbit(octets_delta_in(), delta_t) << " " << std::setw(6) << ppsin << " pkt/s   ";
			msg << "out " << octets_to_mbit(octets_delta_out(), delta_t) << " " << std::setw(6) << ppsout << " pkt/s ";

			np->addmsg(NS_OK, msg.str());

			np->addperfdata(PerfData("pktsin", 1, ppsin));
			np->addperfdata(PerfData("pktsout", 1, ppsout));

			np->addperfdata(PerfData("octetsin", 1, ((double) octets_delta_in())/delta_t));
			np->addperfdata(PerfData("octetsout", 1, ((double) octets_delta_out())/delta_t));
		}


		void if_check_state() {
			/* Admin Down? */
			if (ifs->ifadminstatus() == 2) {
				np->addmsg(NS_WARNING, "Interface is Administrative down");
				return;
			}
			np->addperfdata(PerfData("adminstatus", 0, (double) ifs->ifadminstatus()));
			np->addperfdata(PerfData("operstatus", 0, (double) ifs->ifoperstatus()));

			if (ifs->has_cisco_errdisableifstatuscause()
					&& ifs->cisco_errdisableifstatuscause()) {
				std::ostringstream	msg;
				msg << "Interface is err-disabled - cause "  << ifs->cisco_errdisableifstatuscause() << " ";
				np->addmsg(NS_CRITICAL, msg.str());
			}

#define DUPLEX_HALF 2

			if (ifs->has_dot3statsduplexstatus()
					&& ifs->dot3statsduplexstatus() == DUPLEX_HALF) {
				np->addmsg(NS_WARNING, "Interface is half duplex");
			}

			if (ifs->ifoperstatus() != 1) {
				// ifOperStatus down should not be critical
				if (vm["nolinkstatus"].as<bool>()) {
					np->addmsg(NS_OK, "Interface is Operational down (no ifOperStatus monitoring)");
				} else {
					np->addmsg(NS_CRITICAL, "Interface is Operational down");
				}
			}

			if (ifcap->cap_ipv6_status() &&
					ifs->ipv6ifadminstatus() == 1 &&
					ifs->ipv6ifoperstatus() != 1) {
				if (vm["nolinkstatus"].as<bool>()) {
					np->addmsg(NS_OK, "Interface is IPv6 Operational down (no ifOperStatus monitoring)");
				} else {
					np->addmsg(NS_CRITICAL, "Interface is IPv6 Operational down");
				}
			}

			/* if we dont have a last state */
			if (!ifsl) {
				return;
			}

			if (!ifcap->cap_hc_counter() && !ifcap->cap_lc_counter()) {
				np->addmsg(NS_OK, "No counter");
				return;
			}

			if (!ifcap->cap_errors()) {
				np->addmsg(NS_OK, "If has no errorcounters");
				return;
			}

			delta_alarm("ifInDiscards", vm["ifindiscardsignore"].as<std::string>(), pkt_delta_in());
			delta_alarm("ifOutDiscards", vm["ifoutdiscardsignore"].as<std::string>(), pkt_delta_out());
			delta_alarm("ifInErrors", vm["ifinerrorsignore"].as<std::string>(), pkt_delta_in());
			delta_alarm("ifOutErrors", vm["ifouterrorsignore"].as<std::string>(), pkt_delta_out());

			np->addperfdata(PerfData("ifindiscards", 2, ((double) delta_fault("ifInDiscards"))/time_delta()));
			np->addperfdata(PerfData("ifoutdiscards", 2, ((double) delta_fault("ifOutDiscards"))/time_delta()));
			np->addperfdata(PerfData("ifinerrors", 2, ((double) delta_fault("ifInErrors"))/time_delta()));
			np->addperfdata(PerfData("ifouterrors", 2, ((double) delta_fault("ifOutErrors"))/time_delta()));

			delta_ok_string();
		}

		CheckIf(po::variables_map &vm, SNMP *snmp, NagiosPlugin *np) :
					vm(vm),snmp(snmp),np(np) {

			ifname=vm["ifname"].as<std::string>();
			address=vm["address"].as<std::string>();
			cachedir=vm["cachedir"].as<std::string>();

			ifs=new(IfState);

			IfState *is=state_read();
			if (is) {
				ifsl=is;
				ifcap=ifsl->release_ifcapabilities();
			} else {
				np->addmsg(NS_OK, "No previous state");
			}

			if_capabilities_check();

			if (!ifcap->has_instance() || ifcap->instance() == 0) {
				np->addmsg(NS_CRITICAL, "No such interface");
				state_write();
				return;
			}

			if (!if_read_snmp()) {
				/*
				 * If this is the first time - make shure we write a state
				 * if not we keep the old state as reading failed so we try
				 * our best to produce valid counters in the next run
				 *
				 */
				if (!ifsl)
					state_write();

				np->addmsg(NS_UNKNOWN, "Unable to get SNMP counter");
				return;
			}

			if_check_state();
			transceiver_read();
			state_write();
		}
};

int main(int argc, char **argv) {
	SMI		smi;
	NagiosPlugin	np;

	GOOGLE_PROTOBUF_VERIFY_VERSION;

	openlog("checkif", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);

	po::options_description		desc("Allowed options");
	desc.add_options()
		("help,h", "produce help message")
		("address", po::value<std::string>(), "host address")
		("community", po::value<std::string>(), "host snmp v2 community")
		("ifname", po::value<std::string>(), "interface name to monitor")
		("cachedir", po::value<std::string>(), "cache directory for state files")
		("nolinkstatus", po::bool_switch()->default_value(false), "ifOperStatus down is not critical")
		("ifindiscardsignore", po::value<std::string>()->default_value("0.1%"), "ifInDiscards ignore values")
		("ifoutdiscardsignore", po::value<std::string>()->default_value("0.1%"), "ifOutDiscards ignore values")
		("ifinerrorsignore", po::value<std::string>()->default_value("0.1%"), "ifInErrors ignore values")
		("ifouterrorsignore", po::value<std::string>()->default_value("0.1%"), "ifOutErrors ignore values")
	;
	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << desc << std::endl;
		exit(0);
	}

	if (!vm.count("address") || !vm.count("community") || !vm.count("ifname")) {
		std::cout << "Need address, community and ifname" << std::endl;
		std::cout << desc << std::endl;
		exit(-1);
	}

	smi.addsmimap("ifName","1.3.6.1.2.1.31.1.1.1.1");
	smi.addsmimap("ifInErrors","1.3.6.1.2.1.2.2.1.14");
	smi.addsmimap("ifHCInUcastPkts","1.3.6.1.2.1.31.1.1.1.7");
	smi.addsmimap("ifInUcastPkts","1.3.6.1.2.1.2.2.1.11");
	smi.addsmimap("ipv6IfOperStatus","1.3.6.1.2.1.55.1.5.1.10");
	smi.addsmimap("ipv6IfAdminStatus","1.3.6.1.2.1.55.1.5.1.9");
	smi.addsmimap("ifAlias","1.3.6.1.2.1.31.1.1.1.18");
	smi.addsmimap("ifDescr","1.3.6.1.2.1.2.2.1.2");
	smi.addsmimap("ifAdminStatus","1.3.6.1.2.1.2.2.1.7");
	smi.addsmimap("ifOperStatus","1.3.6.1.2.1.2.2.1.8");
	smi.addsmimap("ifInDiscards","1.3.6.1.2.1.2.2.1.13");
	smi.addsmimap("ifOutDiscards","1.3.6.1.2.1.2.2.1.19");
	smi.addsmimap("ifOutErrors","1.3.6.1.2.1.2.2.1.20");
	smi.addsmimap("ifInUnknownProtos","1.3.6.1.2.1.2.2.1.15");
	smi.addsmimap("ifHCInOctets","1.3.6.1.2.1.31.1.1.1.6");
	smi.addsmimap("ifHCInUcastPkts","1.3.6.1.2.1.31.1.1.1.7");
	smi.addsmimap("ifHCInMulticastPkts","1.3.6.1.2.1.31.1.1.1.8");
	smi.addsmimap("ifHCInBroadcastPkts","1.3.6.1.2.1.31.1.1.1.9");
	smi.addsmimap("ifHCOutOctets","1.3.6.1.2.1.31.1.1.1.10");
	smi.addsmimap("ifHCOutUcastPkts","1.3.6.1.2.1.31.1.1.1.11");
	smi.addsmimap("ifHCOutMulticastPkts","1.3.6.1.2.1.31.1.1.1.12");
	smi.addsmimap("ifHCOutBroadcastPkts","1.3.6.1.2.1.31.1.1.1.13");
	smi.addsmimap("ifInOctets","1.3.6.1.2.1.2.2.1.10");
	smi.addsmimap("ifInNUcastPkts","1.3.6.1.2.1.2.2.1.12");
	smi.addsmimap("ifOutOctets","1.3.6.1.2.1.2.2.1.16");
	smi.addsmimap("ifOutUcastPkts","1.3.6.1.2.1.2.2.1.17");
	smi.addsmimap("ifOutNUcastPkts","1.3.6.1.2.1.2.2.1.18");
	smi.addsmimap("dot3StatsDuplexStatus", "1.3.6.1.2.1.10.7.2.1.19");

	smi.addsmimap("entPhysicalName", "1.3.6.1.2.1.47.1.1.1.1.7");
	smi.addsmimap("entPhysicalChildIndex", "1.3.6.1.2.1.47.1.3.3.1.1");
	smi.addsmimap("entPhysicalClass", "1.3.6.1.2.1.47.1.1.1.1.5");

	smi.addsmimap("entSensorType", "1.3.6.1.4.1.9.9.91.1.1.1.1.1");
	smi.addsmimap("entSensorScale", "1.3.6.1.4.1.9.9.91.1.1.1.1.2");
	smi.addsmimap("entSensorPrecision", "1.3.6.1.4.1.9.9.91.1.1.1.1.3");
	smi.addsmimap("entSensorValue", "1.3.6.1.4.1.9.9.91.1.1.1.1.4");
	smi.addsmimap("entSensorStatus", "1.3.6.1.4.1.9.9.91.1.1.1.1.5");

	smi.addsmimap("cErrDisableRecoveryInterval", "1.3.6.1.4.1.9.9.548.1.1.1");
	smi.addsmimap("cErrDisableIfStatusCause", "1.3.6.1.4.1.9.9.548.1.3.1.1.2");

	std::srand(std::time(0)^getpid());
	Snmp::socket_startup();

	auto LogHandler = [] (google::protobuf::LogLevel level, const char* filename, int line, const std::string& message) {
		std::ostringstream	msg;
		msg << "Protobuf state faile - Filename: " << filename << " msg: " << message;
		syslog(LOG_INFO, "%s", msg.str().c_str());

	};
	google::protobuf::SetLogHandler(LogHandler);


	SNMP		s(vm["address"].as<std::string>(), vm["community"].as<std::string>(), smi);

	CheckIf		ci(
			vm,
			&s,
			&np);
	Snmp::socket_cleanup();
	google::protobuf::ShutdownProtobufLibrary();
	closelog();

	exit(np.exit());
}
