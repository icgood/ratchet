#ifndef __RATCHET_DNS_H
#define __RATCHET_DNS_H

#include <lua.h>

int luaopen_ratchet_dns (lua_State *L);
int luaopen_ratchet_dns_hosts (lua_State *L);
int luaopen_ratchet_dns_resolv_conf (lua_State *L);

#define RATCHET_DNS_QUERY_TYPES_DEFAULT "ratchet_dns_query_types_default"
#define RATCHET_DNS_HOSTS_DEFAULT "ratchet_dns_hosts_default"
#define RATCHET_DNS_RESOLV_CONF_DEFAULT "ratchet_dns_resolv_conf_default"

#endif
// vim:foldmethod=marker:ai:ts=4:sw=4:
