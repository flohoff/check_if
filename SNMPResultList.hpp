
#ifndef SNMPRESULTLIST_HPP
#define SNMPRESULTLIST_HPP

#include <string>
#include <vector>
#include <snmp_pp/snmp_pp.h>

#include "SMI.hpp"

using namespace Snmp_pp;

typedef std::vector<std::pair<std::string,std::string>>	SNMPresult;
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

#endif
