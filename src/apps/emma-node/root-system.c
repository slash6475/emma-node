/*
 * E.M.M.A : Node plateform for Environment Monitoring and Management Agent 
 * Copyright (C) 2011-2013 DUHART Clement, MARDEGAN Nicolas
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
 
#include <string.h>

#include "root-system.h"
#include "memb.h"

#include "contiki.h"
#include "contiki-net.h"
#include "contiki-lib.h"


#define LDEBUG 1
#if (LDEBUG | GDEBUG)
	#define PRINT(...) OUTPUT_METHOD("[ROOT system] " __VA_ARGS__)
	#define PRINTS(...) OUTPUT_METHOD(__VA_ARGS__)
 	#define PRINT6ADDR(addr) PRINT("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#else
	#define PRINT(...)
	#define PRINTS(...)
 	#define PRINT6ADDR(...)
#endif

typedef struct rootAgent {
	int value;
} root_system_t;

#define ROOT_SYSTEM_MAX_ALLOC 6
MEMB(root_system_R, root_system_t, ROOT_SYSTEM_MAX_ALLOC);
#define ROOT_MEM_INIT() memb_init(&root_system_R)
#define ROOT_FREE(r) memb_free(&root_system_R, r)
#define ROOT_ALLOC(r) memb_alloc(&root_system_R)


extern uip_ds6_nbr_t uip_ds6_nbr_cache[];
extern uip_ds6_route_t uip_ds6_routing_table[];
extern uip_ds6_netif_t uip_ds6_if;


PROCESS(emma_system_process, "Emma System Process");

void root_system_init() {
	ROOT_MEM_INIT();

	emma_resource_add(emma_get_resource_root("S/ns-uri"), 	emma_get_resource_name("S/ns-uri"));
	emma_resource_add(emma_get_resource_root("S/rand"), 	emma_get_resource_name("S/rand"));
	emma_resource_add(emma_get_resource_root("S/time"), 	emma_get_resource_name("S/time"));
	emma_resource_add(emma_get_resource_root("S/neighbor"), emma_get_resource_name("S/neighbor"));
	emma_resource_add(emma_get_resource_root("S/routes"), 	emma_get_resource_name("S/routes"));
	emma_resource_add(emma_get_resource_root("S/resources"), emma_get_resource_name("S/resources"));

	process_start(&emma_system_process, NULL);
}

/*************************************************************************************
*
* PROCESS System 
*
**************************************************************************************/
PROCESS_THREAD(emma_system_process, ev, data)
{
  static struct etimer system_timer;

  PROCESS_BEGIN();
  PRINT("Starting Emma System Process...\n");
  etimer_set(&system_timer, EMMA_CLIENT_POLLING_INTERVAL * CLOCK_SECOND);
  int nbBytesRead;

  while(1) 
	{
    PROCESS_WAIT_EVENT();

    	if(ev == PROCESS_EVENT_TIMER)
    	{
    		if (emma_resource_lock("S/time")){
    			emma_resource_set_last_modified("S/time", EMMA_SYSTEM_ID);
  				emma_resource_release("S/time");
    		}

  			etimer_reset(&system_timer); 
    	}
	}
  PROCESS_END();
}

/*************************************************************************************
*
* INTERFACE System 
*
*************************************************************************************/
void
ipaddr_add(char* buff, const uip_ipaddr_t *addr, emma_size_t block_size)
{
  uint16_t a;
  int8_t i, f;
  for(i = 0, f = 0; i < sizeof(uip_ipaddr_t); i += 2) {
    a = (addr->u8[i] << 8) + addr->u8[i + 1];
    if(a == 0 && f >= 0) {
      if(f++ == 0) {
      	snprintf(buff, block_size, "%s::", buff);
      	if(strlen(buff) + 1 == block_size) return;
  		}
    } else {
      if(f > 0) {
        f = -1;
      } else if(i > 0) {
        snprintf(buff, block_size, "%s:", buff);
      	if(strlen(buff) + 1 == block_size) return;
      }
      snprintf(buff, block_size, "%s%x",buff, a);
      	if(strlen(buff) + 1 == block_size) return;
    }
  }
}

static void* root_system_alloc() 
{
	return ROOT_ALLOC(0);
}

void root_system_free(char* uri, void* data) 
{
	ROOT_FREE(data);
}

void root_system_reset(void* data) 
{
	
}

#define BUFF_SIZE 64
int root_system_read(char* uri, void* user_data, uint8_t* data_block, emma_size_t block_size, emma_index_t block_index)
{
	uint16_t a;
	int8_t i, j, k, f, first=1;
	int nbBytesRead = 0, Count=0, offset = 0;
	char buff[BUFF_SIZE];

	char* resource = emma_get_resource_name(uri);

	/*
	* RESOURCE time
	* Return the current runtime in second
	*/
	if(strncmp(resource, "time\0", 4) == 0)
	{
		return snprintf(data_block, block_size, "%d", clock_seconds());

	} 
	/*
	* RESOURCE rand
	* Return a random number
	*/
	else if(strncmp(resource, "rand\0",4) == 0)
	{
		return snprintf(data_block, block_size, "%d", random_rand());

	} 
	/*
	* RESOURCE info
	* Return Contiki and Emma configuration
	*/
	else if(strncmp(resource, "ns-uri\0",4) == 0)
	{
		return snprintf(data_block, block_size, "\"http://localhost/emma/registry/ns-uri1.xml\"");
	}
	/*
	* RESOURCE neighbor
	* Return the neighbor table
	*/
	else if(strncmp(resource, "neighbor\0", 8) == 0)
	{
		Count = 0;
		snprintf(data_block, block_size, "[");
		for(i = 0; i < UIP_DS6_NBR_NB; i++) 
		{
			if(uip_ds6_nbr_cache[i].isused) 
			{
				buff[0] = '\0';
				ipaddr_add(buff, &uip_ds6_nbr_cache[i].ipaddr, BUFF_SIZE);

				/*
				   Compute remaided space in packet payload with the current content and the adress ip in buff with the 2 separators
				*/
				offset = block_size - strlen(data_block) - strlen(buff) - 2;
				snprintf(data_block, block_size, "%s%c\"%s%c", data_block, (!first?',':' '), buff ,(offset>0 ? '"':' '));

				/*
				   We start writing at the \0 added by snprintf if we can add other contain
				*/
				if(offset>0) offset --;

				/* 
				   If the current address has been splited because payload is full, we send the packet
				*/
				if(offset <= 0 && Count == block_index / block_size) 
					return strlen(data_block)+1;
				

				/* 
				   If this is the address which has been splited at the previous packet sending,
				   We add it at the begining of the packet before adding the next address
				*/
				else if(offset <= 0 && Count < block_index / block_size){
					snprintf(data_block, block_size, "%s\"", buff+(strlen(buff) + offset - 1));
					Count ++;
				}

				if(first) first = 0;
			}
		}
	/*
	No more packet to send, we end JSON table and send it
	*/
	if(Count < block_index / block_size) data_block[0] = '\0';
	snprintf(data_block, block_size, "%s]",data_block);
	return strlen(data_block)+1;
	}

	/*
	* RESOURCE routes
	* Return the route table
	*/
	else if(strncmp(resource, "routes\0", 8) == 0)
	{
		Count = 0;
		snprintf(data_block, block_size, "[");

  		for(i = 0; i < UIP_DS6_ROUTE_NB; i++) 
  		{
  			for(j=0; j < 2; j++)
	   			if(uip_ds6_routing_table[i].isused) 
	   			{
	   				if(first)
						snprintf(data_block, block_size, "%s{",data_block);

					buff[0] = '\0';
					ipaddr_add(buff, ((j==0)?&uip_ds6_routing_table[i].ipaddr: &uip_ds6_routing_table[i].nexthop), BUFF_SIZE);

					/*
					   Compute remaided space in packet payload with the current content and the adress ip in buff with the 2 separators
					*/
					offset = block_size - strlen(data_block) - strlen(buff) - 4;
					snprintf(data_block, block_size, "%s%s\"%s%c", data_block, 
						(!first? (j==0?"},{":" : "):"   "), 
						buff ,
						(offset>0 ? '"':' '));
					/*
					   We start writing at the \0 added by snprintf if we can add other contain
					*/
					if(offset>0) offset --;

					/* 
					   If the current address has been splited because payload is full, we send the packet
					*/
					if(offset <= 0 && Count == block_index / block_size) 
						return strlen(data_block)+1;

					/* 
					   If this is the address which has been splited at the previous packet sending,
					   We add it at the begining of the packet before adding the next address
					*/
					else if(offset <= 0 && Count < block_index / block_size){
						Count ++;
						k = ((int8_t)strlen(buff) + offset - 1);
						if(k > 0) //strlen(buff) + offset - 1
							snprintf(data_block, block_size, "%s\"", buff+(strlen(buff) + offset - 1));
						
						else {
							snprintf(data_block, block_size, "%c", data_block[strlen(data_block)-1]=='"'?' ':'"');
							if(j) j--;
							else { i--; j++;}
						}
					}
					if(first) first = 0;
				}
		}

	/*
	No more packet to send, we end JSON table and send it
	*/
	if(Count < block_index / block_size) data_block[0] = '\0';
	else if (strlen(data_block) == 1 && data_block[0] == '[')
			snprintf(data_block, block_size, "%s]", data_block);
	else 	snprintf(data_block, block_size, "%s}]", data_block);
	return strlen(data_block)+1;
	}

	/*
	* RESOURCE resources
	* Return the resource table
	*/
	else if(strncmp(resource, "resources\0", 8) == 0)
	{
	Count = 0;
	snprintf(data_block, block_size, "[");

	get_next_resource_name_by_root_reset();
	nbBytesRead = get_next_resource_name_by_root(0, buff, block_size);
	do
	{
		for(j=nbBytesRead; j < block_size; j++)	buff[j] = '\0';
		/*
		   Compute remaided space in packet payload with the current content and the adress ip in buff with the 2 separators
		*/
		offset = block_size - strlen(data_block) - strlen(buff) - 2;
		snprintf(data_block, block_size, "%s%c\"%s%c", data_block, (!first?',':' '), buff ,(offset>0 ? '"':' '));

		/*
		   We start writing at the \0 added by snprintf if we can add other contain
		*/
		if(offset>0) offset --;
		/* 
		   If the current address has been splited because payload is full, we send the packet
		*/
		if(strlen(data_block) == block_size - 1 && Count == block_index/block_size) 
			return strlen(data_block)+1;
		
		/* 
		   If this is the address which has been splited at the previous packet sending,
		   We add it at the begining of the packet before adding the next address (offset is neative)
		*/
		else if(offset <= 0 && Count < block_index/block_size){		
			/*
				If previous iteration wasn't add all previous resource,
				we generate the buffer and send the end
			*/
			snprintf(data_block, block_size, ",\"%s\"", buff);
			snprintf(buff, block_size, "%s", data_block);

			/*
			We fill with buffer end not already sent
			*/
			snprintf(data_block, block_size, "%s", buff+(strlen(buff) + offset -2));
			Count ++;
		}
		if(first) first = 0;

		nbBytesRead = get_next_resource_name_by_root(0, buff, block_size);
	}
	while(nbBytesRead);
	/*
	No more packet to send, we end JSON table and send it
	*/
	snprintf(data_block, block_size, "%s]",data_block);

	return strlen(data_block)+1;
	}
	return 0;
}

emma_resource_root_t root_system = {
	0,
	"S",
	"ct=\"text/raw\";rt=\"system\"",
	
	root_system_init,
	root_system_alloc,
	root_system_free,
	root_system_reset,
	NULL,
	root_system_read,
	NULL,
	NULL,
	NULL,
	NULL
};
