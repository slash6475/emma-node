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
 
#include "root-local.h"
#include "memb.h"

#define LDEBUG 1
#if (LDEBUG | GDEBUG)
	#define PRINT(...) OUTPUT_METHOD("[EMMA RESOURCE TEST] " __VA_ARGS__)
	#define PRINTS(...) OUTPUT_METHOD(__VA_ARGS__)
#else
	#define PRINT(...)
	#define PRINTS(...)
#endif


typedef struct rootLocal {
	int value;
} root_local_t;

#define ROOT_LOCAL_MAX_ALLOC 5
MEMB(root_local_R, root_local_t, ROOT_LOCAL_MAX_ALLOC);
#define ROOT_MEM_INIT() memb_init(&root_local_R)
#define ROOT_FREE(r) memb_free(&root_local_R, r)
#define ROOT_ALLOC(r) memb_alloc(&root_local_R)

void root_local_init() {ROOT_MEM_INIT();}
static void* root_local_alloc() {return ROOT_ALLOC(0);}
void root_local_free(void* data) {ROOT_FREE(data);}

void root_local_reset(void* data)
{
	((root_local_t*)data)->value = 0;
}

int root_local_write(void* user_data, uint8_t* data_block, emma_size_t block_size, emma_index_t block_index)
{
	PRINT("[WRITE] Not implemented\n");
	return 0;
}

int root_local_read(void* user_data, uint8_t* data_block, emma_size_t block_size, emma_index_t block_index)
{
	PRINT("[READ] Not implemented\n");
	return 0;
}

emma_resource_root_t root_local = {
	0,
	"L",
	"ct=\"text/json\";rt=\"Local\"",
	//"",
	
	root_local_init,
	root_local_alloc,
	root_local_free,
	root_local_reset,
	NULL,
	root_local_read,
	root_local_write,
	NULL,
	NULL,

	NULL
};
