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
#include "emma-server.h"
#include "emma-resource.h"

#include <string.h>

#define LDEBUG 0
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
}

void emma_server_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	// Variables
	unsigned int responseCode = REST.status.NOT_IMPLEMENTED;
	char* pUri = NULL;
	int uriSize = 0;
	char uri[EMMA_MAX_URI_SIZE+1];
	rest_resource_flags_t rq_flag;
	emma_size_t nbBytes = 0;
	uint8_t emmaCode = 0;
	
	// For PUT method only
	uint32_t block_num = 0;
	uint8_t more = 0;
	int payloadSize = 0;
	uint8_t* payload = NULL;
	uint16_t blockSize = 0;
	static uint8_t opened = 0;
	static uint8_t locked = 0;
	
	// DEBUG only
	uint16_t dCnt = 0;
	
	// Init
	uriSize = REST.get_url (request, (const char **)(&pUri));
	rq_flag = REST.get_method_type(request);
	
	PRINT("Handler\n");
	if (uriSize && uriSize<EMMA_MAX_URI_SIZE)
	{
		memcpy(uri, pUri, uriSize);
		uri[uriSize] = '\0';
		if (rq_flag & METHOD_GET)
		{
			if (emma_resource_lock(uri))
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
		else if (rq_flag & METHOD_PUT)
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
			
			// Put payload into resource
			payloadSize = REST.get_request_payload(request, &payload);
			if (payloadSize)
			{
				if (block_num == 0)
				{
					emma_resource_add(emma_get_resource_root(uri), emma_get_resource_name(uri));
					locked = emma_resource_lock(uri);
					opened = emma_resource_open(uri);
				}
				if (locked)
				{
					if (opened)
					{
						PRINT("[HANDLER] PUT '%s' ; payload='", uri);
						for(dCnt=0 ; dCnt<payloadSize ; dCnt++) PRINTS("%c",(char)(payload[dCnt]));
						PRINTS("'\n");
						nbBytes = emma_resource_write(uri, payload, payloadSize, block_num*blockSize);
						if (nbBytes == payloadSize) responseCode = REST.status.OK;
						else responseCode = REST.status.BAD_REQUEST;
				
						if (!more)
						{
							emma_resource_set_last_modified(uri, EMMA_SERVER_ID);
							emma_resource_close(uri);
							emma_resource_release(uri); locked = 0;
						}
					}
					else
					{
						PRINT("[HANDLER] Cannot open resource\n");
						emma_resource_release(uri); locked = 0;
						responseCode = REST.status.SERVICE_UNAVAILABLE;
					}
				}
				else
				{
					PRINT("[HANDLER] Resource locked\n");
					if (opened) {emma_resource_close(uri); opened = 0;}
					else PRINT("[HANDLER] Cannot open resource\n");
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
		else if (rq_flag & METHOD_POST)
		{
			PRINT("[HANDLER] POST '%s'\n", uri);
			emmaCode = emma_resource_add(emma_get_resource_root(uri), emma_get_resource_name(uri));
			if(emmaCode==0)
			{
				emma_resource_set_last_modified(uri, EMMA_SERVER_ID);
				responseCode = REST.status.CREATED;
			}
			else
			{
				// TODO: Adapt error code according to the return value of 'emma_resource_add()'
				PRINT("Code: %d\n", emmaCode);
				responseCode = REST.status.UNAUTHORIZED;
			}
		}
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
		else responseCode = REST.status.METHOD_NOT_ALLOWED;
	}
	else
	{
		if (!uriSize) PRINT("[HANDLER] Cannot resolve URI.\n");
		if (uriSize >= EMMA_MAX_URI_SIZE) PRINT("[HANDLER] URI too long.\n"); 
		responseCode = REST.status.NOT_FOUND;
	}
	
	//responseCode = REST.status.METHOD_NOT_ALLOWED;
	REST.set_response_status(response, responseCode);
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
