
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

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>

#include <syslog.h>

#include "IfState.pb.h"
#include "Nagios.hpp"
#include "SMI.hpp"
#include "SNMP.hpp"
#include "EntitySensor.hpp"
#include "SNMPResultList.hpp"

namespace po = boost::program_options;
namespace bfs = boost::filesystem;
using namespace Snmp_pp;


#define CAP_RECHECK		(3600*12)
#define CAP_RECHECK_RAND	3600
#define CAP_RECHECK_SHORT	600
#define CAP_RECHECK_SHORT_RAND	60
#define DATA_STALE		600

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

		Transceiver *entity_transceiver_check(void ) {
			unsigned int			instance;
			std::string			oid;
			std::vector<Entity>		entitys;

			if (!findinstance_in("entPhysicalName", &instance))
				return nullptr;

			Transceiver	*transceiver=new Transceiver;

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

		void entity_transceiver_read(void ) {
			if (!ifcap->cap_entity_transceiver())
				return;

			Transceiver	*transceiver=ifs->mutable_transceiver();

			EntitySensor	powertx(snmp, transceiver->powertx());
			EntitySensor	powerrx(snmp, transceiver->powerrx());
			EntitySensor	voltage(snmp, transceiver->voltage());
			EntitySensor	current(snmp, transceiver->current());
			EntitySensor	temp(snmp, transceiver->temp());

			std::ostringstream	msg;
			msg << std::setprecision(2) << std::fixed << "Optical Tx:" << powertx.dBm() << "dBm Rx:" << powerrx.dBm() << "dBm Temp: " << temp.printable();
			np->addmsg(NS_OK, msg.str());

			if (powertx.valid())
				np->addperfdata(NagiosPerfData("transceiver_tx_dbm", 2, (double) powertx.dBm()));
			if (powerrx.valid())
				np->addperfdata(NagiosPerfData("transceiver_rx_dbm", 2, (double) powerrx.dBm()));
			if (temp.valid())
				np->addperfdata(NagiosPerfData("transceiver_temp", 2, (double) temp.value()));
			if (current.valid())
				np->addperfdata(NagiosPerfData("transceiver_current", 2, (double) current.value()));
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

			ifcap->set_cap_errors(snmp->has_snmp_variable_instance("ifInErrors", instance));
			ifcap->set_cap_hc_counter(snmp->has_snmp_variable_instance("ifHCInUcastPkts", instance));
			ifcap->set_cap_lc_counter(snmp->has_snmp_variable_instance("ifInUcastPkts", instance));
			ifcap->set_cap_ipv6_status(snmp->has_snmp_variable_instance("ipv6IfAdminStatus", instance));
			ifcap->set_cap_hc_mcast(snmp->has_snmp_variable_instance("ifHCInMulticastPkts", instance));
			ifcap->set_cap_cisco_errdisable(snmp->has_snmp_variable_instance("cErrDisableRecoveryInterval", 0));
			ifcap->set_cap_dot3stats(snmp->has_snmp_variable_instance("dot3StatsDuplexStatus", instance));


			// Need to check whether Transceiver has DOM capabilities
			ifcap->set_cap_hpicf_transceiver(snmp->has_snmp_variable_instance("hpicfXcvrRxPower", instance));

			/* FIXME Leaks tranceiver object */
			Transceiver *tr=entity_transceiver_check();
			if (tr)
				ifcap->set_cap_entity_transceiver(true);

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
			Transceiver			*transceiver=nullptr;

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

			if (ifcap->cap_hpicf_transceiver()) {
				oidstrings.push_back("hpicfXcvrRxPower");
				oidstrings.push_back("hpicfXcvrTxPower");
				oidstrings.push_back("hpicfXcvrTemp");
				oidstrings.push_back("hpicfXcvrVoltage");
				oidstrings.push_back("hpicfXcvrAlarms");
				oidstrings.push_back("hpicfXcvrErrors");

				transceiver=ifs->mutable_transceiver();
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
				oid.append(".")
					.append(std::to_string(ifcap->instance()))
					.append(".0");
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

				if (r.sminame("hpicfXcvrRxPower")) { transceiver->set_powerrx(r.smivalue_int()); continue; }
				if (r.sminame("hpicfXcvrTxPower")) { transceiver->set_powertx(r.smivalue_int()); continue; }
				if (r.sminame("hpicfXcvrTemp")) { transceiver->set_temp(r.smivalue_int()); continue; }
				if (r.sminame("hpicfXcvrVoltage")) { transceiver->set_voltage(r.smivalue_uint32()); continue; }
				if (r.sminame("hpicfXcvrErrors")) { transceiver->set_errors(r.smivalue_uint32()); continue; }
				if (r.sminame("hpicfXcvrAlarms")) { transceiver->set_alarms(r.smivalue_uint32()); continue; }

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

			return result;
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
			return 0;
		}

		/*
		 * 0.1%
		 * 2s
		 */
		uint64_t delta_max(const std::string &threshall, uint64_t delta_t, uint64_t deltapkts) {
			std::vector<std::string>	tlist;
			uint64_t			maxfault=0;

			boost::split(tlist, threshall, boost::is_any_of(","));

			for(auto &thresh : tlist ) {
				if (thresh.back() == '%') {
					std::string num=thresh.substr(0,thresh.size()-1);
					double perc=std::stod(num)/100;
					uint64_t percdelta=(double) deltapkts*perc;
					maxfault=(maxfault < percdelta) ? percdelta : maxfault;
				}

				if (thresh.back() == 's') {
					std::string num=thresh.substr(0,thresh.size()-1);
					double hertz=std::stod(num);
					uint64_t hertzdelta=(double) hertz*delta_t;
					maxfault=(maxfault < hertzdelta) ? hertzdelta : maxfault;
				}
			}

			/* Return what we have or max */
			return maxfault ? maxfault : deltapkts;
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

			np->addperfdata(NagiosPerfData("pktsin", 1, ppsin));
			np->addperfdata(NagiosPerfData("pktsout", 1, ppsout));

			np->addperfdata(NagiosPerfData("octetsin", 1, ((double) octets_delta_in())/delta_t));
			np->addperfdata(NagiosPerfData("octetsout", 1, ((double) octets_delta_out())/delta_t));
		}


		void if_check_state() {
			/* Admin Down? */
			if (ifs->ifadminstatus() == 2) {
				np->addmsg(NS_WARNING, "Interface is Administrative down");
				return;
			}
			np->addperfdata(NagiosPerfData("adminstatus", 0, (double) ifs->ifadminstatus()));
			np->addperfdata(NagiosPerfData("operstatus", 0, (double) ifs->ifoperstatus()));

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

			np->addperfdata(NagiosPerfData("ifindiscards", 2, ((double) delta_fault("ifInDiscards"))/time_delta()));
			np->addperfdata(NagiosPerfData("ifoutdiscards", 2, ((double) delta_fault("ifOutDiscards"))/time_delta()));
			np->addperfdata(NagiosPerfData("ifinerrors", 2, ((double) delta_fault("ifInErrors"))/time_delta()));
			np->addperfdata(NagiosPerfData("ifouterrors", 2, ((double) delta_fault("ifOutErrors"))/time_delta()));

			delta_ok_string();

			if (ifcap->cap_hpicf_transceiver()) {
				std::ostringstream	msg;

				np->addperfdata(NagiosPerfData("transceiver_tx_dbm", 2, ((double) ifs->transceiver().powertx())/1000 ));
				np->addperfdata(NagiosPerfData("transceiver_rx_dbm", 2, ((double) ifs->transceiver().powerrx())/1000 ));
				np->addperfdata(NagiosPerfData("transceiver_temp", 2, ((double) ifs->transceiver().temp())/1000 ));
				np->addperfdata(NagiosPerfData("transceiver_voltage", 2, ((double) ifs->transceiver().voltage())/10000 ));

				msg << "Transceiver tx " << (double) ifs->transceiver().powertx()/1000 << "dBm " <<
					"rx " << (double) ifs->transceiver().powerrx()/1000 << "dBm ";

				// Process Alarms/Errors and possibly put interface to WARNING

				np->addmsg(NS_OK, msg.str());
			}
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
			entity_transceiver_read();
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
		("ifindiscardsignore", po::value<std::string>()->default_value("0.1%,1s"), "ifInDiscards ignore values")
		("ifoutdiscardsignore", po::value<std::string>()->default_value("0.1%,1s"), "ifOutDiscards ignore values")
		("ifinerrorsignore", po::value<std::string>()->default_value("0.1%,1s"), "ifInErrors ignore values")
		("ifouterrorsignore", po::value<std::string>()->default_value("0.1%,1s"), "ifOutErrors ignore values")
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

	smi.addsmimap("hpicfXcvrRxPower", "1.3.6.1.4.1.11.2.14.11.5.1.82.1.1.1.1.15");
	smi.addsmimap("hpicfXcvrTxPower", "1.3.6.1.4.1.11.2.14.11.5.1.82.1.1.1.1.14");
	smi.addsmimap("hpicfXcvrTemp", "1.3.6.1.4.1.11.2.14.11.5.1.82.1.1.1.1.11");
	smi.addsmimap("hpicfXcvrVoltage", "1.3.6.1.4.1.11.2.14.11.5.1.82.1.1.1.1.12");
	smi.addsmimap("hpicfXcvrAlarms", "1.3.6.1.4.1.11.2.14.11.5.1.82.1.1.1.1.16");
	smi.addsmimap("hpicfXcvrErrors", "1.3.6.1.4.1.11.2.14.11.5.1.82.1.1.1.1.17");
	smi.addsmimap("hpicfXcvrDiagnostics", "1.3.6.1.4.1.11.2.14.11.5.1.82.1.1.1.1.9");

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
