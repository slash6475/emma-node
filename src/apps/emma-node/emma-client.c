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
#include "emma-client.h"
#include "emma-resource.h"
#include "emma-eval.h"
#include "emma-JSONnParse.h"
#include "er-coap-07.h"
#include "er-coap-07-engine.h"

#define LDEBUG 0
#if (LDEBUG | GDEBUG | CLIENT_DEBUG)
	#define PRINT(...) OUTPUT_METHOD("[EMMA CLIENT] " __VA_ARGS__)
	#define PRINTS(...) OUTPUT_METHOD(__VA_ARGS__)
	#define PRINT6ADDR(addr) PRINTS("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#else
	#define PRINT(...)
	#define PRINTS(...)
	#define PRINT6ADDR(addr)
#endif

#define EMMA_CLIENT_BUFFER_SIZE	16
#define CLIENT_GET	1
#define CLIENT_PUT	2
#define CLIENT_POST	3
#define CLIENT_DELETE	4

#define EMMA_CLIENT_POLLING_INTERVAL	5

PROCESS(emma_client_process, "EMMA client process");

enum {
	EMMA_CLIENT_WAITING_MUTEX,
	EMMA_CLIENT_RUNNING,
	EMMA_CLIENT_IDLE,
	EMMA_CLIENT_START,
	EMMA_CLIENT_RELEASE
};

// Resource management
static struct etimer emma_client_timer;
static uint8_t state = EMMA_CLIENT_IDLE;
static char resource[EMMA_MAX_URI_SIZE+1];
static emma_size_t nbBytesRead = 0;
static emma_resource_root_id_t agentsRoot = 0;
static emma_index_t startIndex = 0;
static emma_index_t stopIndex = 0;

// PRE evaluation
eval_status_t err;
num_eval_t client_stack;
operand_t pre;

// Data extraction
uint8_t buffer[EMMA_CLIENT_BUFFER_SIZE];
emma_size_t cnt = 0;

// Sending
uip_ipaddr_t TARGETadd;
uint16_t TARGETport;
char TARGETuri[EMMA_MAX_URI_SIZE+1];
uint8_t TARGETmethod = 0;
uint8_t code = 0;

// POST variables
#define PRE_EVALUATED_SEND 1
#define RAW_SEND 2
static json_parser_t POSTjsonParser;
static json_state_t POSTjsonState = JSON_ERROR;
static uint16_t POSTblockNum = 0;
static uint16_t POSTnbBytesRead = 0;
static uint16_t POSTlocalWrite = 0;
static uint8_t POSTblock[EMMA_CLIENT_BUFFER_SIZE];
static emma_index_t POSTstartIndex = 0;
static emma_index_t POSTstopIndex = 0;
static uint16_t POSTcnt = 0;
static uint8_t POSTpreEvaluate = 0;
static uint8_t POSTlastPacket = 0;
static uint8_t previousFail = 0;
static uint8_t POSTLOopened = 0;
static uint8_t POSTmore = 0;

//uint16_t POSTindex = 0;
//uint8_t POSTtype = 0;
//uint16_t POSTblockcnt = 0;


static coap_packet_t request[1];
static uint8_t sendOK = 0;

// Helper functions
uint8_t isLocalAddress(uip_ipaddr_t* address);
eval_status_t client_solver (uint8_t* reference, uint8_t referenceSize, operand_t* value);
void emma_client_chunk_callback(void *response);
uint8_t getNextTARGET(char* resource, uint8_t* method, uip_ipaddr_t* address, uint16_t* port, char uri[EMMA_MAX_URI_SIZE]);
uint16_t getNextPOST(char* resource, uint8_t* block, uint16_t blockSize, uint8_t* type);
typedef uint8_t (* repl_solver) (char* symbol);
typedef int (* repl_getter) (char* name, uint8_t* data_block, uint16_t block_size, emma_index_t block_index);
uint8_t pre_evaluate (char* name, emma_index_t* block, uint8_t* output, uint8_t outputSize, repl_solver solver, repl_getter getter);

void emma_client_init()
{
	PRINT("Starting EMMA client\n");
	state = EMMA_CLIENT_IDLE;
	set_reference_solver(&client_stack, client_solver);
	process_start(&emma_client_process, NULL);
}

PROCESS_THREAD(emma_client_process, ev, data)
{
	PROCESS_BEGIN();
	
	etimer_set(&emma_client_timer, EMMA_CLIENT_POLLING_INTERVAL * CLOCK_SECOND);
	
	while(1)
	{
		// Wake up !
		if (state == EMMA_CLIENT_IDLE)
		{
			// If a resource has been modified OR if the last run was not successful for all agents
			if ((emma_resource_get_last_modified(NULL) != EMMA_CLIENT_ID) || (previousFail == 1))
			{
				//PRINT("Waking up !\n");
				state = EMMA_CLIENT_START;
				previousFail = 0;
				process_post(PROCESS_CURRENT(), PROCESS_EVENT_CONTINUE, NULL);
			}
			else
			{
				//PRINT("Sleeping for %d seconds ...\n", EMMA_CLIENT_POLLING_INTERVAL);
				etimer_reset(&emma_client_timer);
			}
		}
		else //if (state != EMMA_CLIENT_IDLE)
		{
			// Stop timer, use PAUSE() instead
			if (state == EMMA_CLIENT_START)
			{
				PRINT("[MAIN] Resource modified !\n");
				state = EMMA_CLIENT_WAITING_MUTEX;
			}
			else if (state == EMMA_CLIENT_WAITING_MUTEX)
			{
				state = EMMA_CLIENT_RUNNING;
			
				// Try to lock all resources
				// If some resources remain unlocked, set state to EMMA_CLIENT_WAITING_MUTEX
				// In order to try on next loop (maybe let some time to processes to unlock resources)
				nbBytesRead = get_next_resource_name_by_root(1, (uint8_t*)resource, EMMA_MAX_URI_SIZE);
				while (nbBytesRead)
				{
					resource[nbBytesRead] = '\0';
					if (!emma_resource_is_root(resource))
					{
						if (!emma_resource_lock(resource))
						{
							if (emma_resource_get_last_modified(resource) != EMMA_CLIENT_ID)
							{
								// Resource already locked by another process
								state = EMMA_CLIENT_WAITING_MUTEX;
							}
						}
						else
						{
							emma_resource_set_last_modified(resource, EMMA_CLIENT_ID);
							PRINT("%s locked\n", (char*)resource);
						}
					}
					nbBytesRead = get_next_resource_name_by_root(0, (uint8_t*)resource, EMMA_MAX_URI_SIZE);
				}
			
				if (state != EMMA_CLIENT_WAITING_MUTEX) PRINT("[MAIN] All resources locked\n");
			}
			else if (state == EMMA_CLIENT_RUNNING)
			{
				PRINT("[MAIN] Running agents ...\n");
			
				/* ************************************* */
				/* Write here EMMA client code on Agents */
			
				// Init variables
				agentsRoot = emma_get_resource_root("A");
			
				nbBytesRead = get_next_resource_name_by_root(agentsRoot, (uint8_t*)resource, EMMA_MAX_URI_SIZE);
				while (nbBytesRead && (agentsRoot == emma_get_resource_root(resource)))
				{
					resource[nbBytesRead] = '\0';
					if (!emma_resource_is_root(resource))
					{
						/* Print agent name */
						PRINT("[RUN] Agent '%s'\n", resource);
				
						/* Read PRE condition */
						if (emma_resource_get_index_of_pattern(resource, "$PRE", &startIndex, &stopIndex))
						{
							PRINT("[RUN] startIndex=%d ; stopIndex=%d\n", startIndex, stopIndex);
							startIndex++; // Remove extra '"' at beginning
							clean_stack(&client_stack);
							PRINT("PRE='");
							while(startIndex < stopIndex)
							{
								nbBytesRead = emma_resource_read(resource, buffer, EMMA_CLIENT_BUFFER_SIZE, startIndex);
								for(cnt=0 ; cnt<nbBytesRead ; cnt++)
								{
									 PRINTS("%c", buffer[cnt]);
									 if(buffer[cnt]=='"') {nbBytesRead=cnt; break;}
								}
								err = evaluate_block(&client_stack, buffer, nbBytesRead, NULL);
								if (err != EVAL_READY) break;
								startIndex += nbBytesRead;
								if (nbBytesRead < EMMA_CLIENT_BUFFER_SIZE) break;
							}
							PRINTS("'\n");
							if (err == EVAL_READY) err = evaluate_block(&client_stack, buffer, 0, &pre);
							if (err != EVAL_COMPLETE) pre = 0;
							PRINT("PRE evaluated as '%d'\n", pre);
						}
					
						// Get TARGET and send POST
						TARGETmethod = 0;
						PRINT("1\n");
						code = getNextTARGET(resource, &TARGETmethod, &TARGETadd, &TARGETport, TARGETuri);
						PRINT("2\n");
						if (code) code = emma_resource_get_index_of_pattern(resource, "$POST", &POSTstartIndex, &POSTstopIndex);
						PRINT("3\n");
						POSTstartIndex++; // Skip starting '['
					
						while (code && pre)
						{
							// Address found, send POST
							PRINT("[RUN] Address:  "); PRINT6ADDR(TARGETadd.u8); PRINTS("\n");
							PRINT("[RUN] Uri: '%s'\n", TARGETuri);
							PRINT("[RUN] Method: %d\n", TARGETmethod);
							//POSTblockcnt = 0;
						
							// Determine if it is an expression or an object
							JSONnParse_clean(&POSTjsonParser);
							POSTjsonState = JSON_ERROR;
							POSTnbBytesRead = 0;
							do
							{
								POSTnbBytesRead = emma_resource_read(resource, POSTblock, EMMA_CLIENT_BUFFER_SIZE, POSTstartIndex);
								PRINT("POSTdata: '");
								for(POSTcnt=0 ; POSTcnt<POSTnbBytesRead ; POSTcnt++)
								{
									PRINTS("%c", POSTblock[POSTcnt]);
									POSTjsonState = JSONnParse(&POSTjsonParser, POSTblock[POSTcnt], 0);
									if ((POSTjsonState == JSON_STR_START) || (POSTjsonState == JSON_OBJ_START)) break;
									if (POSTjsonState == JSON_ERROR) break;
								}
								PRINTS("'\n");
								POSTstartIndex += POSTcnt;
							} while((POSTnbBytesRead == EMMA_CLIENT_BUFFER_SIZE) && (POSTstartIndex < POSTstopIndex) && (POSTjsonState != JSON_STR_START) && (POSTjsonState != JSON_OBJ_START) && (POSTjsonState != JSON_ERROR));
						
						
							// Content found
							if (1) //((POSTjsonState == JSON_STR_START) || (POSTjsonState == JSON_OBJ_START))
							{
								POSTblockNum = 0;
								POSTlastPacket = 0;
								if (POSTjsonState == JSON_STR_START) POSTpreEvaluate = PRE_EVALUATED_SEND;
								if (POSTjsonState == JSON_OBJ_START) POSTpreEvaluate = RAW_SEND;
								JSONnParse_clean(&POSTjsonParser);
								do
								{
									// Prepare payload
									if (POSTpreEvaluate == PRE_EVALUATED_SEND)
									{
										PRINT("POST Expression\n");
										if (POSTblockNum == 0){POSTstartIndex++; POSTnbBytesRead = pre_evaluate (resource, &POSTstartIndex, POSTblock, EMMA_CLIENT_BUFFER_SIZE, emma_resource_exists, emma_resource_read);}
										else {POSTnbBytesRead = pre_evaluate (NULL, &POSTstartIndex, POSTblock, EMMA_CLIENT_BUFFER_SIZE, emma_resource_exists, emma_resource_read);}
									}
									else if (POSTpreEvaluate == RAW_SEND)
									{
										// DEBUG print here to understand why client failed
										PRINT("POST Object\n");
										POSTnbBytesRead = emma_resource_read(resource, POSTblock, EMMA_CLIENT_BUFFER_SIZE, POSTstartIndex);
										PRINT("POST: resource=%s ; requested=%u ; nbBytesRead=%u ; index=%u\n", resource, EMMA_CLIENT_BUFFER_SIZE, POSTnbBytesRead, POSTstartIndex);
										PRINT("POST Watching: -");
										for(POSTcnt=0 ; POSTcnt<POSTnbBytesRead ; POSTcnt++)
										{
											PRINTS("%c-", POSTblock[POSTcnt]);
											POSTjsonState = JSONnParse(&POSTjsonParser, POSTblock[POSTcnt], 0);
											if (POSTjsonState == JSON_OBJ_END)
											{
												PRINTS("\n");
												PRINT("POST last packet\n");
												POSTcnt++;
												POSTlastPacket=1;
												break;
											}
										}
										PRINTS("\n");
										POSTnbBytesRead = POSTcnt;
										POSTstartIndex += POSTnbBytesRead;
										PRINT("POST: POSTcnt=%u ; POSTstartIndex=%u\n", POSTcnt, POSTstartIndex);
									}
									else {PRINT("Unknown type.\n"); break;}
							
									// Print packet to send
									PRINTS("Sending block #%d: '", POSTblockNum);for(POSTcnt=0 ; POSTcnt<POSTnbBytesRead ; POSTcnt++) PRINTS("%c", POSTblock[POSTcnt]); PRINTS("'\n");
							
									// Prepare CoAP packet (XXX: All CoAP method not implemented !)
									if (TARGETmethod == CLIENT_GET) coap_init_message(request, COAP_TYPE_CON, COAP_PUT, 0);
									else if (TARGETmethod == CLIENT_PUT) coap_init_message(request, COAP_TYPE_CON, COAP_PUT, 0);
									else if (TARGETmethod == CLIENT_POST) coap_init_message(request, COAP_TYPE_CON, COAP_PUT, 0);
									else if (TARGETmethod == CLIENT_DELETE) coap_init_message(request, COAP_TYPE_CON, COAP_DELETE, 0);
									else coap_init_message(request, COAP_TYPE_CON, COAP_PUT, 0);
									coap_set_header_uri_path(request, TARGETuri);
									
									if ((POSTnbBytesRead == EMMA_CLIENT_BUFFER_SIZE) && (!POSTlastPacket)) POSTmore = 1;
									else POSTmore = 0;
									
									coap_set_header_block1(request, POSTblockNum, POSTmore, EMMA_CLIENT_BUFFER_SIZE);
									coap_set_payload(request, POSTblock, POSTnbBytesRead);
							
									// Send packet
									if (isLocalAddress(&TARGETadd))
									{
										PRINT("Local address\n");
										previousFail = 0;
										// Send block in a local way ...
										if (TARGETmethod == CLIENT_DELETE) {
											// In case of the delete is considering current agent, we reinitialize agent iterator function
											get_next_resource_name_by_root_reset();
											emma_resource_del(TARGETuri);
										}
										else if (strncmp(TARGETuri, resource, strlen(TARGETuri)) != 0)
										{
											
											//if (TARGETmethod != CLIENT_DELETE)
											//else
											//{
												// PUT block on local resource
												if (POSTblockNum == 0)
												{
													emma_resource_add(emma_get_resource_root(TARGETuri), emma_get_resource_name(TARGETuri));
													POSTLOopened = emma_resource_open(TARGETuri);
													if (POSTLOopened) PRINT("Local resource '%s' opened\n", TARGETuri);
												}
												if (POSTLOopened)
												{
													PRINT("Local write: uri=%s ; nbBytes=%d ; index=%d\n", TARGETuri, POSTnbBytesRead, POSTblockNum*EMMA_CLIENT_BUFFER_SIZE);
													POSTlocalWrite = emma_resource_write(TARGETuri, POSTblock, POSTnbBytesRead, POSTblockNum*EMMA_CLIENT_BUFFER_SIZE);
													if (POSTlocalWrite == POSTnbBytesRead) PRINT("Written successfully\n");
													else
													{
														PRINT("Local Write ERROR\n");
														previousFail = 1;
													}
													// TODO XXX: Check that write was correct, otherwise set previousFail to 1
												}
												else
												{
													PRINT("[LOCAL] Cannot write resource.\n");
													previousFail=1;
												}
												if (! POSTmore)
												{
													if (POSTLOopened)
													{
														emma_resource_close(TARGETuri); POSTLOopened = 0;
														// TODO XXX: Set the last agent modifier to someone else than the client to re-evaluate agents ...
													}
												}
											//}
										}
										else
										{
											PRINT("Agent cannot modify itself\n");
											previousFail = 0; // Avoid looping on agents that change themselves
										}
									}
									else
									{
										PRINT("Network address\n");
										sendOK = 0;
										if (pre) COAP_BLOCKING_REQUEST(&TARGETadd, TARGETport, request, emma_client_chunk_callback);
										if (!sendOK && pre)
										{
											PRINT("Chunk #%d not distributed !\n", POSTblockNum);
											//PRINT("TODO: Set timer to retry in a few seconds, trying with next agent ...\n");
											pre = 0;
											previousFail = 1; // Force (<=> even if no resource changed) RUN of agents on next evaluation
											break;
										}
										else
										{
											PRINT("Chunk #%d distributed.\n", POSTblockNum);
										}
									}
								
									POSTblockNum++;
								} while(POSTmore);//while (POSTnbBytesRead == EMMA_CLIENT_BUFFER_SIZE); // Tant qu'il y a de la payload à envoyer ...
								PRINT("End of POST transmission\n");
							}
							else
							{
								PRINT("No POST start character found.\n");
							}
						
							/* Print POST
							while(POSTindex == EMMA_CLIENT_BUFFER_SIZE)
							{
								for(cnt=0 ; cnt<POSTindex ; cnt++) PRINTS("%c", buffer[cnt]);
								// Send chunk
								coap_set_header_block1(request, POSTblockcnt, 1, EMMA_CLIENT_BUFFER_SIZE);
								coap_set_payload(request, buffer, POSTindex);
								sendOK = 0;
								if (pre) COAP_BLOCKING_REQUEST(&TARGETadd, TARGETport, request, emma_client_chunk_callback);
								if (!sendOK && pre) PRINT("Error POSTING chunk\n");
								POSTblockcnt++;
								POSTindex = getNextPOST(NULL, buffer, EMMA_CLIENT_BUFFER_SIZE, &POSTtype);
							}
							for(cnt=0 ; cnt<POSTindex ; cnt++) PRINTS("%c", buffer[cnt]);
							PRINTS("'\n");
						
							// Send Last chunk
							coap_set_header_block1(request, POSTblockcnt, 0, EMMA_CLIENT_BUFFER_SIZE);
							coap_set_payload(request, buffer, POSTindex);
							sendOK = 0;
							if (pre) COAP_BLOCKING_REQUEST(&TARGETadd, TARGETport, request, emma_client_chunk_callback);
							if (!sendOK && pre) PRINT("Error POSTING chunk\n");
							//POSTindex = getNextPOST(NULL, buffer, EMMA_CLIENT_BUFFER_SIZE, &POSTtype);
							//*/
						
						
						
							TARGETmethod = 0;
							if (! previousFail) code = getNextTARGET(NULL, &TARGETmethod, &TARGETadd, &TARGETport, TARGETuri);
							else code = 0;
							PRINT("Next target found !\n");
						} // while (code && pre)
					} // If not a ROOT resource
				
					/* Finalize (STOP everything if one failed OR if we DELETE an agent)*/
					if (! previousFail) nbBytesRead = get_next_resource_name_by_root(0, (uint8_t*)resource, EMMA_MAX_URI_SIZE);
					else nbBytesRead = 0;
					
					nbBytesRead = 0;
				} // while(nbBytesRead && ...)
			
				/* ************************************* */
			
				PRINT("[MAIN] Done.\n");
				emma_resource_set_last_modified(NULL, EMMA_CLIENT_ID);
				state = EMMA_CLIENT_RELEASE;
			}
			else if (state == EMMA_CLIENT_RELEASE)
			{
				nbBytesRead = get_next_resource_name_by_root(1, (uint8_t*)resource, EMMA_MAX_URI_SIZE);
				while (nbBytesRead)
				{
					resource[nbBytesRead] = '\0';
					emma_resource_release(resource);
					nbBytesRead = get_next_resource_name_by_root(0, (uint8_t*)resource, EMMA_MAX_URI_SIZE);
				}
				state = EMMA_CLIENT_IDLE;
			}
			else {PRINT("[MAIN] Unknown state switching to IDLE\n"); state=EMMA_CLIENT_IDLE;}
			
			// Select Timer or Scheduled polling
			// Timer polling: When nothing is to be done.
			// Scheduled polling: When client is currently running.
			if (state != EMMA_CLIENT_IDLE) process_post(PROCESS_CURRENT(), PROCESS_EVENT_CONTINUE, NULL);
			else etimer_reset(&emma_client_timer);
		} // if (state != EMMA_CLIENT_IDLE)
		
		PROCESS_YIELD_UNTIL(etimer_expired(&emma_client_timer) || ev == PROCESS_EVENT_CONTINUE);
	} // while(1)
	
	PROCESS_END();
}

//-------------------------------------------------------------------------
// Helper functions

#if UIP_CONF_IPV6
static uint8_t addressCMP(uip_ipaddr_t* address1, uip_ipaddr_t* address2)
{
	uint8_t addressCnt = 0;
	for (addressCnt=0 ; addressCnt<16 ; addressCnt++)
	{
		if (address1->u8[addressCnt] > address2->u8[addressCnt]) return 1;
		else if (address1->u8[addressCnt] < address2->u8[addressCnt]) return -1;
	}
	return 0;
}
#endif

uint8_t isLocalAddress(uip_ipaddr_t* address)
{
#if UIP_CONF_IPV6
	uint8_t addressCnt = 0;
	for (addressCnt=0 ; addressCnt<UIP_DS6_ADDR_NB ; addressCnt++)
	{
    if (uip_ds6_if.addr_list[addressCnt].isused && addressCMP(address, &(uip_ds6_if.addr_list[addressCnt].ipaddr)) == 0)
    {
      return 1;
    }
  }
#else
#warning EMMA Local addressing not implemented
	PRINT("isLocalAddress() not implemented\n");
#endif
	return 0;
}

void emma_client_chunk_callback(void *response)
{
  coap_packet_t* res = (coap_packet_t *)response;
  PRINT("[RESPONSE] %d.%02d\n", (res->code>>5)&0x0F, res->code&0x0F);
  if (((res->code>>5)&0x0F) != 2) sendOK = 0;
  else sendOK = 1;
}

// Return < blockSize when end of PRE reached, then continue on the same agent with the next PRE
uint16_t getNextPRE(char* resource, uint8_t* block, uint16_t blockSize)
{
	static char* agent = NULL;
	static emma_index_t startIndex = 0;
	static emma_index_t stopIndex = 0;
	emma_size_t nbBytesRead = 0;
	emma_size_t cnt = 0;
	
	if (resource)
	{
		agent = resource;
		startIndex = 0;
		stopIndex = 0;
		if (!emma_resource_get_index_of_pattern(agent, "$PRE", &startIndex, &stopIndex)) return 0;
	}
	
	while(startIndex < stopIndex)
	{
		nbBytesRead = emma_resource_read(agent, block, blockSize, startIndex);
		for(cnt=0 ; cnt<nbBytesRead ; cnt++) PRINTS("%c", buffer[cnt]);
		startIndex += nbBytesRead;
		if (nbBytesRead < EMMA_CLIENT_BUFFER_SIZE) break;
	}
							
	nbBytesRead = emma_resource_read(agent, block, blockSize, startIndex);
	
}

// Retourne 1 si address trouvée, 2 si URI too long, 0 sinon
#define BLOCK_SIZE 16
uint8_t getNextTARGET(char* resource, uint8_t* method, uip_ipaddr_t* address, uint16_t* port, char uri[EMMA_MAX_URI_SIZE+1])
{
	static char* agent = NULL;
	static emma_index_t startIndex = 0;
	static emma_index_t stopIndex = 0;
	emma_size_t nbBytesRead = 0;
	emma_size_t cnt = 0;
	uint8_t found = 0;
	uint8_t uriIndex = 0;
	uint8_t block[BLOCK_SIZE];
	uint8_t methodSet = 0;
	uint8_t index = 0;
	
	static char* patterns[4] = {
	"GET",
	"PUT",
	"POST",
	"DELETE"
	};
	static uint8_t patternIndex[4];
	
	
	if (resource)
	{
		agent = resource;
		startIndex = 0;
		stopIndex = 0;
		if (!emma_resource_get_index_of_pattern(agent, "$TARGET", &startIndex, &stopIndex)) return 0;
		startIndex++;
		found = 0;
		uriIndex = 0;
		methodSet = 0;
		for (index=0 ; index<4 ; index++) patternIndex[index]=0;
	}
	
	while(startIndex < stopIndex)
	{
		nbBytesRead = emma_resource_read(agent, block, BLOCK_SIZE, startIndex);
		
		for(cnt=0 ; cnt<nbBytesRead ; cnt++)
		{
			if (found && block[cnt]!='"')
			{
				// Fill user uri
				if (uriIndex < EMMA_MAX_URI_SIZE)
				{
					uri[uriIndex] = block[cnt];
					uriIndex++;
				}
				else
				{
					// URI too long
					startIndex += cnt;
					return 2;
				}
			}
			
			if (found && block[cnt]=='"')
			{
				// Return result
				uri[uriIndex] = '\0';
				startIndex += cnt;
				return 1;
			}
			
			if (!found && !methodSet)
			{
				//PRINT("[getNextTARGET] Watching: %c\n", block[cnt]);
				if (block[cnt]=='[') methodSet = 1;
				else
				{
					for (index=0 ; index<4 ; index++)
					{
						if (patterns[index][patternIndex[index]] == block[cnt]) patternIndex[index]++;
						else patternIndex[index] = 0;
						if (patterns[index][patternIndex[index]] == '\0')
						{
							PRINT("[getNextTARGET] Pattern %s found !\n", patterns[index]);
							*method = index+1;
							methodSet = 1;
							break;
						}
					}
				}
			}
			
			if (!found && methodSet && (getIPaddress (block[cnt], address, port) == EMMA_EVAL_END)) found = 1;
		}

		startIndex += nbBytesRead;
		if (nbBytesRead < BLOCK_SIZE) break;
	}
	
	return 0;
}


// Return < blockSize when end of POST reached, then continue on the same agent with the next POST
#define TYPE_EMPTY	0
#define TYPE_OBJECT	1
#define TYPE_EXPRESSION	2
#define TYPE_STOP	3
uint16_t getNextPOST(char* resource, uint8_t* block, uint16_t blockSize, uint8_t* type)
{
	static char* agent = NULL;
	static emma_index_t startIndex = 0;
	static emma_index_t stopIndex = 0;
	uint16_t leftIndex = 0;
	uint16_t i = 0;
	emma_size_t nbBytesRead = 0;
	emma_size_t cnt = 0;
	static uint8_t found = 0;
	static json_parser_t jsonParser;
	static json_state_t jsonState;
	
	if (type) *type = TYPE_EMPTY;
	
	if (resource)
	{
		agent = resource;
		startIndex = 0;
		stopIndex = 0;
		JSONnParse_clean(&jsonParser);
		jsonState = JSON_ERROR;
		leftIndex = 0;
		if (!emma_resource_get_index_of_pattern(agent, "$POST", &startIndex, &stopIndex)) return 0;
		startIndex++;
		found = 0;
		//PRINT("[getNextPOST] startIndex=%d ; stopIndex=%d\n", startIndex, stopIndex);
	}
	
	//PRINT("[getNextPOST] startIndex=%d ; stopIndex=%d\n", startIndex, stopIndex);
	while(startIndex < stopIndex)
	{
		if (found==TYPE_STOP)
		{
			// Send an explicit end of POST, before reading anything
			//PRINT("[getNextPOST] Sending explicit 0\n");
			found = TYPE_EMPTY;
			return 0;
		}
		
		nbBytesRead = emma_resource_read(agent, block, blockSize, startIndex);
		leftIndex = 0;
		
		for(cnt=0 ; cnt<nbBytesRead ; cnt++)
		{
			//PRINTS("%c", block[cnt]);
			// Apply JSON parser
			jsonState = JSONnParse(&jsonParser, block[cnt], 0);
			
			if (!found)
			{
				if ((jsonState == JSON_OBJ_START) || (jsonState == JSON_STR_START))
				{
					//PRINTS("leftIndex=%d\n", leftIndex);
					if (jsonState == JSON_OBJ_START) found = TYPE_OBJECT;
					else found = TYPE_EXPRESSION;
					leftIndex++;
					
					if (leftIndex)
					{
						// Shift left the block
						for(i=leftIndex ; i<blockSize ; i++) block[i-leftIndex] = block[i];
					
						// Complete block
						nbBytesRead += emma_resource_read(agent, block+(blockSize-leftIndex), leftIndex, startIndex+nbBytesRead);
					
						// Adjust counters
						startIndex+=leftIndex;
						nbBytesRead-=leftIndex;
						cnt-=leftIndex; 
					}
				}
				else leftIndex++;
			}
			
			if ((found==TYPE_OBJECT) || (found==TYPE_EXPRESSION))
			{
				if ((jsonState == JSON_OBJ_END) || (jsonState == JSON_STR_END))
				{
					if (type) *type = found;
					found = TYPE_EMPTY;
					startIndex += cnt;
					if (cnt == nbBytesRead-1) found = TYPE_STOP;
					else found = TYPE_EMPTY;
					startIndex++;
					//PRINT("[getNextPOST] Returning end of POST ; cnt=%d ; nbBytesRead=%d\n", cnt, nbBytesRead);
					return cnt; // cnt+1;
				}
			}
		}
		
		//PRINTS("BLOCK#%d: type=%d\n", startIndex/blockSize, *type);

		startIndex += nbBytesRead;
		if ((found==TYPE_OBJECT) || (found==TYPE_EXPRESSION))
		{
			//PRINT("[getNextPOST] Returning block\n");
			if (type) *type = found;
			return cnt;
		}
		if (nbBytesRead < blockSize) break;
	}
	
	//PRINT("[getNextPOST] Default return\n");
	return 0;
}

/* TODO */
#define EMMA_INT_SIZE	9
eval_status_t client_solver (uint8_t* reference, uint8_t referenceSize, operand_t* value)
{
	char buffer[EMMA_MAX_URI_SIZE];
	char val[EMMA_INT_SIZE];
	uint8_t cnt = 0;
	int nbBytesRead = 0;
	if (((reference != NULL) && (referenceSize != 0)) && (value != NULL))
	{
		snprintf(buffer, MIN(referenceSize + 1, EMMA_MAX_URI_SIZE), "%s", (char*)reference);
		while(cnt < MIN(referenceSize + 1, EMMA_MAX_URI_SIZE)) {if(buffer[cnt]=='#')buffer[cnt]='/';cnt++;}
		PRINTS("[REFERENCE SOLVER 2] Solving reference of '%s'\n", buffer);	
		nbBytesRead = emma_resource_read(buffer, (uint8_t*)val, EMMA_INT_SIZE, 0);
		for (cnt=0 ; cnt<nbBytesRead ; cnt++) PRINTS("%c-", (char)(val[cnt]));
		
		if (nbBytesRead == 0) {return EVAL_BAD_REFERENCE;}
		else
		{
			*value = (operand_t)antoi(val, nbBytesRead);
			return EVAL_COMPLETE;
		}
		return EVAL_BAD_REFERENCE;
	}
	else return EVAL_INVALID_ARG;
}

// Pre-evaluate blocks
#define REPL_BUFFER 16
#define REPL_EOB '"'
// End Of Buffer character

//typedef uint8_t erd_index_t;


//*

typedef enum {
	REPL_RAZ,
	REPL_SHIFT_BUFFER,
	REPL_FILL_BUFFER,
	REPL_ANALYZE,
	REPL_COPY_BUFFER,
	REPL_COPY_OPERAND,
	REPL_ERROR,
	REPL_END
} repl_state_t;

void printState(repl_state_t state)
{
	if (state == REPL_RAZ) 											{PRINT("[PRINT STATE] RAZ\n");}
	else if (state == REPL_FILL_BUFFER) 				{PRINT("[PRINT STATE] FILL BUFFER\n");}
	else if (state == REPL_SHIFT_BUFFER) 				{PRINT("[PRINT STATE] SHIFT BUFFER\n");}
	else if (state == REPL_ANALYZE) 			{PRINT("[PRINT STATE] ANALYZE\n");}
	else if (state == REPL_COPY_BUFFER) 				{PRINT("[PRINT STATE] COPY BUFFER\n");}
	else if (state == REPL_COPY_OPERAND) 				{PRINT("[PRINT STATE] COPY OPERAND\n");}
	else if (state == REPL_ERROR) 							{PRINT("[PRINT STATE] ERROR\n");}
	else if (state == REPL_END)									{PRINT("[PRINT STATE] END\n");}
	else 																				{PRINT("[PRINT STATE] Unknown\n");}
}


uint8_t pre_evaluate (char* name, emma_index_t* block, uint8_t* output, uint8_t outputSize, repl_solver solver, repl_getter getter)
{
	// Variables
	static char* nam = NULL;							// Name of initial resource
	static emma_index_t namIndex=0;						// Read index of name resource
	static char ref[EMMA_MAX_URI_SIZE+1];							// Pointer on reference resource
	static emma_index_t refIndex=0;						// Read index of reference resource
	
	static repl_state_t currentState = REPL_RAZ;
	static uint8_t buffer[REPL_BUFFER+1];	// Evaluated expression buffer
	static uint8_t bufferIndex = 0;
	static uint8_t copyIndex = 0;					// Index used for copy
	
	uint8_t outputCond = 0;								// Output condition boolean
	uint8_t outputIndex = 0;							// Index on output buffer
	
	int nbBytesRead = 0;
	uint8_t cnt = 0;
	char tempC;
	//resource_t* res = NULL;
	
	if (! block){PRINT("No index.\n"); return 0;}
	
	
	// Init FSA
	if (name != NULL)
	{
		currentState = REPL_RAZ;
		nam = name;
		namIndex = *block;
	}
	
	// Finite State Machine run
	while (! outputCond)
	{
		//PRINT("\n");printState(currentState);
		if (currentState == REPL_RAZ)
		{
			// Reset all static variables
			currentState = REPL_FILL_BUFFER;
			ref[0] = '\0';
			refIndex = 0;
			bufferIndex = 0;
			buffer[REPL_BUFFER] = '\0';
			copyIndex = 0;
		}
		
		// SHIFT BUFFER
		else if (currentState == REPL_SHIFT_BUFFER)
		{
			if ((strlen((char*)buffer) < REPL_BUFFER) && (buffer[bufferIndex] == '\0'))
			{
				currentState=REPL_END;
				outputCond=1;
			}
			else
			{
				if (strlen((char*)buffer) >= REPL_BUFFER) currentState = REPL_FILL_BUFFER;
				else currentState = REPL_ANALYZE;
				
				cnt=bufferIndex;
				while (buffer[cnt] != '\0')
				{
					buffer[cnt-bufferIndex]=buffer[cnt];
					cnt++;
				}
				buffer[cnt-bufferIndex]=buffer[cnt];
				if (currentState == REPL_FILL_BUFFER) bufferIndex = cnt-bufferIndex;
				else bufferIndex = 0;
				//PRINT("Buffer: %s\n", (char*)buffer);
			}
		}
		
		// FILL BUFFER
		else if (currentState == REPL_FILL_BUFFER)
		{
			if (bufferIndex < REPL_BUFFER)
			{
				// Fill buffer with initial resource
				nbBytesRead = getter(nam, buffer+bufferIndex, REPL_BUFFER-bufferIndex, namIndex);
				namIndex += nbBytesRead;
				*block = namIndex;
				//PRINT("nbBytesRead: %d\n", nbBytesRead);
				if (bufferIndex+nbBytesRead < REPL_BUFFER-1) buffer[bufferIndex+nbBytesRead+1] = '\0';
				//PRINT("Buffer: %s\n", (char*)buffer);
			
				// Search end of block character
				cnt = 0;
				while ((cnt < REPL_BUFFER) && (buffer[cnt] != REPL_EOB)) cnt++;
				if (buffer[cnt] == REPL_EOB)
				{
					buffer[cnt] = '\0';
					*block -= (nbBytesRead - cnt);
					*block+=1;
					//PRINT("EOB FOUND ; readIndex=%d\n", *block);
				}
				
				
				bufferIndex = 0;
				currentState = REPL_ANALYZE;
			}
			else currentState = REPL_ERROR;
		}
		
		// ANALYZE
		else if (currentState == REPL_ANALYZE)
		{
			find_operator(buffer, strlen((char*)buffer), &bufferIndex);
			tempC = buffer[bufferIndex];
			buffer[bufferIndex] = '\0';
			cnt=0;while(buffer[cnt]!='\0' && cnt<EMMA_MAX_URI_SIZE) // Invert # with / 
			{
				ref[cnt] = buffer[cnt];
				if(ref[cnt]=='#')ref[cnt]='/';
				cnt++;
			}
			ref[cnt] = '\0';
			buffer[bufferIndex] = tempC;
			if (solver(ref)) {copyIndex = bufferIndex;currentState = REPL_COPY_OPERAND;}
			else {copyIndex = 0;currentState = REPL_COPY_BUFFER;}
		}
		
		// COPY BUFFER
		else if (currentState == REPL_COPY_BUFFER)
		{
			cnt=copyIndex;
			while ((cnt<=bufferIndex) && (outputIndex<outputSize) && (buffer[cnt] != '\0'))
			{
				//PRINT("%c-", buffer[cnt]);
				output[outputIndex] = buffer[cnt];
				cnt++;
				outputIndex++;
			}
			copyIndex = cnt;
			if (outputIndex >= outputSize) {outputCond=1;}
			else
			{
				if (buffer[cnt] != '\0') bufferIndex++;
				currentState = REPL_SHIFT_BUFFER;
			}
		}
		
		// COPY OPERAND
		else if (currentState == REPL_COPY_OPERAND)
		{
			if (solver(ref)) nbBytesRead = getter(ref, output+outputIndex, outputSize-outputIndex, refIndex);
			else nbBytesRead = getter(NULL, output+outputIndex, outputSize-outputIndex, refIndex);
			refIndex += nbBytesRead;
			outputIndex += nbBytesRead;
			
			// Search end of block character
			cnt = 0;
			while ((cnt < outputIndex) && (output[cnt] != REPL_EOB)) cnt++;
			if (output[cnt] == REPL_EOB) outputIndex = cnt;
			
			if (outputIndex == outputSize) {outputCond = 1;}
			else
			{
				refIndex = 0;
				currentState = REPL_COPY_BUFFER;
			}
		}
		
		// ERROR
		else if (currentState == REPL_ERROR)
		{
			// Print all variables to see DEBUG
			//PRINT("\n\n");
			//PRINT("[REPLACER] nam: %s ; namIndex=%d\n", nam, namIndex);
			//PRINT("[REPLACER] buffer: %s ; bufferIndex=%d\n", buffer, bufferIndex);
			//PRINT("[REPLACER] ref: %s ; refIndex=%d\n", (char*)ref, refIndex); // XXX: Not a char* in EMMA
			//PRINT("[REPLACER] copyIndex=%d\n", copyIndex);
			//PRINT("[REPLACER] outputIndex=%d\n", outputIndex);
			outputIndex = 0;
			outputCond = 1;
			currentState = REPL_END;
		}
		
		// END
		else if (currentState == REPL_END)
		{
			outputCond=1;
			outputIndex = 0;
			//PRINT("END OF REPLACER ; readIndex=%d\n", *block);
		}
		else PRINT("[REPLACER][ERROR] Unknown state !\n");
	}
	return outputIndex;
}//*/
