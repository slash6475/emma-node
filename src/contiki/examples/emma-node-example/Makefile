all: emma-node-example

CONTIKI=../..
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

# for static routing, if enabled
ifneq ($(TARGET), minimal-net)
ifneq ($(TARGET), native)
ifneq ($(findstring avr,$(TARGET)), avr)
${info INFO: Compiling with static routing}
PROJECT_SOURCEFILES += static-routing.c
endif
endif
endif

# variable for root Makefile.include
WITH_UIP6=1
# for some platforms
UIP_CONF_IPV6=1
UIP_CONF_RPL=1

# variable for this Makefile
# configure CoAP implementation (3|7) (er-coap-07 also supports CoAP draft 08)
WITH_COAP=7
WITH_EMMA_SHELL=0

# linker optimizations
SMALL=1
CFLAGS += -DRS232_PORT=1

# EMMA shell, requires EMMA_SHELL
ifeq ($(WITH_EMMA_SHELL), 1)
${info INFO: compiling with EMMA SHELL}
CFLAGS += -DEMMA_SHELL=1
else
${info INFO: compiling with NO EMMA SHELL}
CFLAGS += -DEMMA_SHELL=0
endif

# Coffee file system required by EMMA agents (emma-agent.h/.c)
COFFEE_FILES=2

# REST framework, requires WITH_COAP
ifeq ($(WITH_COAP), 7)
${info INFO: compiling with CoAP-08}
CFLAGS += -DWITH_COAP=7
CFLAGS += -DREST=coap_rest_implementation
CFLAGS += -DUIP_CONF_TCP=0
APPS += er-coap-07
else ifeq ($(WITH_COAP), 3)
${info INFO: compiling with CoAP-03}
CFLAGS += -DWITH_COAP=3
CFLAGS += -DREST=coap_rest_implementation
CFLAGS += -DUIP_CONF_TCP=0
APPS += er-coap-03
else
${info INFO: compiling with HTTP}
CFLAGS += -DWITH_HTTP
CFLAGS += -DREST=http_rest_implementation
CFLAGS += -DUIP_CONF_TCP=1
APPS += er-http-engine
endif

APPS += erbium emma-node

include $(CONTIKI)/Makefile.include

# border router rules
$(CONTIKI)/tools/tunslip6:	$(CONTIKI)/tools/tunslip6.c
	(cd $(CONTIKI)/tools && $(MAKE) tunslip6)

connect-router:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 aaaa::1/64

connect-router-cooja:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 -a 127.0.0.1 aaaa::1/64

tap0up:
	sudo ip address add fdfd::1/64 dev tap0
