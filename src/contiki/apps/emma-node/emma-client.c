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

PROCESS(emma_client_process, "EMMA client process");

enum {
	EMMA_CLIENT_WAITING_MUTEX,
	EMMA_CLIENT_RUNNING,
	EMMA_CLIENT_IDLE,
	EMMA_CLIENT_START,
	EMMA_CLIENT_RELEASE
};

// Resource management
static uint8_t state = 0;
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
uint8_t code = 0;
uint16_t POSTindex = 0;
uint8_t POSTtype = 0;
uint16_t POSTblockcnt = 0;
static coap_packet_t request[1];
static uint8_t sendOK = 0;

// Helper functions
eval_status_t client_solver (uint8_t* reference, uint8_t referenceSize, operand_t* value);
void emma_client_chunk_callback(void *response);
uint8_t getNextTARGET(char* resource, uip_ipaddr_t* address, uint16_t* port, char uri[EMMA_MAX_URI_SIZE]);
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
	
	while(1)
	{
		if ((emma_resource_get_last_modified(NULL) != EMMA_CLIENT_ID) && (state == EMMA_CLIENT_IDLE))
		{
			state = EMMA_CLIENT_START;
		}
		// XXX HACK:
		//state = EMMA_CLIENT_IDLE;
	
		if (state != EMMA_CLIENT_IDLE)
		{
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
						code = getNextTARGET(resource, &TARGETadd, &TARGETport, TARGETuri);
						POSTindex = getNextPOST(resource, buffer, EMMA_CLIENT_BUFFER_SIZE, &POSTtype);
						//PRINT("First call\n");
						//PRINT("[RUN] POSTx (%d): '", POSTtype);for(cnt=0 ; cnt<POSTindex ; cnt++) PRINTS("%c", buffer[cnt]);PRINTS("'\n");
						
						while (code)
						{
							//if (code == 1)
							{
								// Address found, send POST
								PRINT("[RUN] Address:  "); PRINT6ADDR(TARGETadd.u8); PRINTS("\n");
								PRINT("[RUN] Uri: '%s'\n", TARGETuri);
								coap_init_message(request, COAP_TYPE_CON, COAP_PUT, 0);
								coap_set_header_uri_path(request, TARGETuri);
								POSTblockcnt = 0;
								
								PRINT("[RUN] POST (%d): '", POSTtype);
								// Print POST
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
								
								//PRINT("[RUN] POST1 (%d): '", POSTtype);PRINTS("'\n");
								
								POSTindex = getNextPOST(NULL, buffer, EMMA_CLIENT_BUFFER_SIZE, &POSTtype);
								//PRINT("[RUN] POST2 (%d): '", POSTtype);for(cnt=0 ; cnt<POSTindex ; cnt++) PRINTS("%c", buffer[cnt]);PRINTS("'\n");
							}
							//else if (code == 2) PRINT("[RUN] Uri too long: '%s'\n", TARGETuri);
							code = getNextTARGET(NULL, &TARGETadd, &TARGETport, TARGETuri);
						}
						
						/*if (emma_resource_get_index_of_pattern(resource, "$POST", &startIndex, &stopIndex))
						{
							PRINT("[RUN] startIndex=%d ; stopIndex=%d\n", startIndex, stopIndex);
							PRINT("POST='");
							while(startIndex < stopIndex)
							{
								nbBytesRead = emma_resource_read(resource, buffer, EMMA_CLIENT_BUFFER_SIZE, startIndex);
								for(cnt=0 ; cnt<nbBytesRead ; cnt++) PRINTS("%c", buffer[cnt]);
								startIndex += nbBytesRead;
								if (!nbBytesRead) break;
							}
							PRINTS("'\n");
						}*/
						
						//uint16_t getNextPOST(char* resource, uint8_t* block, uint16_t blockSize, uint8_t* type);
						
					}
					
					/* Finalize */
					nbBytesRead = get_next_resource_name_by_root(0, (uint8_t*)resource, EMMA_MAX_URI_SIZE);
				}
				
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
		}
	
		PROCESS_PAUSE(); // Or Use a timer ...
	}
	
	PROCESS_END();
}

//-------------------------------------------------------------------------
// Helper functions

void emma_client_chunk_callback(void *response)
{
  // TODO: Analyze response
  sendOK = 1;
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
uint8_t getNextTARGET(char* resource, uip_ipaddr_t* address, uint16_t* port, char uri[EMMA_MAX_URI_SIZE+1])
{
	static char* agent = NULL;
	static emma_index_t startIndex = 0;
	static emma_index_t stopIndex = 0;
	emma_size_t nbBytesRead = 0;
	emma_size_t cnt = 0;
	uint8_t found = 0;
	uint8_t uriIndex = 0;
	uint8_t block[BLOCK_SIZE];
	
	if (resource)
	{
		agent = resource;
		startIndex = 0;
		stopIndex = 0;
		if (!emma_resource_get_index_of_pattern(agent, "$TARGET", &startIndex, &stopIndex)) return 0;
		found = 0;
		uriIndex = 0;
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
			if (!found && (getIPaddress (block[cnt], address, port) == EMMA_EVAL_END)) found = 1;
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


/*

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
	resource_t* res = NULL;
	
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
		PRINT("\n");printState(currentState);
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
				PRINT("Buffer: %s\n", (char*)buffer);
			}
		}
		
		// FILL BUFFER
		else if (currentState == REPL_FILL_BUFFER)
		{
			if (bufferIndex < REPL_BUFFER)
			{
				// Fill buffer with initial resource
				//res = solver(nam);
				if (res) nbBytesRead = getter(nam, buffer+bufferIndex, REPL_BUFFER-bufferIndex, namIndex);
				else nbBytesRead = getter(NULL, buffer+bufferIndex, REPL_BUFFER-bufferIndex, namIndex);
				namIndex += nbBytesRead;
				*block = namIndex;
				if (bufferIndex+nbBytesRead < REPL_BUFFER-1) buffer[bufferIndex+nbBytesRead+1] = '\0';
				PRINT("Buffer: %s\n", (char*)buffer);
			
				// Search end of block character
				cnt = 0;
				while ((cnt < REPL_BUFFER) && (buffer[cnt] != REPL_EOB)) cnt++;
				if (buffer[cnt] == REPL_EOB)
				{
					buffer[cnt] = '\0';
					*block -= (nbBytesRead - cnt);
					*block+=1;
					PRINT("EOB FOUND ; readIndex=%d\n", *block);
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
				//else if(buffer[cnt]=='/'){buffer[cnt]='#';cnt++;}
			}
			ref[cnt] = '\0';
			//ref = solver((char*)(buffer));
			//strncpy();
			//cnt=0;while(buffer[cnt]!='\0') // Invert # with / back
			//{
			//	if(buffer[cnt]=='/')buffer[cnt]='#';
			//	cnt++;
			//	//else if(buffer[cnt]=='#'){buffer[cnt]='/';cnt++;}
			//}
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
			PRINT("END OF REPLACER ; readIndex=%d\n", *block);
		}
		else PRINT("[REPLACER][ERROR] Unknown state !\n");
	}
	return outputIndex;
}//*/
