
#ifndef SNMP_HPP
#define SNMP_HPP

#include <memory>
#include <snmp_pp/snmp_pp.h>

#include "SMI.hpp"
#include "SNMPResultList.hpp"

#define BULK_MAX	10

using namespace Snmp_pp;

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

	SNMPResultList *snmpget(std::string &oidstring) {
		std::vector<std::string>	list;
		SNMPResultList			*resultlist;

		list.push_back(oidstring);

		resultlist=snmpget(list);

		return resultlist;
	}

	std::string snmpget_printable(std::string &oidstring) {
		SNMPResultList			*resultlist;
		std::string			result;

		resultlist=snmpget(oidstring);

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

	bool has_snmp_variable_instance(const char *eoidstring, int instance) {
		std::string	oidstring=eoidstring;
		std::string	value;

		oidstring.append(".");
		oidstring.append(std::to_string(instance));

		value=snmpget_printable(oidstring);

		if (value.size() == 0)
			return false;

		return true;
	}
};

#endif
