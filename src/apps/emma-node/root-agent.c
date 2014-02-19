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
 
#include "root-agent.h"
#include "memb.h"

#include "emma-JSONnParse.h"
#include "cfs/cfs.h"
#include "cfs/cfs-coffee.h"
#include <string.h>

#define LDEBUG 1
#if (LDEBUG | GDEBUG)
	#define PRINT(...) OUTPUT_METHOD("[ROOT AGENT] " __VA_ARGS__)
	#define PRINTS(...) OUTPUT_METHOD(__VA_ARGS__)
#else
	#define PRINT(...)
	#define PRINTS(...)
#endif

/*typedef struct rootAgent {
	int value;
} root_agent_t;*/

#define SIZEOF_STATIC(table) (sizeof((table)) / sizeof((table)[0]))

// Do not use empty strings => (security hole if hacker send a '\0')
#define PRE_PATTERN			0
#define POST_PATTERN		1
#define TARGET_PATTERN	2
#define NAME_PATTERN		3
#define NB_PATTERNS			4
static char* agent_patterns[] = {
	"\"PRE\":\"",
	"\"POST\":[",
	"\"TARGET\":[",
	"\"NAME\":\""
};

#define MAX_PATTERNS SIZEOF_STATIC(agent_patterns)

typedef struct rootAgent {
	uint8_t memoryIndex;
	
	uint8_t patternIndex[MAX_PATTERNS];
	emma_index_t fileIndex[MAX_PATTERNS];
	uint16_t size;
	
	json_parser_t parser;
} root_agent_t;

#define ROOT_AGENT_MAX_ALLOC 5
MEMB(root_agent_R, root_agent_t, ROOT_AGENT_MAX_ALLOC);
#define ROOT_MEM_INIT() memb_init(&root_agent_R)
#define ROOT_FREE(r) memb_free(&root_agent_R, r)
#define ROOT_ALLOC(r) memb_alloc(&root_agent_R)

static uint8_t memory[ROOT_AGENT_MAX_ALLOC];
static uint8_t getMemoryIndex()
{
	uint8_t cnt=0;
	while((memory[cnt] != 0) && (cnt<ROOT_AGENT_MAX_ALLOC)) cnt++;
	if (cnt >= ROOT_AGENT_MAX_ALLOC) PRINT("[ERROR] BAD filesystem management (memory leak).\n");
	memory[cnt]=1;
	return cnt;
}

static char* getFileName(uint8_t memoryIndex)
{
	static char fileName[] = "ROOTAGENT000";
	if (memoryIndex >= ROOT_AGENT_MAX_ALLOC) {return NULL;}
	else
	{
		sprintf(fileName, "ROOTAGENT%d", memoryIndex);
		return fileName;
	}
}

void root_agent_init()
{
	ROOT_MEM_INIT();
	memset(memory, 0, ROOT_AGENT_MAX_ALLOC);
	//cfs_coffee_format();
}

static void* root_agent_alloc()
{
	root_agent_t* agent = ROOT_ALLOC(0);
	if (agent)
	{
		agent->memoryIndex = getMemoryIndex();
		//PRINT("[ALLOC] index=%d\n", agent->memoryIndex);
		if (agent->memoryIndex >= ROOT_AGENT_MAX_ALLOC)
		{
			ROOT_FREE(agent);
			agent = NULL;
		}
	}
	return agent;
}
void root_agent_free(char* uri, void* data) {ROOT_FREE(data);memory[((root_agent_t*)data)->memoryIndex]=0;}

void root_agent_reset(char* uri, void* data)
{
	// Never set or reset memoryIndex !!! (already managed in alloc / free)
	memset(((root_agent_t*)data)->patternIndex, 0, MAX_PATTERNS);
	memset(((root_agent_t*)data)->fileIndex, 0, MAX_PATTERNS*sizeof(emma_index_t));
	((root_agent_t*)data)->size = 0;
	JSONnParse_clean(&(((root_agent_t*)data)->parser));
}

static uint8_t root_agent_analyze_block(char* uri, root_agent_t* agent, uint8_t* data_block, emma_size_t block_size, emma_index_t block_index)
{
	int cnt;
	uint8_t patternCnt;
	json_state_t jsonState = JSON_ERROR;
	
	// Verify syntax
	
	// Init the agent if first block
	if (block_index == 0)
	{
		root_agent_reset(uri, agent);
		JSONnParse_clean(&(agent->parser));
	}
	
	// TODO XXX:
	// Compare block_index with agent->size
	// If the block is being re-send, clean parser and parse from 0 to blockIndex
	
	// Loop on each character and try to find patterns
	for (cnt=0 ; cnt < block_size ; cnt++)
	{
		jsonState = JSONnParse(&(agent->parser), data_block[cnt], 1);
		
		for (patternCnt=0 ; patternCnt < SIZEOF_STATIC(agent_patterns) ; patternCnt++)
		{
			if (data_block[cnt] == agent_patterns[patternCnt][agent->patternIndex[patternCnt]]
//				&& !(patternCnt == 0 && strlen(agent->patternIndex) > 0) 
				) 
				agent->patternIndex[patternCnt]++;
			else agent->patternIndex[patternCnt] = 0;
			if (agent_patterns[patternCnt][agent->patternIndex[patternCnt]] == '\0')
			{
				// We got a pattern matching !
				if (jsonState != JSON_CONTENT)
				{
					if (agent->fileIndex[patternCnt]==0)
					{
						PRINT("[ANALYZE] Matching - %s - at %d.\n", agent_patterns[patternCnt], agent->size+cnt);
						PRINT("agent->size=%d ; cnt=%d\n", agent->size, cnt);
						agent->fileIndex[patternCnt] = agent->size + cnt;
					}
					else
					{
						PRINT("[ANALYZE] Twice Matching - %s - at %d.\n", agent_patterns[patternCnt], agent->size+cnt);
						return 1;
					} // Not the first time we match this pattern !
					//lastMatched = patternCnt;
				}
				//else return 0; // Not the first time we match this pattern !
			}
		}
		//agent->size++;
	}
	return 0;
}

int root_agent_write(char* uri, void* user_data, uint8_t* data_block, emma_size_t block_size, emma_index_t block_index)
{
	// Variables
	root_agent_t* agent = (root_agent_t*)user_data;
	int nbBytesWritten = 0;
	cfs_offset_t offset = 0;
	
	//printf("ICI0 %d,%d"block_index,agent->size);
	//if (block_index < agent->size) return 0; // Do not write multiple times the same block !!
	
	// Verify block
	if (root_agent_analyze_block(uri, agent, data_block, block_size, block_index) != 0)
	{
		PRINT("[WRITE] Invalid block at index #%d\n", block_index);
		return 0;
	}

	// Write block on EEPROM
	if(agent)
	{
		// Agent is active, try to set data block
		int fd = cfs_open(getFileName(agent->memoryIndex), CFS_WRITE);
		if (fd >= 0)
		{
			//PRINTF("[AGENT SET DATA] Opening file '%s'\n", agent->name);
			// Seek to the right amount of blocks
			offset = cfs_seek(fd, block_index, CFS_SEEK_SET);
			
			// Write data
			if (offset >= 0)
			{
				nbBytesWritten = cfs_write(fd, data_block, block_size);
			}
			else
			{
				PRINT("[WRITE] Unable to seek file\n");
			}
			PRINT("[WRITE] Writing '%s' at %d.\n", getFileName(agent->memoryIndex), offset);
			
			// Close file
			cfs_close(fd);
			//PRINTF("[AGENT SET DATA] '%s'closed\n", agent->name);
		}
		else
		{
			PRINT("[WRITE] Unable to open file '%s'.\n", getFileName(agent->memoryIndex));
			nbBytesWritten = 0;
		}
	}
	else
	{
		PRINT("[WRITE] Empty agent.\n");
		nbBytesWritten = 0;
	}
	agent->size += nbBytesWritten;
	return nbBytesWritten;
}

int root_agent_read(char* uri, void* user_data, uint8_t* data_block, emma_size_t block_size, emma_index_t block_index)
{
	root_agent_t* agent = (root_agent_t*)user_data;
	int nbBytesRead = 0;
	cfs_offset_t offset = 0;
	if(agent != NULL)
	{
		// Agent is active, try to get data block
		int fd = cfs_open(getFileName(agent->memoryIndex), CFS_READ);
		//PRINT("[READ] Opening file '%s'\n", getFileName(agent->memoryIndex));
		if (fd >= 0)
		{
			// Seek to the right amount of blocks
			offset = cfs_seek(fd, block_index, CFS_SEEK_SET);
			
			// Read data
			if (offset >= 0)
			{
				nbBytesRead = cfs_read(fd, data_block, block_size);
			}
			else
			{
				PRINT("[READ] Unable to seek file\n");
			}
			PRINT("[READ] Reading '%s' at %d.\n", getFileName(agent->memoryIndex), offset);
			
			// Close file
			cfs_close(fd);
			//PRINT("[READ] '%s'closed\n", getFileName(agent->memoryIndex));
		}
		else
		{
			PRINT("[READ] Unable to open file '%s'.\n", getFileName(agent->memoryIndex));
			nbBytesRead = 0;
		}
	}
	else
	{
		PRINT("[READ] Empty agent\n");
		nbBytesRead = 0;
	}
	return nbBytesRead;
}

uint8_t root_agent_regex(char* uri, void* user_data, char* pattern, emma_index_t* start, emma_index_t* stop)
{
	//PRINT("[REGEX] Not implemented\n");
	// Compute stopIndex, startIndex is already saved in agent structure
	// Manage case where a pattern has not been found during write of agent
	// Variables
	root_agent_t* agent = (root_agent_t*)user_data;
	emma_index_t startIndex = 0;
	emma_index_t stopIndex = agent->size;
	emma_index_t temp = 0;
	uint8_t cnt = 0;
	if (!agent || !pattern) return 0;
	if (strncmp(pattern, "$PRE", 4) == 0)
	{
		startIndex = agent->fileIndex[PRE_PATTERN];
	}
	else if (strncmp(pattern, "$POST", 5) == 0)
	{
		startIndex = agent->fileIndex[POST_PATTERN];
	}
	else if (strncmp(pattern, "$TARGET", 7) == 0)
	{
		startIndex = agent->fileIndex[TARGET_PATTERN];
	}
	else
	{
		PRINT("[REGEX] Pattern not found\n");
		if (start) *start = 0;
		if (stop) *stop = 0;
		return 0;
	}
	
	for(cnt=0 ; cnt<MAX_PATTERNS ; cnt++)
	{
		temp = agent->fileIndex[cnt] - strlen(agent_patterns[cnt]);
		if (temp > startIndex)
		{
			if (temp < stopIndex)
			{
				stopIndex = temp;
			}
		}
	}
	
	if (start) *start = startIndex;
	if (stop) *stop = stopIndex;
	return 1;
}

emma_resource_root_t root_agent = {
	0,
	"A",
	"ct=\"text/json\";rt=\"Agent\"",
	//"",
	
	root_agent_init,
	root_agent_alloc,
	root_agent_free,
	root_agent_reset,
	NULL,
	root_agent_read,
	root_agent_write,
	NULL,
	root_agent_regex,

	NULL
};
