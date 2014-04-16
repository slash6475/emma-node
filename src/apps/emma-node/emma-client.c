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

#define EMMA_CLIENT_BUFFER_SIZE	REST_MAX_CHUNK_SIZE
#define CLIENT_GET		1
#define CLIENT_PUT		2
#define CLIENT_POST		3
#define CLIENT_DELETE	4


PROCESS(emma_client_process, "EMMA client process");

// Resource management
static struct etimer emma_client_timer;
static uint8_t state = EMMA_CLIENT_IDLE;
static char resource[EMMA_MAX_URI_SIZE+1];
static emma_size_t nbBytesRead = 0;
static emma_resource_root_id_t agentsRoot = 0;
static emma_index_t startIndex = 0;
static emma_index_t stopIndex = 0;

extern uint16_t node_id;

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
//uint16_t POSTblockcnt_begin = 0;

uip_ipaddr_t Broadcastaddr;

static coap_packet_t request[1];
static uint8_t sendOK = 0;

// Helper functions
uint8_t isLocalAddress(uip_ipaddr_t* address);
eval_status_t client_solver (uint8_t* reference, uint8_t referenceSize, operand_t* value);
void emma_client_chunk_callback(void *response);
uint8_t getNextTARGET(char* resource, uint8_t* method, uip_ipaddr_t* address, uint16_t* port, char uri[EMMA_MAX_URI_SIZE]);
uint16_t getNextPOST(char* resource, uint8_t* block, uint16_t blockSize, uint8_t* type);
static uint8_t addressCMP(uip_ipaddr_t* address1, uip_ipaddr_t* address2);

// Function pointer to return boolean test resource existence on node (identified by name) (use 'emma_resource_read')
typedef uint8_t (* repl_solver) (char* symbol);

// Function pointer to read a resource on node (identified by name) (use 'emma_resource_read')
typedef int (* repl_getter) (char* name, uint8_t* data_block, uint16_t block_size, emma_index_t block_index);
uint8_t preprocessor (char* name, emma_index_t* block, uint8_t* output, uint8_t outputSize, repl_solver solver, repl_getter getter);

void emma_client_init()
{
	PRINT("Starting EMMA client\n");
	state = EMMA_CLIENT_IDLE;
	set_reference_solver(&client_stack, client_solver);
	process_start(&emma_client_process, NULL);
}

uint8_t emma_client_state(){
	return state;
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
				PRINT("Waking up !\n");
				state = EMMA_CLIENT_START;
				previousFail = 0;
				process_post(PROCESS_CURRENT(), PROCESS_EVENT_CONTINUE, NULL);
			}
			else
			{
				PRINT("Sleeping for %d seconds ...\n", EMMA_CLIENT_POLLING_INTERVAL);
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
								PRINT("Already locked %s\n", (char*)resource);
								break;
							}
						}
						else 
						{
							// If a resource cannot be locked, some process is working, we let him to finish
							nbBytesRead = 0;
							emma_resource_set_last_modified(resource, EMMA_CLIENT_ID);
						}
					}
					if(nbBytesRead)
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
						//PRINT("1\n");
						code = getNextTARGET(resource, &TARGETmethod, &TARGETadd, &TARGETport, TARGETuri);
						//PRINT("2\n");
						if (code) code = emma_resource_get_index_of_pattern(resource, "$POST", &POSTstartIndex, &POSTstopIndex);
						//PRINT("3\n");
						POSTstartIndex++; // Skip starting '['
					
						while (code && pre)
						{
printf("AGENT %s starting\n", resource);
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
										if (POSTblockNum == 0){POSTstartIndex++; POSTnbBytesRead = preprocessor (resource, &POSTstartIndex, POSTblock, EMMA_CLIENT_BUFFER_SIZE, emma_resource_exists, emma_resource_read);}
										else {POSTnbBytesRead = preprocessor (NULL, &POSTstartIndex, POSTblock, EMMA_CLIENT_BUFFER_SIZE, emma_resource_exists, emma_resource_read);}
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
									else if (TARGETmethod == CLIENT_POST) coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
									else if (TARGETmethod == CLIENT_DELETE) coap_init_message(request, COAP_TYPE_CON, COAP_DELETE, 0);
									else coap_init_message(request, COAP_TYPE_CON, COAP_PUT, 0);
									coap_set_header_uri_path(request, TARGETuri);

									if ((POSTnbBytesRead == EMMA_CLIENT_BUFFER_SIZE) && (!POSTlastPacket)) POSTmore = 1;
									else POSTmore = 0;
									
									coap_set_header_block1(request, POSTblockNum, POSTmore, EMMA_CLIENT_BUFFER_SIZE);
									coap_set_payload(request, POSTblock, POSTnbBytesRead);
if(POSTblockNum == 0)
printf("AGENT %s sending request to %s\n", resource, TARGETuri);
if(POSTmore == 0)
printf("AGENT %s sending request end\n", resource);
									// Send packet
									if (isLocalAddress(&TARGETadd))
									{
										PRINT("Local address\n");
										previousFail = 0;

										if( !(emma_resource_exists(TARGETuri) && TARGETmethod == CLIENT_POST && POSTblockNum == 0)){

											// Send block in a local way ...
											if (TARGETmethod == CLIENT_DELETE) {
												emma_resource_del(TARGETuri);
											}
											else if (strncmp(TARGETuri, resource, strlen(TARGETuri)) != 0)
											{
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
											PRINT("Resource is already existing\n");
										}
									}
									else
									{
										PRINT("Network address\n");
										sendOK = 0;

                  // TODO : No ACK If it is a broadcast but it is required when there are several blocks
//                  uip_ip6addr(&Broadcastaddr, 0xff02, 0, 0, 0, 0, 0, 0, 0x0002);
//                  if(!addressCMP(&Broadcastaddr, &TARGETadd))
//									  ((coap_packet_t *)request)->type = COAP_TYPE_NON;
										if(POSTblockNum == 0){
											random_init(clock_seconds()*node_id);
											coap_set_current_mid(random_rand());
										}

										if (pre) COAP_BLOCKING_REQUEST(&TARGETadd, TARGETport, request, emma_client_chunk_callback);
										if (!sendOK && pre)
										{
											PRINT("Chunk #%d not distributed !\n", POSTblockNum);
											//PRINT("TODO: Set timer to retry in a few seconds, trying with next agent ...\n");
											pre = 1;//0;
											previousFail = 0; //1; // Force (<=> even if no resource changed) RUN of agents on next evaluation
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

							TARGETmethod = 0;
							if (! previousFail) code = getNextTARGET(NULL, &TARGETmethod, &TARGETadd, &TARGETport, TARGETuri);
							else code = 0;
							if(code)
								PRINT("Next target found !\n");
						} // while (code && pre)
					} // If not a ROOT resource
				
					/* Finalize (STOP everything if one failed OR if we DELETE an agent)*/
					if (! previousFail) nbBytesRead = get_next_resource_name_by_root(0, (uint8_t*)resource, EMMA_MAX_URI_SIZE);
					else nbBytesRead = 0;
					
					//nbBytesRead = 0;
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
	uint8_t addresscnt = 0;
	for (addresscnt=0 ; addresscnt<16 ; addresscnt++)
	{
		if (address1->u8[addresscnt] > address2->u8[addresscnt]) return 1;
		else if (address1->u8[addresscnt] < address2->u8[addresscnt]) return -1;
	}
	return 0;
}
#endif

uint8_t isLocalAddress(uip_ipaddr_t* address)
{
#if UIP_CONF_IPV6
	uint8_t addresscnt = 0;
	uip_ipaddr_t loopback;

	for (addresscnt=0 ; addresscnt<15 ; addresscnt++)
		loopback.u8[addresscnt] = 0;
    loopback.u8[15] = 1;

	// If it's me
	if(addressCMP(address, &(loopback)) == 0)
		return 1;

	//
	for (addresscnt=0 ; addresscnt<UIP_DS6_ADDR_NB ; addresscnt++)
	{
    if (uip_ds6_if.addr_list[addresscnt].isused && addressCMP(address, &(uip_ds6_if.addr_list[addresscnt].ipaddr)) == 0)
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
		startIndex++; // drop [ of list
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
					
				// Extract method name
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
		while(cnt < MIN(referenceSize + 1, EMMA_MAX_URI_SIZE)) {
			if(buffer[cnt]=='#') buffer[cnt]='/';
			if(buffer[cnt]==' ') buffer[cnt]='\0';
			cnt++;
		}
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
	REPL_EXTRACT_URI,
	REPL_COPY_BUFFER,
	REPL_COPY_OPERAND,
	REPL_ERROR,
	REPL_END
} repl_state_t;

void printState(repl_state_t state)
{
	if (state == REPL_RAZ) 								{PRINT("[PRINT STATE] RAZ\n");}
	else if (state == REPL_FILL_BUFFER) 				{PRINT("[PRINT STATE] FILL BUFFER\n");}
	else if (state == REPL_SHIFT_BUFFER) 				{PRINT("[PRINT STATE] SHIFT BUFFER\n");}
	else if (state == REPL_ANALYZE) 					{PRINT("[PRINT STATE] ANALYZE\n");}
	else if (state == REPL_COPY_BUFFER) 				{PRINT("[PRINT STATE] COPY BUFFER\n");}
	else if (state == REPL_COPY_OPERAND) 				{PRINT("[PRINT STATE] COPY OPERAND\n");}
	else if (state == REPL_ERROR) 						{PRINT("[PRINT STATE] ERROR\n");}
	else if (state == REPL_END)							{PRINT("[PRINT STATE] END\n");}
	else 												{PRINT("[PRINT STATE] Unknown\n");}
}

/* preprocessor function
* This function fill the output buffer according to the resource file name
* It replace resource reference by its file to generate packet payload. 
*
*                         REPL_RAZ
*                             |
*                             v
* |------------------> REPL_FILL_BUFFER   ->   REPL_ERROR
* |                           |
* |                           v
* |------------------->  REPL_ANALYZE
* |                           |
* |          ----------------------------------------
* |          |                |                     |
* |          v                v                     v
* | REPL_COPY_OPERAND -> REPL_COPY_BUFFER <- REPL_EXTRACT_URI
* |                           |
* |        ---------------------------------
* |        |                               |       
* |        v                               v
* --REPL_SHIFT_BUFFER ----------------> REPL_END
*/

char
isDelimiter(char c){
	if(c == ':' || c == '"' || c == ' ' || c == '\'' || c == ',' || c == '{' || c == '}' || c == '[' || c == ']')
		return 1;
	return 0;
}

enum{
	NO_REF,
	REF_BEGIN_CURRENT,
	REF_BEGIN_NEXT,
	REF_END
};

uint8_t preprocessor (char* name, emma_index_t* block, uint8_t* output, uint8_t outputSize, repl_solver solver, repl_getter getter)
{
	// Variables
	static char* nam = NULL;							// Name of initial resource
	static emma_index_t namIndex=0;						// Read index of name resource
	static uint8_t ref[EMMA_MAX_URI_SIZE+1];							// Pointer on reference resource
	static uint8_t var_build = 0;
	static emma_index_t refIndex=0;						// Read index of reference resource
	
	static repl_state_t currentState = REPL_RAZ;
	static uint8_t buffer[REPL_BUFFER+1];	// Evaluated expression buffer
	static uint8_t buffer2[REPL_BUFFER+1];	// Evaluated expression buffer


	static uint8_t bufferIndex = 0;
	static uint8_t copyIndex = 0;					// Index used for copy
	static int8_t  cnt_end_next;
	 

	uint8_t outputCond = 0;								// Output condition boolean
	uint8_t outputIndex = 0;							// Index on output buffer
	
	int nbBytesRead = 0;
	static int8_t cnt_begin = 0, FLAG, pos, buffer_length, i;
	static char tempC;
	char *tmp;

	int8_t sens = 0, cnt_end;
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
			//PRINT("REPL_RAZ \n");
			// Reset all static variables
			currentState = REPL_FILL_BUFFER;
			refIndex = 0;
			bufferIndex = 0;
			buffer[REPL_BUFFER] = '\0';
			buffer2[REPL_BUFFER] = '\0';
			cnt_end_next = 0;
			copyIndex = 0;
			var_build = 0;
			for (cnt=0; cnt < EMMA_MAX_URI_SIZE; cnt ++)
				ref[cnt] = '\0';
		}
		
		/* REPL_SHIFT_BUFFER
		*  Add data from resource file into buffer according o bufferIndex
		*/
		else if (currentState == REPL_SHIFT_BUFFER)
		{
			PRINT("REPL_SHIFT_BUFFER\n");
			if ((strlen((char*)buffer) < REPL_BUFFER) && (buffer[bufferIndex] == '\0'))
			{
				if(strlen((char*)ref) > 0)
					currentState=REPL_ANALYZE;
				else {
					currentState=REPL_END;
					outputCond=1;
					}
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
		
		/* REPL_FILL_BUFFER
		* Copy resource file into free buffer space
		*/
		else if (currentState == REPL_FILL_BUFFER)
		{
			PRINT("REPL_FILL_BUFFER\n");
			if (bufferIndex < REPL_BUFFER)
			{
				// Fill buffer with initial resource file
				nbBytesRead = getter(nam, buffer+bufferIndex, REPL_BUFFER-bufferIndex, namIndex);
				namIndex += nbBytesRead;
				*block = namIndex;
				PRINT("nbBytesRead: %d\n", nbBytesRead);
				if (bufferIndex+nbBytesRead < REPL_BUFFER-1) buffer[bufferIndex+nbBytesRead+1] = '\0';
				PRINT("Buffer: %s\n", (char*)buffer);
			
				// Search end of block character
				cnt = 0;
				while ((cnt < REPL_BUFFER) && (buffer[cnt] != REPL_EOB)) cnt++;
				if (buffer[cnt] == REPL_EOB)
				{
					//buffer[cnt] = '\0';
					*block -= (nbBytesRead - cnt);
					*block+=1;
					PRINT("EOB FOUND ; readIndex=%d\n", *block);
				}

				/******************** CLEAN BUFFER **********************
				* If the new buffer contains the end of previous resource reference
				*/

				/* There is the end of a resource reference in new buffer, we clean it */
				if(cnt_end_next != 0) {
					for(cnt=0; cnt < cnt_end_next; cnt ++)
						if(buffer[cnt] != '\0')
							buffer[cnt] = ' ';
					cnt_end_next = 0;
				}
				/*******************************************************/
				bufferIndex = 0;
				currentState = REPL_ANALYZE;
			}
			else currentState = REPL_ERROR;
		}
		
		/* REPL_ANALYZE
		* Extract the resource reference into ref buffer and replace the resource reference by escapes 
		* If there is a reference, the state REPL_COPY_OPERAND will transmit the resource file
		* Else the buffer is directly send by the state REPL_COPY_BUFFER
		*/
		else if (currentState == REPL_ANALYZE)
		{
			PRINT("REPL_ANALYZE \n");
			if(strlen((char*)ref) > 0){
				currentState = REPL_COPY_OPERAND;
				copyIndex = bufferIndex; 
				for(i=strlen(buffer); i < REPL_BUFFER; i++)
					buffer[i] = ' ';
				buffer[i] = '\0';
			}
			else {
				find_operator(buffer, strlen((char*)buffer), &bufferIndex);
				tempC = buffer[bufferIndex];
				buffer[bufferIndex] = '\0';
				FLAG = NO_REF;

printf("REPL_ANALYZE0: Operator : %c bufferIndex %d buffer %s \n", tempC, bufferIndex, buffer);
				getter(nam, buffer2, REPL_BUFFER, namIndex);
printf("REPL_ANALYZE01: Operator : %c bufferIndex %d buffer %s \n", tempC, bufferIndex, buffer);				
				buffer2[REPL_BUFFER] = '\0';
printf("REPL_ANALYZE1: Operator : %c bufferIndex %d buffer %s \n", tempC, bufferIndex, buffer);

				if(strstr(buffer, "#") != NULL){
					currentState = REPL_EXTRACT_URI; 
					FLAG = REF_BEGIN_CURRENT;
					}

				else if(strstr(buffer2, "#") != NULL){
					for(cnt_begin=0; cnt_begin < REPL_BUFFER; cnt_begin ++)
						if(isDelimiter(buffer2[cnt_begin]))
							break;
						else if (buffer2[cnt_begin] == '#')
							FLAG = REF_BEGIN_NEXT;	

					if(FLAG == REF_BEGIN_NEXT){
						currentState = REPL_EXTRACT_URI; 
					}
					else {
						buffer[bufferIndex] = tempC;
						copyIndex = 0;
						currentState = REPL_COPY_BUFFER;
						}
					}
				else {
					buffer[bufferIndex] = tempC;
					copyIndex = 0;
					currentState = REPL_COPY_BUFFER;
					}	
				}
			}

		else if (currentState == REPL_EXTRACT_URI)
		{
			PRINT("REPL_EXTRACT_URI\n");

			copyIndex = 0;
			currentState = REPL_COPY_BUFFER;

			/************************* FIND REFERENCE ********************************/
			buffer_length = strlen(buffer);

			/* Compute the position of reference delimiter # */
			if(FLAG == REF_BEGIN_CURRENT){            		
				tmp = strstr(buffer, "#");
				if(tmp == NULL){
					PRINT("ERROR UNKNOWN 1\n");
					return -1;
				}
				pos = (char*)tmp - (char*)buffer;
			}
			/* Set the position of delimiter # in the next buffer */
			else if(FLAG == REF_BEGIN_NEXT)	{			
				pos = buffer_length;
				buffer[pos] = tempC;
			}

			/* Find the begining and build resource reference */
			for(cnt_begin=pos; cnt_begin >= 0; cnt_begin--) 			
				if(isDelimiter(buffer[cnt_begin]))  break;

			for(cnt_end=++cnt_begin; cnt_end < buffer_length; cnt_end++){
				if(isDelimiter(buffer[cnt_end])) {
					FLAG = REF_END;
					break;
				}
				ref[cnt_end-cnt_begin] = buffer[cnt_end];			
			}
			ref[cnt_end-cnt_begin] = '\0';

			/* If there is no more packet and the resource reference is at the end */
			if(buffer_length < REPL_BUFFER){
				PRINT("Found end resource due to end buffer\n");
				FLAG = REF_END;
			}

			/* Extract the end of resource reference in next buffer */
			else if(FLAG != REF_END)
			{
				getter(nam, buffer2, REPL_BUFFER, namIndex);
				buffer2[REPL_BUFFER] = '\0';

				PRINT("Looking for end resource reference in next buffer\n");
				for(cnt_end_next=0; cnt_end_next < REPL_BUFFER; cnt_end_next ++){
					if(isDelimiter(buffer2[cnt_end_next])) {
						FLAG = REF_END;
						break;
					}
				ref[cnt_end - cnt_begin + cnt_end_next] = buffer2[cnt_end_next];
				}
			ref[cnt_end - cnt_begin + cnt_end_next] = '\0';
			}

			/************************ PREPARE REFERENCE ********************************/
			for (i = 0; i < strlen(ref); i ++)
				if(ref[i] == '#') ref[i] = '/';
			PRINT("\nURI extraction : %s\n", ref);

			/************************* SOLVE REFERENCE *********************************/
			if (FLAG == REF_END)
			{

				if(solver(ref)){
					PRINT("Resource found %s\n", ref);
					// Replace variable name 
					for (cnt_begin=cnt_begin; cnt_begin < REPL_BUFFER; cnt_begin ++)
						if(buffer[cnt_begin] != '\0')
							buffer[cnt_begin] = ' ';	
					}
				else {
					PRINT("Resource not found %s\n", ref);
					buffer[bufferIndex] = tempC != '\0'? tempC : '\0';
				}

			/* 
				If there is some data after the resource reference,
				we shit the buffer to process it on the next packet
			*/
			if(cnt_end < REPL_BUFFER){
				//namIndex -=  (REPL_BUFFER - cnt_end);	
				namIndex -=  ((tempC == '\0' ? buffer_length : REPL_BUFFER) - cnt_end);
				for(i=cnt_end; i < (tempC == '\0' ? buffer_length : REPL_BUFFER); i++)
					buffer[i] = ' ';
				}
			}
			// Resource not found because : it is incomplete
			else PRINT("ERROR Unable to extract resource reference\n");
			
			/********************** CHECK MULTI REFERENCE ******************************/
		}

		/*
		* COPY BUFFER
		* This function copy directly from the buffer to the ouputput buffer which will be transmited
		*/
		else if (currentState == REPL_COPY_BUFFER)
		{
			PRINT("REPL_COPY_BUFFER (%d): %s\n", copyIndex, buffer);
			cnt=copyIndex;
			
			// Skip copy buffer to output if there is a local variable to replace
			while ((cnt<=bufferIndex) && (outputIndex<outputSize) && (buffer[cnt] != '\0') && (buffer[cnt] != REPL_EOB))
			{
				output[outputIndex] = buffer[cnt];
				if(output[outputIndex] == '\'')
					output[outputIndex] = '"';

				cnt++;
				outputIndex++;
			}
			copyIndex = cnt;
			if (outputIndex >= outputSize || (buffer[cnt] == REPL_EOB)) {
				outputCond=1;
				if (buffer[cnt] == REPL_EOB)
					currentState = REPL_END;
			}
			else
			{
				if (buffer[cnt] != '\0') bufferIndex++;
				if(outputIndex == outputSize) outputCond = 1;
				currentState = REPL_SHIFT_BUFFER;
			}
		}
		
		/*
		* REPL_COPY_OPERAND
		* Replace the resource reference by its file value
		*/

		else if (currentState == REPL_COPY_OPERAND)
		{
			PRINT("REPL_COPY_OPERAND %d, %d, %d '%s): %s\n", refIndex, outputSize, outputIndex, ref, buffer);
			if(outputIndex > 0){
				while(outputIndex < outputSize)
					output[outputIndex++]= ' ';
				output[--outputIndex]= '\0';
			}


			if (solver(ref)) {
				nbBytesRead = getter(ref, output+outputIndex, outputSize-outputIndex, refIndex);
			}
			else {
				nbBytesRead = getter(NULL, output+outputIndex, outputSize-outputIndex, refIndex);
			}
			refIndex += nbBytesRead;
			outputIndex += nbBytesRead;
			
			if (outputIndex == outputSize) {
				outputCond = 1;
			}
			else
			{
				refIndex = 0;
				ref[0] = '\0';
				currentState = REPL_COPY_BUFFER;
			}
		}
		
		// ERROR
		else if (currentState == REPL_ERROR)
		{
			//PRINT("REPL_ERROR \n");
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
			PRINT("REPL_END \n");
			outputCond=1;
			outputIndex = 0;
			//PRINT("END OF REPLACER ; readIndex=%d\n", *block);
		}
		else PRINT("[REPLACER][ERROR] Unknown state !\n");

		// Remove empty block
		if(outputCond){
			outputCond  = 0;
			for(cnt=0; cnt < outputSize; cnt ++)
				if(output[cnt] != ' '){
					outputCond = 1;
					break;
				}
			if(!outputCond)
				outputIndex = 0;
		}
	}

	return outputIndex;
}//*/
