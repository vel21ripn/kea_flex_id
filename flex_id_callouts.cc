// Copyright (C) 2021 Internet Systems Consortium, Inc. ("ISC")
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <config.h>

#include <cc/command_interpreter.h>
#include <hooks/hooks.h>
#include <flex_id_log.h>
#include <dhcp/option6_ia.h>
#include <dhcp/pkt4.h>
#include <dhcp/pkt6.h>
#include <dhcpsrv/lease.h>
#include <dhcpsrv/subnet.h>
#include <dhcpsrv/host.h>

#include <unistd.h> // readlink
#include <sys/socket.h> // inet_aton
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h> // isalpha

namespace isc {
namespace flex_id {


} // namespace flex_id
} // namespace isc

using namespace isc;
using namespace isc::asiolink;
using namespace isc::data;
using namespace isc::dhcp;
using namespace isc::hooks;
using namespace isc::flex_id;
using namespace isc::util;
using namespace isc::log;


static std::vector<std::string> flex_id_template;
static std::vector<std::string> flex_cls_template;

bool kea_flex_id_debug = false;

#define MODE_ALL 0
#define MODE_FIRST 1
int kea_flex_cls_mode = 0;
std::string flex_cls_default("");

// Functions accessed by the hooks framework use C linkage to avoid the name
// mangling that accompanies use of the C++ compiler as well as to avoid
// issues related to namespaces.
extern "C" {

static int kea_flex_get_template(ConstElementPtr pattern,
	isc::log::MessageID msg,
	std::vector<std::string> &list) 
{
    std::string value;
    if(pattern) {
        switch(pattern->getType()) {
         case Element::string:
            value = pattern->stringValue();
            LOG_INFO(flex_id_logger, msg).arg(value);
            list.push_back(value);
    	break;
         case Element::list:
            if (pattern->empty())
                    isc_throw(isc::BadValue,
                           "Parameter 'template' is empty!");

            for (auto option : pattern->listValue()) {
                if(option->getType() != Element::string)
                    isc_throw(isc::BadValue,
                           "Parameter 'template' must be a string!");
                value = option->stringValue();
                if (value.empty())
                    isc_throw(isc::BadValue,
                           "'template' is empty!");
                LOG_INFO(flex_id_logger, msg).arg(value);
                list.push_back(value);
    	}
    	break;
          default:
                isc_throw(isc::BadValue,
                     "Parameter 'template' must be a list or string");
        }
    }
    return list.size();
}

/// @brief This function is called when the library is loaded.
///
/// @param handle library handle
/// @return 0 when initialization is successful, 1 otherwise
int load(LibraryHandle& handle) {

    ConstElementPtr param_debug = handle.getParameter("debug");
    if(param_debug) {
      if (param_debug->getType() != Element::boolean)
            isc_throw(isc::BadValue, "Parameter 'debug' must be a bool!");

      kea_flex_id_debug = param_debug->boolValue();
    }

    ConstElementPtr param_mode = handle.getParameter("mode");
    if(param_mode) {
        if(param_mode->getType() != Element::string)
            isc_throw(isc::BadValue, "Parameter 'mode' must be a string!");
        std::string mode_str = param_mode->stringValue();
        if(mode_str == "all") kea_flex_cls_mode = MODE_ALL;
          else if(mode_str == "first") kea_flex_cls_mode = MODE_FIRST;
            else isc_throw(isc::BadValue, "Parameter 'mode' must be 'all' or 'first'");
    }
    LOG_INFO(flex_id_logger, FLEX_CLS_MODE).arg(kea_flex_cls_mode == MODE_FIRST ? "first":"all");

    int template_id_ok = 0;
    int template_cls_ok = 0;

    ConstElementPtr template_id = handle.getParameter("template_id");
    template_id_ok = kea_flex_get_template(template_id, FLEX_ID_OPTSTR, flex_id_template);
    ConstElementPtr template_cls = handle.getParameter("template_class");
    template_cls_ok = kea_flex_get_template(template_cls, FLEX_CLS_OPTSTR, flex_cls_template);

    ConstElementPtr param_default_class = handle.getParameter("default_class");
    if(param_default_class) {
        if(param_default_class->getType() != Element::string)
            isc_throw(isc::BadValue, "Parameter 'default_class' must be a string!");
        flex_cls_default = param_default_class->stringValue();
        if(!flex_cls_default.empty()) {
            LOG_INFO(flex_id_logger, FLEX_CLS_DEFAULT).arg(flex_cls_default);
        }
    }

    LOG_INFO(flex_id_logger, FLEX_ID_LOAD).arg(template_id_ok).arg(template_cls_ok);
    return (0);
}

/// @brief This function is called when the library is unloaded.
///
/// @return always 0.

int unload() {
    LOG_INFO(flex_id_logger, FLEX_ID_UNLOAD);
    return (0);
}

static char *flex_id_readfile(char *output,size_t len, const char *path) {
    memset(output,0,len);

    int fd = open(path,O_RDONLY);
    if(fd >= 0) {
        read(fd,output,len-1);
        close(fd);
    } else {
        if(readlink(path,output,len-1) < 0) {
            if(0 && kea_flex_id_debug) {
               LOG_INFO(flex_id_logger, FLEX_ID_NO_LINK).arg(path).arg(strerror(errno));
            }
            return NULL;
        }
    }
    char *d = strchr(output,'\n');
    // removing all space and control symbols at end of line
    while(d && d != output && *d <= ' ') *d-- = '\0';
    // skip all space and control symbols at start of line
    for(d = output; *d && *d <= ' '; d++);
    return d;
}

static void flex_get_params(Pkt4Ptr &pkt4, isc::dhcp::SubnetID sid,
		std::map<char,std::string> &macros) {

    const OptionPtr option = pkt4->getOption(DHO_DHCP_AGENT_OPTIONS);

    macros.clear();

    if(!option) { 
        if(kea_flex_id_debug) {
	  LOG_DEBUG(flex_id_logger, DBGLVL_PKT_HANDLING, "No DHCP_AGENT_OPTIONS");
	}
    } else {
      OptionPtr opt82 = option->getOption(RAI_OPTION_AGENT_CIRCUIT_ID);
      if(!opt82) {
        if(kea_flex_id_debug) {
	  LOG_DEBUG(flex_id_logger, DBGLVL_PKT_HANDLING, "No AGENT_CIRCUIT_ID");
	}
      } else {
	const OptionBuffer o82v = opt82->getData();
	if(o82v[0] != 0 || o82v[1] != 4) {
          if(kea_flex_id_debug) {
	    LOG_DEBUG(flex_id_logger, DBGLVL_PKT_HANDLING, "No port 00 04");
	  }
	  std::string  opt_id;
	  opt_id.clear();
	  bool is_text = true;
	  int opt_len = opt82->len()-2;
	  for(int i=0; i < opt_len; i++) {
	    const char c = (char)o82v[i];
	    if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
	       (c >= '0' && c <= '9') || strchr(":;.,_-=",c)) {
	          opt_id += c;
	    } else {
	          is_text = false;
	    }
	  }
	  if(is_text) {
	    macros['o'] = opt_id;
	  }
        } else {
    	    macros['v'] = boost::lexical_cast<std::string>(int ((o82v[2]*256 + o82v[3]) & 0xfff));
	    macros['p'] = boost::lexical_cast<std::string>(int ((o82v[4]*256 + o82v[5]) & 0xffff));
	}
      }

      opt82 = option->getOption(RAI_OPTION_REMOTE_ID);
      if(!opt82) { 
        if(kea_flex_id_debug) {
	  LOG_DEBUG(flex_id_logger, DBGLVL_PKT_HANDLING, "No REMOTE_ID");
	}
      } else {
	const OptionBuffer o82s = opt82->getData();
	if(o82s[0] != 0 || o82s[1] != 6) {
          if(kea_flex_id_debug) {
	    LOG_DEBUG(flex_id_logger, DBGLVL_PKT_HANDLING, "No vlan 00 06");
	  }
        } else {
	    std::string  opt_id;
	    opt_id.clear();
	    for(int i=2; i < 8; i++) {
		if(opt_id.empty()) opt_id += ':';
		uint8_t v1 = (o82s[i] & 0xf) + 0x30;
		uint8_t v2 = ((o82s[i] >> 4) & 0xf) + 0x30;
		opt_id += (char)(v2 < 0x3a ? v2 : v2+39);
		opt_id += (char)(v1 < 0x3a ? v1 : v1+39);
	    }
	    macros['s'] = opt_id;
	}
      }
    }
    std::string ifname = pkt4->getIface();
    if(!ifname.empty())
	    macros['i'] = ifname;
    std::string addr = pkt4->getHWAddr()->toText(false);
    if(!addr.empty())
	    macros['m'] = addr;
    macros['n'] = boost::lexical_cast<std::string>(sid);
}

static int flex_make_path(std::map<char,std::string> &macros,
                const char *src, char *buf, size_t len) {
    char *d;
    const char *s = src;
    size_t l;
    int n;

    for(l = 0,d = buf; src && *src && l < len-2; src++) {
        if(*src != '$') { *d++ = *src; l++; continue; }
        src++;
        if(macros.count(*src)) {
            n = snprintf(d,len-2-l,"%s",macros[*src].c_str());
            d += n; l += n;
        } else {
            if(kea_flex_id_debug) {
               LOG_INFO(flex_id_logger, FLEX_ID_NO_OPT).arg(*src);
            }
            return 0;
        }
    }
    *d = '\0';
    if(kea_flex_id_debug) {
	struct stat st;
	const char * res = lstat(buf,&st) < 0 ? "NO":"YES";
        LOG_INFO(flex_id_logger, FLEX_ID_PATH).arg(s).arg(buf).arg(res);
    }
    return(1);
}

int find_id(Pkt4Ptr &pkt4, isc::dhcp::SubnetID sid, std::vector<uint8_t> &id_value) {
    char path[256],ip[64],*d;
    struct in_addr inp = { 0 };
    size_t l;

    std::map<char,std::string> macros;

    flex_get_params(pkt4,sid,macros);
    id_value.clear();

    for (auto& src: flex_id_template) {

        if(!flex_make_path(macros,src.c_str(),path,sizeof(path)))
                continue;
        d = flex_id_readfile(ip,sizeof(ip),path);
        if(!d) continue;
        if(!inet_aton(d,&inp)) {
            if(kea_flex_id_debug) {
               LOG_INFO(flex_id_logger, FLEX_ID_BAD_IP).arg(path).arg(d);
            }
	    continue;
        }

	char id_hex_str[12];
	snprintf(id_hex_str,sizeof(id_hex_str),"0x%08x",htonl(inp.s_addr));
        LOG_INFO(flex_id_logger, FLEX_ID_IP).arg(path).arg(id_hex_str);
        
        for(l=0; l < 4; l++,inp.s_addr >>= 8)
            id_value.push_back(inp.s_addr & 0xff);

        return 1;
    }
    return 0;
}

int pkt4_receive(CalloutHandle& handle) {
    CalloutHandle::CalloutNextStep status = handle.getStatus();
    if (status == CalloutHandle::NEXT_STEP_DROP ||
        status == CalloutHandle::NEXT_STEP_SKIP) {
        return (0);
    }
    Pkt4Ptr pkt4;
    handle.getArgument("query4", pkt4);
    if(!pkt4) return(0);

    char path[256],ip[64],*d;

    std::map<char,std::string> macros;

    flex_get_params(pkt4,0,macros);

    int added_classes = 0;
    for (auto& src: flex_cls_template) {

    	if(!flex_make_path(macros,src.c_str(),path,sizeof(path)))
		continue;
	d = flex_id_readfile(ip,sizeof(ip),path);
	if(!d) continue;
	for(char *c = d; *c ; c++)
	  if(!isalnum(*c) && *c != '_') {
	    LOG_INFO(flex_id_logger, FLEX_CLS_BAD).arg(d);
	    continue;
	  }
	pkt4->addClass(d);
	LOG_INFO(flex_id_logger, FLEX_CLS_ADD).arg(d);
	added_classes++;
	if(kea_flex_cls_mode == MODE_FIRST) break;
    }
    if(!added_classes && !flex_cls_default.empty()) {
	pkt4->addClass(flex_cls_default.c_str());
	LOG_INFO(flex_id_logger, FLEX_CLS_ADD).arg(flex_cls_default);
    }
    return (0);
}

int subnet4_select(CalloutHandle& handle) {
    CalloutHandle::CalloutNextStep status = handle.getStatus();
    if (status == CalloutHandle::NEXT_STEP_DROP ||
        status == CalloutHandle::NEXT_STEP_SKIP) {
        return (0);
    }
    Pkt4Ptr pkt4;
    handle.getArgument("query4", pkt4);
    if(!pkt4) return(0);
    Subnet4Ptr subnet4;
    handle.getArgument("subnet4", subnet4);
    if(!subnet4) return(0);
    handle.setContext("flex-subnet-id",subnet4->getID());
    return (0);
}

int host4_identifier(CalloutHandle& handle) {
    CalloutHandle::CalloutNextStep status = handle.getStatus();
    if (status == CalloutHandle::NEXT_STEP_DROP ||
        status == CalloutHandle::NEXT_STEP_SKIP) {
        return (0);
    }
    Pkt4Ptr pkt4;
    handle.getArgument("query4", pkt4);
    if(!pkt4) return(0);

    isc::dhcp::SubnetID sid;
    handle.getContext("flex-subnet-id", sid);

    std::vector<uint8_t> id_value;
    if(find_id(pkt4, sid, id_value)) {
	Host::IdentifierType type = Host::IDENT_FLEX;
	handle.setArgument("id_value", id_value);
	handle.setArgument("id_type", type);
    }
    return (0);
}

/// @brief This function is called to retrieve the multi-threading compatibility.
///
/// @return 1 which means compatible with multi-threading.
int multi_threading_compatible() {
    return (1);
}

} // end extern "C"
