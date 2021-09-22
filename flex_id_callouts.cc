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


std::string kea_flex_id_template;

bool kea_flex_id_debug = false;

// Functions accessed by the hooks framework use C linkage to avoid the name
// mangling that accompanies use of the C++ compiler as well as to avoid
// issues related to namespaces.
extern "C" {

/// @brief This function is called when the library is loaded.
///
/// @param handle library handle
/// @return 0 when initialization is successful, 1 otherwise
int load(LibraryHandle& handle) {

    ConstElementPtr param_template = handle.getParameter("template");

    if(!param_template)
	    isc_throw(isc::BadValue,"Missing 'template'!");

    if (param_template->getType() != Element::string)
            isc_throw(isc::BadValue,
                       "Parameter 'template' must be a string!");
    kea_flex_id_template = param_template->stringValue(); 
    if(kea_flex_id_template.find("$m") == std::string::npos)
            isc_throw(isc::BadValue,
                       "Parameter 'template' not containts '$m'!");
    if(kea_flex_id_template.find("$i") == std::string::npos &&
       kea_flex_id_template.find("$n") == std::string::npos )
            isc_throw(isc::BadValue,
                       "Parameter 'template' not containts '$i' or '$n'!");
    ConstElementPtr param_debug = handle.getParameter("debug");

    if(param_debug) {
      if (param_debug->getType() != Element::boolean)
            isc_throw(isc::BadValue, "Parameter 'debug' must be a bool!");

      kea_flex_id_debug = param_debug->boolValue();
    }

    LOG_INFO(flex_id_logger, FLEX_ID_LOAD);
    return (0);
}

/// @brief This function is called when the library is unloaded.
///
/// @return always 0.

int unload() {
    LOG_INFO(flex_id_logger, FLEX_ID_UNLOAD);
    return (0);
}
int flex_make_path(Pkt4Ptr &pkt4,isc::dhcp::SubnetID sid,const char *src,char *buf,size_t len) {
    char *d;
    size_t l;
    int n;
    std::string ifname = pkt4->getIface();
    std::string addr = pkt4->getHWAddr()->toText(false);

    if(ifname.empty() || addr.empty()) return 0;

    for(l = 0,d = buf; src && *src && l < len-2; src++) {
	if(*src != '$') { *d++ = *src; l++; continue; }
	src++;
	switch(*src) {
	  case 'm':
		n = snprintf(d,len-2-l,"%s",addr.c_str());
		d += n; l += n;
		break;
	  case 'i':
		n = snprintf(d,len-2-l,"%s",ifname.c_str());
		d += n; l += n;
		break;
	  case 'n':
		n = snprintf(d,len-2-l,"%d",sid);
		d += n; l += n;
		break;
	  default:
	        *d++ = *src; l++;
	}
    }
    *d = '\0';
    return(1);
}

char *flex_id_readfile(char *output,size_t len, const char *path) {
    memset(output,0,len);
    if(readlink(path,output,len-1) < 0) {
	struct stat st;
	int fd = -1;
	if(stat(path,&st) < 0) {
            if(kea_flex_id_debug) {
               LOG_INFO(flex_id_logger, FLEX_ID_NO_LINK).arg(path).arg(strerror(errno));
            }
	    return NULL;
	}
	if(S_ISREG(st.st_mode) && (fd = open(path,O_RDONLY)) >= 0)
		read(fd,output,len-1);
	if(fd >= 0) close(fd);
    }
    char *d = strchr(output,'\n');
    while(d && d != output && *d <= ' ') *d-- = '\0';
    for(d = output; *d && *d <= ' '; d++);
    return d;
}

int find_id(Pkt4Ptr &pkt4, isc::dhcp::SubnetID sid, std::vector<uint8_t> &id_value) {
    char path[256],ip[64],*d;
    struct in_addr inp = { 0 };
    size_t l;
    const char *src = kea_flex_id_template.c_str();

    if(!flex_make_path(pkt4,sid,src,path,sizeof(path))) return 0;
    id_value.clear();
    d = flex_id_readfile(ip,sizeof(ip),path);

    if(!d) return 0;
    if(!inet_aton(d,&inp)) {
        if(kea_flex_id_debug) {
           LOG_INFO(flex_id_logger, FLEX_ID_BAD_IP).arg(path).arg(d);
        }
	return 0;
    }
    for(l=0; l < 4; l++,inp.s_addr >>= 8)
        id_value.push_back(inp.s_addr & 0xff);

    if(kea_flex_id_debug) {
           LOG_INFO(flex_id_logger, FLEX_ID_IP).arg(path).arg(d);
    }
    return 1;
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
    if(find_id(pkt4, sid, id_value))
	handle.setArgument("id_value", id_value);
    return (0);
}

/// @brief This function is called to retrieve the multi-threading compatibility.
///
/// @return 1 which means compatible with multi-threading.
int multi_threading_compatible() {
    return (1);
}

} // end extern "C"
