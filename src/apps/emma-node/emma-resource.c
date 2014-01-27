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
 
/* -------------------------------------------------------- */
/* ------- Plateform dependent implementations ------------ */
/* -------------------------------------------------------- */

// Include all roots files here
#include "root-agent.h"
#include "root-resource.h"
#include "root-system.h"

static emma_resource_root_t* resources_roots[] = {
	EMMA_RESOURCE_ROOT(root_agent),
	EMMA_RESOURCE_ROOT(root_resource),
	EMMA_RESOURCE_ROOT(root_system)
};



/* -------------------------------------------------------- */
/* -------------------------------------------------------- */
/* -------------------------------------------------------- */






/* -------------------------------------------------------- */
/* ------ EMMA resources interface implementation --------- */
/* -------------------------------------------------------- */

#include "emma-resource.h"
#include "memb.h"
#include <string.h>

#define LDEBUG 0
#if (LDEBUG | GDEBUG | RESOURCE_DEBUG)
	#define PRINT(...) OUTPUT_METHOD("[EMMA RESOURCE INTERFACE] " __VA_ARGS__)
	#define PRINTS(...) OUTPUT_METHOD(__VA_ARGS__)
#else
	#define PRINT(...)
	#define PRINTS(...)
#endif

MEMB(emma_resources_R, emma_resource_t, EMMA_MAX_RESOURCES);
#define EMMA_RESOURCE_MEM_INIT() memb_init(&emma_resources_R)
#define EMMA_RESOURCE_FREE(r) memb_free(&emma_resources_R, r)
#define EMMA_RESOURCE_ALLOC(r) memb_alloc(&emma_resources_R)

#define EMMA_NB_ROOTS resources_roots_size
#define EMMA_MAX_ROOTS (MAXIMUM_VALUE(emma_resource_root_id_t) - 1)
#define DEMI_VALUE(type) (1u<<((8*sizeof(type))-1))
#define MAXIMUM_VALUE(type) (DEMI_VALUE(type) + (DEMI_VALUE(type) - 1))
#define SIZEOF_STATIC(array) (sizeof((array)) / sizeof((array)[0]))
static const emma_resource_root_id_t resources_roots_size = SIZEOF_STATIC(resources_roots);


static emma_resource_root_id_t get_next_resource_name_by_root_rootTmp = 0;
static emma_resource_t* get_next_resource_name_by_root_previous = NULL;
static emma_resource_t* get_next_resource_name_by_root_current = NULL;
static emma_resource_t* get_next_resource_name_by_root_next = NULL;
static uint8_t get_next_resource_name_by_root_found = 0;

// TODO XXX:
// MEMB for emma_resource_t !!!!!

// Helper functions
static void emma_clean_resource(emma_resource_t* resource)
{
	//PRINT("emma_clean_resource not implemented.\n");
	resource->nextOfResearch = NULL;
	resource->name[0] = '\0';
	//emma_resource_root_id_t type;
	resource->data = NULL;
	resource->mutex = 0;
	resource->lastModified = 0;
	resource->prev = NULL;
	resource->next = NULL;
	resource->subRoot = NULL;
}

static uint8_t emma_resource_name_is_basic(char* uri)
{
	int cnt = 0;
	while (uri[cnt] != '\0') {if(uri[cnt]=='/')break;cnt++;}
	if (uri[cnt] == '\0') return 1;
	else return 0;
}

static char* emma_get_resource_basename(char* uri)
{
	int cnt = strlen(uri);
	while(cnt!=0 && uri[cnt]!='/') {cnt--;}
	//PRINT("Basename index: %d\n", strlen(uri) - cnt);
	if (uri[cnt] != '/') return (uri + cnt);
	else return (uri + cnt + 1);
}

static char* emma_get_name(char* uri)
{
	int cnt=0;
	if (!uri) return NULL;
	while(uri[cnt]!='\0' && uri[cnt]!='/') cnt++;
	if (uri[cnt]=='\0') return (uri + cnt);
	else return (uri + cnt + 1);
}

static emma_resource_root_id_t emma_get_root(char* uri)
{
	int cnt = 0;
	int rootLength = 0;
	if (!uri) return 0;
	while (uri[cnt] != '\0' && uri[cnt] != '/') cnt++;
	rootLength = cnt;
	//PRINT("[GET ROOT] Watching '%s' ; rootLength=%d\n", uri, rootLength);
	for (cnt=0 ; cnt<EMMA_NB_ROOTS ; cnt++)
	{
		//PRINT("[GET ROOT] Watching '%s'\n", (resources_roots[cnt])->name);
		if ((rootLength==strlen((resources_roots[cnt])->name)) && (strncmp(uri, (resources_roots[cnt])->name, rootLength) == 0)) break;
	}
	if (cnt < EMMA_NB_ROOTS) return cnt+1;
	else return 0;
}


static void emma_get_resource(char* uri, emma_resource_root_id_t root, emma_resource_t** folder, emma_resource_t** previousResource, emma_resource_t** resource)
{
	// Variables
	int cnt = 0;
	emma_resource_t* current = NULL;
	int startIndex = 0;
	int stopIndex = 0;
	
	// Init user variables
	if (folder) *folder = NULL;
	if (previousResource) *previousResource = NULL;
	if (resource) *resource = NULL;
	if (root <= 0 || root > EMMA_NB_ROOTS) return;
	else root--;
	
	current = (resources_roots[root])->list;
	startIndex = 0;
	stopIndex = 0;
	
	//PRINT("[GET] '%s'\n", uri);
	
	do
	{
		while (uri[cnt] != '\0') {if(uri[cnt]=='/')break;cnt++;}
		
		// Resource research
		stopIndex = cnt;
		
		if (startIndex)
		{
			//PRINT("[GET] Entering folder\n");
			if (folder) *folder = current;
			current = current->subRoot;
		}
		
		if (previousResource) *previousResource = current;
		while(current)
		{
			//PRINT("[GET] Watching: '%s'\n", current->name);
			if ((strlen(current->name)==(stopIndex-startIndex)) && (strncmp(uri+startIndex, current->name, stopIndex-startIndex)==0))
			{
				//PRINT("[GET] Found ; startIndex=%d : stopIndex=%d\n", startIndex, stopIndex);
				break;
			}
			if (previousResource) *previousResource = current;
			current = current->next;
		}
		
		if (current)
		{
			//PRINT("[GET] Found.\n");
			if (resource) *resource = current;
		}
		else
		{
			//PRINT("[GET] Not Found.\n");
			if (resource) *resource = NULL;
			if ((uri[cnt] != '\0') && (folder)) *folder = NULL;
			break;
		}
		if (uri[cnt] != '\0') startIndex = stopIndex+1;
		else startIndex = stopIndex;
		cnt = startIndex;
		
	} while (uri[cnt] != '\0');
}

static void print_full_path_inverted(emma_resource_t* resource)
{
	emma_resource_t* current = resource;
	PRINTS("%s/", current->name);
	if (resource)
	{
		while(current->prev)
		{
			if (current->prev->subRoot == current)
			{
				PRINTS("%s/", current->prev->name);
			}
			current = current->prev;
		}
	}
}

static emma_size_t print_full_path(emma_resource_root_id_t root, emma_resource_t* resource, uint8_t* block, emma_size_t blockSize)
{
	emma_resource_t* current = resource;
	emma_size_t resourceNameLen = 0;
	emma_size_t nbBytesWritten = 0;
	//PRINTS("%s/", current->name);
	
	if (root <= 0 || root > EMMA_NB_ROOTS) return 0;
	else root--;
	
	if (current)
	{
		// Print resource
		resourceNameLen = strlen(current->name);
		if (blockSize >= resourceNameLen+1)
		{
			memcpy(block+blockSize-resourceNameLen,current->name,resourceNameLen);
			//PRINTS("[PFP]%s/", current->name);
			blockSize -= (resourceNameLen+1);
			block[blockSize] = '/';
			nbBytesWritten += (resourceNameLen+1);
		}
		else return 0;
		
		// Print UPPER folders
		while(current->prev)
		{
			if (current->prev->subRoot == current)
			{
				resourceNameLen = strlen(current->prev->name);
				if (blockSize >= resourceNameLen+1)
				{
					memcpy(block+blockSize-resourceNameLen,current->prev->name,resourceNameLen);
					//PRINTS("[PFP]%s/", current->prev->name);
					blockSize -= (resourceNameLen+1);
					block[blockSize] = '/';
					nbBytesWritten += (resourceNameLen+1);
				}
				else return 0;
			}
			current = current->prev;
		}
	}
	
	// Print resource ROOT name
	resourceNameLen = strlen((resources_roots[root])->name);
	if (blockSize >= resourceNameLen)
	{
		memcpy(block+blockSize-resourceNameLen,(resources_roots[root])->name,resourceNameLen);
		//PRINTS("%s/", current->prev->name);
		blockSize -= resourceNameLen;
		nbBytesWritten += resourceNameLen;
	}
	else return 0;
	
	// Shift block to the left
	if (blockSize) memcpy(block, block+blockSize, nbBytesWritten);
	
	return nbBytesWritten;
}
// ---------------- Helper functions

void emma_resource_init()
{
	//PRINT("init_resources not implemented\n");
	emma_resource_root_id_t root = 0;
	EMMA_RESOURCE_MEM_INIT();
	
	PRINT("[INFO] Maximum roots: %d\n", EMMA_MAX_ROOTS);
	for (root=0 ; root < EMMA_NB_ROOTS ; root++)
	{
		PRINT("[INFO] Initializing '%s(%d)' (%s) ...", (resources_roots[root])->name, root, (resources_roots[root])->description);
		(resources_roots[root])->type = root;
		if ((resources_roots[root])->init) (resources_roots[root])->init();
		else PRINT("[WARNING] No init method\n");
		PRINTS(" Done.\n");
	}
}

uint8_t get_resources_number()
{
	return EMMA_NB_ROOTS;
}


void
get_next_resource_name_by_root_reset(){
	get_next_resource_name_by_root_rootTmp  = 0;
	get_next_resource_name_by_root_previous = NULL;
	get_next_resource_name_by_root_next     = NULL;
	get_next_resource_name_by_root_found	= 0;
	get_next_resource_name_by_root_current = (resources_roots[get_next_resource_name_by_root_rootTmp])->list;

}

emma_size_t get_next_resource_name_by_root(emma_resource_root_id_t root, uint8_t* block, emma_size_t block_size)
{
	emma_size_t nbBytesRead = 0;
	
	if (root != 0)
	{
		get_next_resource_name_by_root_rootTmp = root-1;
		get_next_resource_name_by_root_previous = NULL;
		get_next_resource_name_by_root_current = NULL;
		get_next_resource_name_by_root_next = NULL;
		get_next_resource_name_by_root_found = 0;
		get_next_resource_name_by_root_current = (resources_roots[get_next_resource_name_by_root_rootTmp])->list;
	}
	
	if (get_next_resource_name_by_root_rootTmp < 0 || get_next_resource_name_by_root_rootTmp >= EMMA_NB_ROOTS) return 0;
	
	while (get_next_resource_name_by_root_current && !get_next_resource_name_by_root_found)
	{
		if (get_next_resource_name_by_root_previous == get_next_resource_name_by_root_current->prev)
		{
			//previousVisit = get_next_resource_name_by_root_current;
			get_next_resource_name_by_root_previous = get_next_resource_name_by_root_current;
			get_next_resource_name_by_root_next = get_next_resource_name_by_root_current->subRoot;
		}
		if (get_next_resource_name_by_root_next == NULL || get_next_resource_name_by_root_previous == get_next_resource_name_by_root_current->subRoot)
		{
			//get_next_resource_name_by_root_current->nextOfResearch = previousVisit;
			//PRINT("[GET] RESOURCE: ");
			//print_full_path_inverted(get_next_resource_name_by_root_current);
			//PRINTS(" ; prev=&%d", (int)get_next_resource_name_by_root_current->prev);

			get_next_resource_name_by_root_found = 1;
			get_next_resource_name_by_root_previous = get_next_resource_name_by_root_current;
			get_next_resource_name_by_root_next = get_next_resource_name_by_root_current->next;
		}
		if (get_next_resource_name_by_root_next == NULL || get_next_resource_name_by_root_previous == get_next_resource_name_by_root_current->next)
		{
			get_next_resource_name_by_root_previous = get_next_resource_name_by_root_current;
			get_next_resource_name_by_root_next = get_next_resource_name_by_root_current->prev;
		}
		//previousVisit = next;
		get_next_resource_name_by_root_current = get_next_resource_name_by_root_next;
	}
	
	if (get_next_resource_name_by_root_found)
	{
		// Complete block with resource name
		//PRINT("[GET NEXT NAME] ROOT %d\n", get_next_resource_name_by_root_);
		//print_full_path_inverted(get_next_resource_name_by_root_previous);
		nbBytesRead = print_full_path(get_next_resource_name_by_root_rootTmp+1, get_next_resource_name_by_root_previous, block, block_size);
		get_next_resource_name_by_root_found = 0;
	}
	else if (!get_next_resource_name_by_root_current)
	{
		//PRINT("END");
		nbBytesRead = print_full_path(get_next_resource_name_by_root_rootTmp+1, NULL, block, block_size);
		get_next_resource_name_by_root_previous = NULL;
		get_next_resource_name_by_root_next = NULL;
		get_next_resource_name_by_root_rootTmp++;
		get_next_resource_name_by_root_current = (resources_roots[get_next_resource_name_by_root_rootTmp])->list;
	}
	
	return nbBytesRead;
	
	//else return 0;
}

emma_resource_name_t* get_resources_names_by_root(emma_resource_root_id_t root)
{
	PRINT("get_resources_names_by_root not functionnal\n");
	//return NULL;
	//emma_resource_t* previousVisit = NULL;
	emma_resource_t* previous = NULL;
	emma_resource_t* current = NULL;
	emma_resource_t* next = NULL;
	
	if (root <= 0 || root > EMMA_NB_ROOTS) return NULL;
	else root--;
	
	current = (resources_roots[root])->list;
	while (current)
	{
		if (previous == current->prev)
		{
			//previousVisit = current;
			previous = current;
			next = current->subRoot;
		}
		if (next == NULL || previous == current->subRoot)
		{
			//current->nextOfResearch = previousVisit;
			PRINT("[GET] RESOURCE: ");
			print_full_path_inverted(current);
			//PRINTS(" ; prev=&%d", (int)current->prev);
			PRINTS("\n");
			previous = current;
			next = current->next;
		}
		if (next == NULL || previous == current->next)
		{
			previous = current;
			next = current->prev;
		}
		//previousVisit = next;
		current = next;
	}
	
	return NULL;
	//return (emma_resource_name_t*)((resources_roots[root])->list);
}

emma_resource_root_id_t emma_get_resource_root(char* uri)
{
	return emma_get_root(uri);
}

char* emma_get_resource_name(char* uri)
{
	return emma_get_name(uri);
}

emma_resource_name_t* get_resources_names_by_uri(char* uri)
{
	PRINT("get_resources_names_by_uri not implemented.\n");
	return NULL;
}

emma_resource_access_t get_resources_access(emma_resource_root_id_t root)
{
	PRINT("get_resources_access not implemented\n");
	return 0;
	if (root <= 0 || root > EMMA_NB_ROOTS) return 0;
	else root--;
}

char* get_resources_root_description(emma_resource_root_id_t root)
{
	if (root <= 0 || root > EMMA_NB_ROOTS) return NULL;
	else root--;
	return (resources_roots[root])->description;
}

uint8_t emma_resource_exists(char* uri)
{
	PRINT("emma_resource_exists:\n");
	emma_resource_root_id_t root = 0;
	emma_resource_t* resource = NULL;
	
	root = emma_get_root(uri);
	if (root <= 0 || root > EMMA_NB_ROOTS) return 0;
	else root--;
	emma_get_resource(emma_get_name(uri), root+1, NULL, NULL, &resource);
	
	if (resource) return 1;
	else return 0;
}

uint8_t emma_resource_add (emma_resource_root_id_t root, char* name)
{
	//PRINT("emma_resource_add:\n");
	emma_resource_t *folder, *previousResource, *resource;
	
	if (root <= 0 || root > EMMA_NB_ROOTS) return 1;
	else root--;
	
	if (!name || !(*name)) return 6; // Empty resource name
	
	PRINT("[ADD] NAME: '%s'\n", name);
	emma_get_resource(name, root+1, &folder, &previousResource, &resource);
	
	if (! resource)
	{
		// Allocate resource
		resource = (emma_resource_t*) EMMA_RESOURCE_ALLOC(sizeof(emma_resource_t));
		
		// Clean resource
		// TODO XXX !!!
		emma_clean_resource(resource);
	
		// Allocate user data
		if (resource && ((resources_roots[root])->alloc)) {resource->data=(resources_roots[root])->alloc();}
		else
		{
			EMMA_RESOURCE_FREE(resource);
			return 3; // No more resources available || not allocatable root
		}

		// Reset resource
		if (resource->data)
		{
			//PRINT("[ADD] Allocated=&%d\n", resource->data);
			strncpy(resource->name, emma_get_resource_basename(name), EMMA_RESOURCE_NAME_SIZE);
			if ((resources_roots[root])->reset) (resources_roots[root])->reset(name, resource->data);
		}
		else
		{
			EMMA_RESOURCE_FREE(resource);
			return 4; // No more space available on this root
		}
		

		// Add resource to list
		if (folder)
		{
			//PRINT("[ADD] Folder: '%s'\n", folder->name);
			//PRINT("[ADD] Basename: '%s'\n", emma_get_resource_basename(name));
			resource->prev = folder;
			resource->next = folder->subRoot;
			folder->subRoot = resource;
			if (resource->next) resource->next->prev = resource;
		}
		else
		{
			// Resource name should not contain '/' at this stage
			if (emma_resource_name_is_basic(name))
			{
				// Add resource to ROOT
				//PRINT("[ADD] Resource: '%s'\n", emma_get_resource_basename(name));
				resource->next = (resources_roots[root])->list;
				(resources_roots[root])->list = resource;
				if (resource->next) resource->next->prev = resource;
			}
			else
			{
				// Cannot add resource
				PRINT("RESOURCE NAME IS NOT BASIC\n");
				return 5;
			}
		}
		return 0;
	}
	else return 2;
}

uint8_t emma_resource_del (char* uri)
{
	//PRINT("emma_resource_del not implemented.\n");
	//return 0;
	
	// Variables
	emma_resource_root_id_t root = 0;
	emma_resource_t* folder = NULL;
	emma_resource_t* resource = NULL;
	emma_resource_t* previousResource = NULL;
	
	// In case of the delete is considering current agent, we reinitialize agent iterator function
	get_next_resource_name_by_root_reset();

	root = emma_get_root(uri);
	
	if (root <= 0 || root > EMMA_NB_ROOTS) return 1; // No associated root !!!
	else root--;
	emma_get_resource(emma_get_name(uri), root+1, &folder, &previousResource, &resource);
	
	if (resource)
	{
		if (resource->subRoot != NULL) return 3; // Unable to delete non-empty folder
		
		// Free user data
		if ((resources_roots[root])->free) (resources_roots[root])->free(uri, resource->data);
		else return 4; // Unable to delete resource
		
		// Extract resource from list or subRoot
		if (folder)
		{
			if (previousResource == resource)
			{
				folder->subRoot = resource->next;
				if (folder->subRoot) folder->subRoot->prev = folder;
			}
			else
			{
				previousResource->next = resource->next;
				if (previousResource->next) previousResource->next->prev = previousResource;
			}
		}
		else
		{
			if (previousResource == resource)
			{
				(resources_roots[root])->list = resource->next;
				if ((resources_roots[root])->list) (resources_roots[root])->list->prev = NULL;
			}
			else
			{
				previousResource->next = resource->next;
				if (previousResource->next) previousResource->next->prev = previousResource;
			}
		}
		
		// Free resource
		EMMA_RESOURCE_FREE(resource);
		
		return 0;
	}
	else return 2;
}

int emma_resource_read (char* uri, uint8_t* data_block, emma_size_t block_size, emma_index_t block_index)
{
	PRINT("emma_resource_read:\n");
	emma_resource_root_id_t root = 0;
	emma_resource_t* resource = NULL;
	
	root = emma_get_root(uri);
	if (root <= 0 || root > EMMA_NB_ROOTS) return 0;
	else root--;
	emma_get_resource(emma_get_name(uri), root+1, NULL, NULL, &resource);
	
	if (resource)
	{
		PRINT("[READ] '%s'\n", resource->name);
		if ((resources_roots[root])->read) return (resources_roots[root])->read(uri, resource->data, data_block, block_size, block_index);
		else return 0;
	}
	else return 0;
}

int emma_resource_write (char* uri, uint8_t* data_block, emma_size_t block_size, emma_index_t block_index)
{
	PRINT("emma_resource_write:\n");
	emma_resource_root_id_t root = 0;
	emma_resource_t* resource = NULL;
	
	root = emma_get_root(uri);
	if (root <= 0 || root > EMMA_NB_ROOTS) return 0;
	else root--;
	emma_get_resource(emma_get_name(uri), root+1, NULL, NULL, &resource);
	
	if (resource)
	{
		PRINT("[WRITING] '%s'\n", resource->name);
		if ((resources_roots[root])->write) return (resources_roots[root])->write(uri, resource->data, data_block, block_size, block_index);
		else return 0;
	}
	else return 0;
}

uint8_t emma_resource_lock (char* uri)
{
	PRINT("emma_resource_lock: for %s : ", uri);
	emma_resource_t* res = NULL;
	emma_get_resource(emma_get_name(uri), emma_get_root(uri), NULL, NULL, &res);
	if (res)
	{
		if (res->mutex == EMMA_MUTEX_LOCKED) ;
		else if (res->mutex == EMMA_MUTEX_RELEASED)
		{
			PRINT("[DONE]\n");
			res->mutex = EMMA_MUTEX_LOCKED;
			return 1;
		}
		else ;
	}
	PRINT("[REFUSE]\n");
	return 0; // For compatibility reasons with function 'emma_resource_get_next_name_by_root' which
	// returns also ROOT name (ROOT name cannot be locked ...)
}

uint8_t emma_resource_is_locked(char* uri){
	emma_resource_t* res = NULL;
	emma_get_resource(emma_get_name(uri), emma_get_root(uri), NULL, NULL, &res);
	if (res)
	{
		return (res->mutex == EMMA_MUTEX_LOCKED)?1:0;
	}
	return 0;
}

uint8_t emma_resource_release (char* uri)
{
	//PRINT("emma_resource_release:\n");
	emma_resource_t* res = NULL;
	emma_get_resource(emma_get_name(uri), emma_get_root(uri), NULL, NULL, &res);
	if (res)
	{
		if (res->mutex == EMMA_MUTEX_RELEASED) {return 0;}
		else if (res->mutex == EMMA_MUTEX_LOCKED)
		{
			res->mutex = EMMA_MUTEX_RELEASED;
			return 1;
		}
		else {return 0;}
	}
	else return 0;
}

static emma_resource_mutex_t lastModifier = 0;
void emma_resource_set_last_modified (char* uri, emma_resource_mutex_t key)
{
	//PRINT("emma_resource_set_last_modified:\n");
	emma_resource_t* res = NULL;
	if (!uri) {lastModifier = key; return;}
	emma_get_resource(emma_get_name(uri), emma_get_root(uri), NULL, NULL, &res);
	if (res)
	{
		lastModifier = key;
		res->lastModified = key;
	}
}

emma_resource_mutex_t emma_resource_get_last_modified (char* uri)
{
	//PRINT("emma_resource_get_last_modified:\n");
	emma_resource_t* res = NULL;
	if (!uri) return lastModifier;
	emma_get_resource(emma_get_name(uri), emma_get_root(uri), NULL, NULL, &res);
	if (res)
	{
		return res->lastModified;
	}
	else return 0;
}

void emma_resource_set_clocktime (char* uri, uint32_t clocktime)
{
	//PRINT("emma_resource_get_last_modified:\n");
	emma_resource_t* res = NULL;
	emma_get_resource(emma_get_name(uri), emma_get_root(uri), NULL, NULL, &res);
	if (res)
	{
		res->clocktime = clocktime;
	}
}

uint32_t emma_resource_get_clocktime (char* uri){
	emma_resource_t* res = NULL;
	emma_get_resource(emma_get_name(uri), emma_get_root(uri), NULL, NULL, &res);
	if (res)
	{
		return res->clocktime;
	}	
	return 0;
}

uint8_t emma_resource_get_index_of_pattern(char* uri, char* pattern, emma_index_t* start, emma_index_t* stop)
{
	PRINT("emma_resource_get_index_of_pattern not implemented.\n");
	emma_resource_root_id_t root = 0;
	emma_resource_t* resource = NULL;
	
	root = emma_get_root(uri);
	if (root <= 0 || root > EMMA_NB_ROOTS) return 0;
	else root--;
	emma_get_resource(emma_get_name(uri), root+1, NULL, NULL, &resource);
	
	if (resource)
	{
		if ((resources_roots[root])->match) return (resources_roots[root])->match(uri, resource->data, pattern, start, stop);
		else return 0;
	}
	else return 0;
}

uint8_t emma_resource_is_root(char* uri)
{
	char* temp = emma_get_name(uri);
	//PRINT("[ISROOT] '%c'\n", *temp);
	if (*temp) return 0;
	else return 1;
}

int emma_resource_open(char* uri)
{
	PRINT("emma_resource_open:\n");
	emma_resource_root_id_t root = 0;
	emma_resource_t* resource = NULL;
	
	root = emma_get_root(uri);
	if (root <= 0 || root > EMMA_NB_ROOTS) return 0;
	else root--;
	emma_get_resource(emma_get_name(uri), root+1, NULL, NULL, &resource);
	if (resource)
	{
		PRINT("[OPEN] '%s'\n", resource->name);
		if ((resources_roots[root])->open) return (resources_roots[root])->open(uri, resource->data);
		return 1;
	}
	
	return -1;
}

int emma_resource_close(char* uri)
{
	PRINT("emma_resource_open:\n");
	emma_resource_root_id_t root = 0;
	emma_resource_t* resource = NULL;
	
	root = emma_get_root(uri);
	if (root <= 0 || root > EMMA_NB_ROOTS) return 0;
	else root--;
	emma_get_resource(emma_get_name(uri), root+1, NULL, NULL, &resource);
	
	if (resource)
	{
		PRINT("[OPEN] '%s'\n", resource->name);
		if ((resources_roots[root])->close) return (resources_roots[root])->close(uri, resource->data);
		else return 1;
	}
	else return 0;
}
