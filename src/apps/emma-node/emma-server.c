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

#include "emma-conf.h"
#include "erbium.h"
#include "er-coap-07.h"
#include "er-coap-07-transactions.h"

#include "emma-client.h"
#include "emma-server.h"
#include "emma-resource.h"

#include <string.h>

#define LDEBUG TRUE   
#if (LDEBUG | GDEBUG | SERVER_DEBUG)
	#define PRINT(...) OUTPUT_METHOD("[EMMA SERVER] " __VA_ARGS__)
	#define PRINTS(...) OUTPUT_METHOD(__VA_ARGS__)
#else
	#define PRINT(...)
	#define PRINTS(...)
#endif

#define EMMA_PACKET_SIZE 16
//#define EMMA_PACKET_SIZE REST_MAX_CHUNK_SIZE

enum {
	EMMA_SERVER_START,
	EMMA_SERVER_ORES,
	EMMA_SERVER_SLSH,
	EMMA_SERVER_NAME,
	EMMA_SERVER_CRES,
	EMMA_SERVER_SECO,
	EMMA_SERVER_COMMA,
	EMMA_SERVER_ATTR,
	EMMA_SERVER_NEXT,
	EMMA_SERVER_END
};

#include "memb.h"
MEMB(emma_server_resources_R, resource_t, EMMA_MAX_RESOURCES);
#define EMMA_RESOURCE_MEM_INIT() memb_init(&emma_server_resources_R)
#define EMMA_RESOURCE_FREE(r) memb_free(&emma_server_resources_R, r)
#define EMMA_RESOURCE_ALLOC(r) memb_alloc(&emma_server_resources_R)

RESOURCE(emma_well_known_core, METHOD_GET, ".well-known/core", "ct=40");
RESOURCE(emma_server, METHOD_GET|METHOD_PUT|METHOD_POST|METHOD_DELETE, "\0", "ct=40");


uint8_t opened = 0;
uint8_t locked = 0;

/*********************/
PROCESS(emma_server_process, "Emma Server Process");
PROCESS_THREAD(emma_server_process, ev, data)
{
  static struct etimer timer;
  uint8_t done, i;
  char resource[EMMA_MAX_URI_SIZE];

  PROCESS_BEGIN();
  PRINT("Starting Emma Server Process...\n");
  etimer_set(&timer, EMMA_SERVER_TIMEOUT_SECOND * CLOCK_SECOND);

  do {
    PROCESS_WAIT_EVENT();

	/*
	----------------------------------------------------------------------
	WATCHDOG for deadlock
	----------------------------------------------------------------------
	*/
    if(ev == PROCESS_EVENT_TIMER){
    	done = 0;
    	do {
    		for(i=0; i < get_resources_number(); i++){
	    		done = get_next_resource_name_by_root(i, (uint8_t*)resource, EMMA_MAX_URI_SIZE);
	    		resource[done] = '\0';

	      		if(emma_resource_is_locked(resource) && 
	      			clock_seconds() - emma_resource_get_clocktime (resource) > EMMA_SERVER_TIMEOUT_SECOND &&
	      			(emma_client_state() == EMMA_CLIENT_IDLE || emma_client_state() == EMMA_CLIENT_WAITING_MUTEX))
	      		{

	      			PRINT("Timeout - Forced unlocking of resource %s\n", resource);

					if (!emma_resource_del(resource))
						emma_resource_set_last_modified(NULL, EMMA_SERVER_ID);

					else {
						PRINT("Timeout - Unable to delete partial resource %s\n", resource);
						emma_resource_release(resource);
						}
					opened = 0;
					locked = 0;
   			   		} 
   			   	}
    	} while (!done);
      etimer_set(&timer, EMMA_SERVER_TIMEOUT_SECOND * CLOCK_SECOND);
    }
  } while(1);

  PROCESS_END();
}

/*
----------------------------------------------------------------------
EMMA SERVER INITIALIZATION
----------------------------------------------------------------------
*/
void emma_server_init()
{
	EMMA_RESOURCE_MEM_INIT();
	//emma_resource_root_id_t root;
	resource_t* resource = NULL;
	
	PRINT("Starting EMMA server\n");
	PRINT("%d ROOT resources.\n", get_resources_number());
	
	// Disable er-coap-07 ".well-known/core" resource
	for (resource = (resource_t*)list_head(rest_get_resources()); resource; resource = resource->next)
  {
  	if (strncmp(resource->url, ".well-known/core", strlen(".well-known/core"))==0)
  	{
  		PRINT("Disabling .well-known/core\n");
  		rest_disable_resource(resource);
  	}
  }
  
  // Enable EMMA ".well-known/core" resource
  rest_activate_resource(&resource_emma_well_known_core);
  
  // Enable EMMA default resource
  rest_activate_resource(&resource_emma_server);

  // Start Emma Server Process
  process_start(&emma_server_process, NULL);
}


/*
----------------------------------------------------------------------
SERVER HANDLER for GET/PUT/POST/DELETE method on EMMA resources
----------------------------------------------------------------------
*/
void emma_server_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	// Variables
	unsigned int responseCode = REST.status.NOT_IMPLEMENTED;
	char* pUri = NULL;
	int uriSize = 0;
	char uri[EMMA_MAX_URI_SIZE+1];
	char buff[EMMA_MAX_URI_SIZE+1];
	rest_resource_flags_t rq_flag;
	emma_size_t nbBytes = 0;
	uint8_t emmaCode = 0;

	uint8_t i = 0, j,k;
	static unsigned long long  count;

	// For PUT method only
	uint32_t block_num = 0;
	uint8_t more = 0;
	int payloadSize = 0;
	uint8_t* payload = NULL;
	uint16_t blockSize = 0;
	
	// DEBUG only
	uint16_t dCnt = 0;
	
	// Init
	uriSize = REST.get_url (request, (const char **)(&pUri));
	rq_flag = REST.get_method_type(request);
	
	//PRINT("Handler\n");
	if (uriSize && uriSize<EMMA_MAX_URI_SIZE)
	{
		memcpy(uri, pUri, uriSize);
		uri[uriSize] = '\0';
		/*
		----------------------------------------------------------------------
		RESOURCE OPENING
		----------------------------------------------------------------------
		*/
		if (rq_flag & METHOD_GET)
		{
			/*
			* Request for the list of a particular root
			*/
			if (emma_resource_is_root(uri)){
				PRINT("[HANDLER] GET root '%s' ; preferred_size=%d ; offset=%d\n", uri, preferred_size, *offset);
				buffer[0] = '[';
				buffer[1] = '\0';
				dCnt = 0;

				nbBytes = get_next_resource_name_by_root(emma_get_resource_root(uri), buff, preferred_size);
				do {
					buff[nbBytes] = '\0';
					/*
					* Add only resource of the selected root
					*/
					if(emma_get_resource_root(uri) == emma_get_resource_root(buff) && strlen(buff) > 0)
						snprintf(buffer, preferred_size, "%s%c\"%s\"", buffer, (strlen(buffer)==1?' ':','), buff);

					if(strlen(buffer) == preferred_size-1 && dCnt < (*offset)/preferred_size){
						for(i=0; i < preferred_size; i++) buffer[i] = '\0';
						dCnt ++;
					}

					j= nbBytes;					
					nbBytes = get_next_resource_name_by_root(0, buff, preferred_size);

					if(j == 0) snprintf(buffer, preferred_size, "%s]", buffer);
				} 
				/*
				* We send if the fragmented packet to send is reached and full or if there is no more resource to add
				*/
				while((dCnt <= (*offset)/preferred_size && j != 0) && strlen(buffer) < preferred_size-1);
				*offset += strlen(buffer);
				if(strlen(buffer) < preferred_size-1) *offset = -1;

				REST.set_response_payload(response, buffer, *offset);
				responseCode = REST.status.OK;
			}

			/*
			* Request for a particular resource
			*/
			else if (emma_resource_lock(uri))
			{
				if (emma_resource_open(uri))
				{
					PRINT("[HANDLER] GET '%s' ; preferred_size=%d ; offset=%d\n", uri, preferred_size, *offset);
					nbBytes = emma_resource_read(uri, buffer, preferred_size, *offset);
					PRINT("[HANDLER] GET #%d ; payload='", (*offset)/preferred_size);
					for(dCnt=0 ; dCnt<nbBytes ; dCnt++) PRINTS("%c-",(char)(buffer[dCnt]));
					PRINTS("'\n");
					REST.set_response_payload(response, buffer, nbBytes);
					responseCode = REST.status.OK;
					if (nbBytes < preferred_size) *offset = -1;
					else *offset += preferred_size;
					emma_resource_close(uri);
				}
				else
				{
					PRINT("[HANDLER] Cannot open resource\n");
					responseCode = REST.status.SERVICE_UNAVAILABLE;
				}
				emma_resource_release(uri);
			}
			else
			{
				PRINT("[HANDLER] Resource locked\n");
				responseCode = REST.status.SERVICE_UNAVAILABLE;
			}
		}
		/*
		----------------------------------------------------------------------
		RESOURCE CREATION AND EDITION
		----------------------------------------------------------------------
		*/
		else if ((rq_flag & METHOD_PUT) || (rq_flag & METHOD_POST))
		{
			// Retrieve block number if any
			if (coap_get_header_block1(request, &block_num, &more, &blockSize, NULL))
			{
				PRINT("[HANDLER] Block #%d", block_num);
				PRINTS(" ; Size=%d", blockSize);
				PRINTS(" ; More=%d\n", more);
				coap_set_header_block1(response, block_num, 0, REST_MAX_CHUNK_SIZE);
			}
			else
			{
				block_num = 0;
				more = 0;
			}
			/* 
			----------------------------------------------------------------------
			EXIT POST METHOD if RESOURCE ALREADY EXIST
			----------------------------------------------------------------------
			If POST method and resource is already existing, 
			we drop the request without responding,
			because if it is a multi-cast request,
			it avoids to close multicast transaction for other nodes
			*/

			if((rq_flag & METHOD_POST) && emma_resource_exists(uri) && !locked){
				PRINT("[HANDLER] Resource already exist, drop request !\n");
				((coap_packet_t*)response)->mid = -1;
				return;
				responseCode = REST.status.UNAUTHORIZED;
			}
			/*----------------------------------------------------------------------*/

			/* 
			----------------------------------------------------------------------
			RESOURCE CREATION OR EDITION
			----------------------------------------------------------------------
			*/
			payloadSize = REST.get_request_payload(request, &payload);
			if (payloadSize)
			{
				/*
				First packet initialize the resource and add it in resource tree memory
				*/
				if (block_num == 0)
				{
					if(locked){
						PRINT("%s is locked by another transaction\n", uri);
						((coap_packet_t*)response)->mid = -1;
						return;
						responseCode = REST.status.SERVICE_UNAVAILABLE;						
					}
printf("AGENT %s reception\n", uri);
					count = -1;
					emma_resource_add(emma_get_resource_root(uri), emma_get_resource_name(uri));
					if (!locked)  locked = emma_resource_lock(uri);
					if (!opened)  opened = emma_resource_open(uri);
				}

				/*
				----------------------------------------------------------------------
				EXIT TRANSACTION if LOOSING PACKET
				----------------------------------------------------------------------
				If we lost a packet of object transaction, 
				we release the resource because it is incomplete,
				and close transaction without reponding anything to the sender.

				This is done in case of several multicast transactions received on the same object.
				*/
				if(block_num - count > 1){
					PRINT("[HANDLER] Lost packet => Cancel transaction\n");
					if (!emma_resource_del(uri))
						emma_resource_set_last_modified(NULL, EMMA_SERVER_ID);

					else {
						PRINT("Timeout - Unable to delete partial resource %s\n", uri);
						emma_resource_release(uri);
						}
					opened = 0;
					locked = 0;	
					((coap_packet_t*)response)->mid = -1;
					return;
					}


				/*
				----------------------------------------------------------------------
				RESOURCE WRITING in PERMANENT MEMORY
				----------------------------------------------------------------------
				The resource is stored at the resource type offset address + num_block*(size_block)
				*/
				if (locked)
				{
					if (opened)
					{	
						if(block_num == count){
							PRINT("Packet already proceeded, just ignore it \n");
							responseCode = REST.status.OK;
							return;
						}

						PRINT("[HANDLER] PUT '%s' ; payload='", uri);
						for(dCnt=0 ; dCnt<payloadSize ; dCnt++) PRINTS("%c",(char)(payload[dCnt]));
						PRINTS("'\n");
						nbBytes = emma_resource_write(uri, payload, payloadSize, block_num*blockSize);
						if (nbBytes == payloadSize) {
							PRINT("Resource writing [SUCCESS]\n");
							responseCode = REST.status.OK;
						}
						else {
							PRINT("Resource writing [FAILED]\n");
							emmaCode = emma_resource_del(uri);
							if (emmaCode==0)
							{
								emma_resource_set_last_modified(NULL, EMMA_SERVER_ID);
							}
							else
							{
								// TODO: Adapt error code according to the return value of 'emma_resource_del()'
								PRINT("Unable to delete incomplete agent (code: %d)\n", emmaCode);
								emma_resource_release(uri);
							}
							locked = 0;
							opened = 0;
							responseCode = REST.status.BAD_REQUEST;
						}

						emma_resource_set_clocktime (uri, clock_seconds());

						if (!more)
						{
printf("AGENT %s reception end\n", uri);
							emma_resource_set_last_modified(uri, EMMA_SERVER_ID);
							emma_resource_close(uri);
							emma_resource_release(uri); locked = 0;

							if((rq_flag & METHOD_POST))
								responseCode = REST.status.CREATED;
						}
					}
					else
					{
						PRINT("[HANDLER] Cannot open resource\n");
						emma_resource_release(uri); locked = 0;
						/*
						----------------------------------------------------------------------
						EXIT TRANSACTION if RESOURCE cannot be opened
						----------------------------------------------------------------------
						It exist the transaction,
						if the resource is unavailable due to not enough memory,
						without responding if it is a broadcast packet.
						*/
						((coap_packet_t*)response)->mid = -1;
						return;
						responseCode = REST.status.SERVICE_UNAVAILABLE;
					}
				}
				else
				{
					PRINT("[HANDLER] Resource locked by %d\n", emma_resource_get_last_modified(uri) );
					if (opened) {emma_resource_close(uri); opened = 0;}
					else PRINT("[HANDLER] Cannot open resource\n");

					/*
					----------------------------------------------------------------------
					EXIT TRANSACTION if RESOURCE LOCKED
					----------------------------------------------------------------------
					It exist the transaction,
					if the resource is locked by the webclient process,
					without responding if it is a broadcast packet.
					*/
					((coap_packet_t*)response)->mid = -1;
					return;
					responseCode = REST.status.SERVICE_UNAVAILABLE;
				}
			}
			else
			{
				PRINT("[HANDLER] Empty payload\n");
				if (!more)
				{
					emma_resource_set_last_modified(uri, EMMA_SERVER_ID);
					emma_resource_close(uri);
					emma_resource_release(uri);
				}
				responseCode = REST.status.BAD_REQUEST;
			}
		}

		/* 
		----------------------------------------------------------------------
		RESOURCE DELETION
		----------------------------------------------------------------------
		*/
		else if (rq_flag & METHOD_DELETE)
		{
			if (emma_resource_lock(uri))
			{
				PRINT("[HANDLER] DELETE '%s'\n", uri);
				emmaCode = emma_resource_del(uri);
				if (emmaCode==0)
				{
					emma_resource_set_last_modified(NULL, EMMA_SERVER_ID);
					responseCode = REST.status.DELETED;
				}
				else
				{
					// TODO: Adapt error code according to the return value of 'emma_resource_del()'
					PRINT("Code: %d\n", emmaCode);
					responseCode = REST.status.NOT_FOUND;
					emma_resource_release(uri);
				}
			}
			else
			{
				PRINT("[HANDLER] Resource locked\n");
				responseCode = REST.status.SERVICE_UNAVAILABLE;
			}
		}
		/*
		----------------------------------------------------------------------
		*/
		else responseCode = REST.status.METHOD_NOT_ALLOWED;
	}
	else
	{
		if (!uriSize) PRINT("[HANDLER] Cannot resolve URI.\n");
		if (uriSize >= EMMA_MAX_URI_SIZE) PRINT("[HANDLER] URI too long.\n"); 
		responseCode = REST.status.NOT_FOUND;
	}
	
	REST.set_response_status(response, responseCode);
	//count |= 1<<block_num;
	//printf("COUNT : 0x%08.8X\n", count);
	count = block_num;
}

// Simple ".well-known/core" implementation without filtering
void emma_well_known_core_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    // Variables
		unsigned int responseCode = REST.status.OK;
		static uint8_t nameBuffer[EMMA_MAX_URI_SIZE+1];
		emma_size_t nbBytesRead = 0;
		static char* resAttributes = NULL;
		static uint8_t state = EMMA_SERVER_START;
		static uint16_t outBufIndex = 0;
		static uint16_t inBufIndex = 0;
		static uint16_t globalIndex = 0;
		uint16_t cnt = 0;
		
		// Init
		nbBytesRead = 0;
		outBufIndex = 0;
		cnt = 0;
	
		//PRINT(".well-known/core: preferred_size=%d ; offset=%d\n", preferred_size, *offset);
		
		//if (*offset == 0)
		{
			responseCode = REST.status.OK;
			nbBytesRead = 0;
			resAttributes = NULL;
			state = EMMA_SERVER_START;
			outBufIndex = 0;
			inBufIndex = 0;
			globalIndex = 0;
			cnt = 0;
		}
		
		/*nbBytesRead = get_next_resource_name_by_root(1, nameBuffer, EMMA_MAX_URI_SIZE);
		while (nbBytesRead)
		{
			nameBuffer[nbBytesRead] = '\0';
			PRINT("%s\n", (char*)nameBuffer);
			nbBytesRead = get_next_resource_name_by_root(0, nameBuffer, EMMA_MAX_URI_SIZE);
		}*/
		
		
		while ((outBufIndex < preferred_size || globalIndex <= *offset) && state != EMMA_SERVER_END)
		{
			if (globalIndex <= *offset) outBufIndex = 0;
			
			if (state == EMMA_SERVER_START)
			{
				//PRINTS("[START]\n");
				nbBytesRead = get_next_resource_name_by_root(1, nameBuffer, EMMA_MAX_URI_SIZE);
				nameBuffer[nbBytesRead] = '\0';
				PRINT("Resource: %s\n", (char*)nameBuffer);
				if (!nbBytesRead) state = EMMA_SERVER_END;
				else state = EMMA_SERVER_ORES;
				outBufIndex = 0;
				inBufIndex = 0;
				resAttributes = NULL;
			}
			else if (state == EMMA_SERVER_ORES)
			{
				//PRINTS("[ORES] globalIndex=%d\n", globalIndex);
				buffer[outBufIndex] = '<';
				outBufIndex++;
				globalIndex++;
				state = EMMA_SERVER_SLSH;
			}
			else if(state == EMMA_SERVER_SLSH)
			{
				//PRINTS("[SLSH]\n");
				buffer[outBufIndex] = '/';
				outBufIndex++;
				globalIndex++;
				state = EMMA_SERVER_NAME;
			}
			else if(state == EMMA_SERVER_NAME)
			{
				//PRINTS("[NAME]\n");
				// Copy name into buffer ...
				if (nameBuffer[inBufIndex] == '\0')
				{
					inBufIndex = 0;
					state = EMMA_SERVER_CRES;
				}
				else
				{
					buffer[outBufIndex] = nameBuffer[inBufIndex];
					outBufIndex++;
					inBufIndex++;
					globalIndex++;
				}
			}
			else if(state == EMMA_SERVER_CRES)
			{
				//PRINTS("[CRES]\n");
				buffer[outBufIndex] = '>';
				outBufIndex++;
				globalIndex++;
				resAttributes = get_resources_root_description(emma_get_resource_root((char*)nameBuffer));
				if (resAttributes) state = EMMA_SERVER_SECO;
				else state = EMMA_SERVER_NEXT;
			}
			else if(state == EMMA_SERVER_SECO)
			{
				//PRINTS("[SECO]\n");
				buffer[outBufIndex] = ';';
				outBufIndex++;
				globalIndex++;
				state = EMMA_SERVER_ATTR;
			}
			else if(state == EMMA_SERVER_COMMA)
			{
				//PRINTS("[COMMA]\n");
				buffer[outBufIndex] = ',';
				outBufIndex++;
				globalIndex++;
				state = EMMA_SERVER_ORES;
			}
			else if(state == EMMA_SERVER_ATTR)
			{
				//PRINTS("[ATTR]\n");
				// Copy description into buffer
				if (resAttributes[inBufIndex] == '\0')
				{
					state = EMMA_SERVER_NEXT;
					inBufIndex = 0;
				}
				else
				{
					buffer[outBufIndex] = resAttributes[inBufIndex];
					outBufIndex++;
					globalIndex++;
					inBufIndex++;
				}
			}
			else if(state == EMMA_SERVER_NEXT)
			{
				//PRINTS("[NEXT]\n");
				nbBytesRead = get_next_resource_name_by_root(0, nameBuffer, EMMA_MAX_URI_SIZE);
				nameBuffer[nbBytesRead] = '\0';
				//PRINT("Resource: %s\n", (char*)nameBuffer);
				//PRINT("outBufIndex=%d ; inBufIndex=%d ; globalIndex=%d\n", outBufIndex, inBufIndex, globalIndex);
				//if (emma_resource_is_root((char*)nameBuffer)) PRINT("This is a ROOT\n");
				//else PRINT("This is NOT a ROOT\n");
				if (!nbBytesRead) state = EMMA_SERVER_END;
				else state = EMMA_SERVER_COMMA;
			}
			else PRINT("[WELL-KNOWN] Unknown state %d\n", state);
			
			//if (globalIndex > *offset) PRINTS("buffer[%d]='%c' ; globalIndex=%d\n", outBufIndex-1, buffer[outBufIndex-1], globalIndex);
		} // End while
		
		if (outBufIndex)
		{
			// Packet is ready
			//PRINT("[WELL-KNOWN] (%d) Sending '", outBufIndex);
			//for (cnt=0 ; cnt<outBufIndex ; cnt++) PRINTS("%c", buffer[cnt]);
			//PRINTS("'\n");
			REST.set_response_payload(response, buffer, outBufIndex);
    	REST.set_header_content_type(response, REST.type.APPLICATION_LINK_FORMAT);
    	responseCode = REST.status.OK;
		}
		else
		{
			PRINT("[WELL-KNOWN] FIXME: How to handle this case ?\n");
			// Nothing else to send
			//coap_set_status_code(response, BAD_OPTION_4_02);
			responseCode = REST.status.BAD_OPTION;
      REST.set_response_payload(response, "BlockOutOfScope", 15);
		}
		
		if(state == EMMA_SERVER_END)
		{
			//PRINT("[WELL-KNOWN] Last packet\n");
			*offset = -1;
			state = EMMA_SERVER_START;
		}
		else
		{
			//PRINT("[WELL-KNOWN] More to come\n");
			*offset += outBufIndex;
		}
		
		//responseCode = REST.status.METHOD_NOT_ALLOWED;
		REST.set_response_status(response, responseCode);   
}
