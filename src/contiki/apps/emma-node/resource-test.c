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
 
#include "resource-test.h"


#define LDEBUG 1
#if (LDEBUG | GDEBUG)
	#define PRINT(...) OUTPUT_METHOD("[EMMA RESOURCE TEST] " __VA_ARGS__)
	#define PRINTS(...) OUTPUT_METHOD(__VA_ARGS__)
#else
	#define PRINT(...)
	#define PRINTS(...)
#endif

#include "memb.h"
#define EMMA_MAX_TEST_RESOURCES 10
MEMB(emma_test_resources_R, emma_test_resource_t, EMMA_MAX_TEST_RESOURCES);
#define EMMA_RESOURCE_MEM_INIT() memb_init(&emma_test_resources_R)
#define EMMA_RESOURCE_FREE(r) memb_free(&emma_test_resources_R, r)
#define EMMA_RESOURCE_ALLOC(r) memb_alloc(&emma_test_resources_R)

void* emma_test_resource_alloc()
{
	return EMMA_RESOURCE_ALLOC(0);
}

void emma_test_resource_free(void* data)
{
	EMMA_RESOURCE_FREE(data);
}

int emma_test_resource_write(void* user_data, uint8_t* data_block, emma_size_t block_size, emma_index_t block_index)
{
	return 0;
}

int emma_test_resource_read(void* user_data, uint8_t* data_block, emma_size_t block_size, emma_index_t block_index)
{
	return 0;
}

void emma_test_resource_reset(void* data)
{
	((emma_test_resource_t*)data)->value = 0;
}

void emma_test_resource_init()
{
	EMMA_RESOURCE_MEM_INIT();
}

emma_resource_root_t emma_test_resource = {
	0,
	"R",
	"text/json",
	
	emma_test_resource_init,
	emma_test_resource_alloc,
	emma_test_resource_free,
	emma_test_resource_reset,
	emma_test_resource_read,
	emma_test_resource_write,

	NULL
};
