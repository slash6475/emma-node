#ifndef __PROJECT_EMMA_EXAMPLE_CONF_H__
#define __PROJECT_EMMA_EXAMPLE_CONF_H__

// Define here specific macros for the project

// Run example with the emma shell available on node
//#define EMMA_CONF_WITH_SHELL 1
//#define EMMA_SHELL 1


#define SICSLOWPAN_CONF_FRAG	1

/* Disabling RDC for demo purposes. Core updates often require more memory. */
/* For projects, optimize memory and enable RDC again. */
#undef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC     nullrdc_driver

/* Save some memory for the sky platform. */
#undef UIP_CONF_DS6_NBR_NBU
#define UIP_CONF_DS6_NBR_NBU     5
#undef UIP_CONF_DS6_ROUTE_NBU
#define UIP_CONF_DS6_ROUTE_NBU   5

/* Increase rpl-border-router IP-buffer when using 128. */
#ifndef REST_MAX_CHUNK_SIZE
#define REST_MAX_CHUNK_SIZE    64
#endif

/* Multiplies with chunk size, be aware of memory constraints. */
#ifndef COAP_MAX_OPEN_TRANSACTIONS
#define COAP_MAX_OPEN_TRANSACTIONS   2
#endif

/* Must be <= open transaction number. */
#ifndef COAP_MAX_OBSERVERS
#define COAP_MAX_OBSERVERS      COAP_MAX_OPEN_TRANSACTIONS-1
#endif

/* Reduce 802.15.4 frame queue to save RAM. */
#undef QUEUEBUF_CONF_NUM
#define QUEUEBUF_CONF_NUM               4

#endif // __PROJECT_EMMA_EXAMPLE_CONF_H__
