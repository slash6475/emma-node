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
 
#include "root-template.h"
#include "memb.h"

#define LDEBUG 1
#if (LDEBUG | GDEBUG)
	#define PRINT(...) OUTPUT_METHOD("[ROOT TEMPLATE] " __VA_ARGS__)
	#define PRINTS(...) OUTPUT_METHOD(__VA_ARGS__)
#else
	#define PRINT(...)
	#define PRINTS(...)
#endif

typedef struct rootAgent {
	int value;
} root_template_t;

#define ROOT_TEMPLATE_MAX_ALLOC 5
MEMB(root_template_R, root_template_t, ROOT_TEMPLATE_MAX_ALLOC);
#define ROOT_MEM_INIT() memb_init(&root_template_R)
#define ROOT_FREE(r) memb_free(&root_template_R, r)
#define ROOT_ALLOC(r) memb_alloc(&root_template_R)

void root_template_init() {ROOT_MEM_INIT();}
static void* root_template_alloc() {return ROOT_ALLOC(0);}
void root_template_free(char* uri, void* data) {ROOT_FREE(data);}

void root_template_reset(void* data)
{
	((root_template_t*)data)->value = 0;
}

int root_template_open(char* uri, void* data)
{
	PRINT("[OPEN] Not implemented\n");
	return 0;
}

int root_template_write(char* uri, void* user_data, uint8_t* data_block, emma_size_t block_size, emma_index_t block_index)
{
	PRINT("[WRITE] Not implemented\n");
	return 0;
}

int root_template_read(char* uri, void* user_data, uint8_t* data_block, emma_size_t block_size, emma_index_t block_index)
{
	PRINT("[READ] Not implemented\n");
	return 0;
}

int root_template_close(char* uri, void* data)
{
	PRINT("[CLOSE] Not implemented\n");
	return 0;
}

uint8_t root_template_regex(char* uri, void* user_data, char* pattern, emma_index_t* start, emma_index_t* stop)
{
	PRINT("[REGEX] Not implemented\n");
	return 0;
}

emma_resource_root_t root_template = {
	0,
	"T",
	"ct=\"text/raw\";rt=\"Template\"",
	
	root_template_init,
	root_template_alloc,
	root_template_free,
	root_template_reset,
	root_template_open,
	root_template_read,
	root_template_write,
	root_template_close,
	root_template_regex,

	NULL
};
