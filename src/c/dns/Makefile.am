
noinst_LTLIBRARIES = libdns.la

libdns_la_LIBADD = libdns/libdns.la
libdns_la_CFLAGS = -I.. -std=gnu99 -Wall -Wno-unused-function -Wno-unused-label
libdns_la_LDFLAGS = 

libdns_la_SOURCES = dns.c \
		    resolv_conf.c \
		    hosts.c

SUBDIRS = libdns

