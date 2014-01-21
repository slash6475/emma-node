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

/**
	* \addtogroup resource-group
	* @{
	*/

/**
 * \file
 *      EMMA resources interface
 * \author
 *      Nicolas Mardegan <mardegan@ece.fr>
 *
 * Resources are classified by ROOTs following a typical filesystem representation.
 * Each ROOT is the entry point of a set of resources that behave similarly.
 * A ROOT is actually characterized by a set of function that apply to all the resources it contains.
 *
 * These functions (or methods) can be classified by usage:
 * 		- Memory management: allocation, de-allocation, reset, MUTEX-lock, MUTEX-release
 * 		- Access (inspired from POSIX): open, read, write, close
 * 		- Processing: Pattern matching
 *
 * The declaration of a new ROOT is as follow:
 * 		1) Create new sources using the ROOT-pattern files
 *		2) Replace "PATTERN" and "pattern" with the desired ROOT name in these files
 *		3) Implement functions of the ROOT, remember that it apply to all resources that belong to the ROOT
 *		4) Instantiate the ROOT-structure in the table located in 'emma-resource.c'
 * N.B.: All functions pointers in a ROOT-structure can be NULL, the performance will change accordingly.
 */


#ifndef __EMMA_RESOURCE_H_INCLUDED__
#define __EMMA_RESOURCE_H_INCLUDED__


#include "emma-conf.h"

#define EMMA_RESOURCE_NAME_SIZE 16

typedef uint8_t emma_resource_root_id_t;
typedef int emma_resource_mutex_t;
typedef int emma_resource_access_t;
typedef int emma_index_t;
typedef uint16_t emma_size_t;

typedef struct EmmaResourceName {
	struct EmmaResourceName *next;
	char name[EMMA_RESOURCE_NAME_SIZE];
} emma_resource_name_t;


typedef void* (* emma_resource_allocator)();
typedef void (* emma_resource_cleaner)(char* uri, void* data);
typedef int (* emma_resource_opener)(char* uri, void* data);
typedef int (* emma_resource_writer) (char* uri, void* user_data, uint8_t* data_block, emma_size_t block_size, emma_index_t block_index);
typedef int (* emma_resource_reader) (char* uri, void* user_data, uint8_t* data_block, emma_size_t block_size, emma_index_t block_index);
typedef int (* emma_resource_closer)(char* uri, void* data);
typedef void (* emma_resource_setter) (char* uri, void* data);
typedef void (* emma_resource_root_init) ();
typedef uint8_t (* emma_resource_regex) (char* uri, void* user_data, char* pattern, emma_index_t* start, emma_index_t* stop);

#define EMMA_MUTEX_LOCKED	1
#define EMMA_MUTEX_RELEASED	0

typedef struct EmmaResource {
	struct EmmaResource *nextOfResearch;
	char name[EMMA_RESOURCE_NAME_SIZE];
	//emma_resource_root_id_t type;
	void* data;
	emma_resource_mutex_t mutex;
	emma_resource_mutex_t lastModified;
	struct EmmaResource *next;
	struct EmmaResource *prev;
	struct EmmaResource *subRoot;
	uint32_t clocktime;
} emma_resource_t;

typedef struct {
	emma_resource_root_id_t type;
	//char name[EMMA_RESOURCE_ROOT_NAME_SIZE+1];
	//char description[EMMA_RESOURCE_ROOT_DESC_SIZE+1];
	char* name;
	char* description;
	
	emma_resource_root_init init;
	emma_resource_allocator alloc;
	emma_resource_cleaner		free;
	emma_resource_setter		reset;
	emma_resource_opener		open;
	emma_resource_reader		read;
	emma_resource_writer		write;
	emma_resource_closer		close;
	emma_resource_regex			match;

	emma_resource_t* list;
} emma_resource_root_t;

#define EMMA_RESOURCE_ROOT(root)	&(root)


/**
	* Init EMMA resources.
	*
	*/
void emma_resource_init();

/**
	* Get the number of EMMA root-resources.
	*
	* \return The number of EMMA root-resources.
	*/
uint8_t get_resources_number();

/**
	* Fill a buffer with the name of the next resource.
	*
	* \param root The ROOT-resource identifier.
	*
	* \param block The block where resource name will be written.
	*
	* \param block_size The size of block.
	*
	* \return The number of bytes written in block, 0 if done or if an error occured.
	*
	* The root parameter is firstly set to a valid root-ID, then a 0 indicates
	* to the function that it should continue with the next resource.
	*/
emma_size_t get_next_resource_name_by_root(emma_resource_root_id_t root, uint8_t* block, emma_size_t block_size);

/**
	* Reset tree routing of resource when we remove a resource in it.
	*
	* \return 
	*
	*/
void get_next_resource_name_by_root_reset();

/**
	* Get the root-ID of a given URI.
	*
	* \param uri A NULL-terminated string representing an URI.
	*
	* \return The ROOT-resource identifier, 0 if none.
	*/
emma_resource_root_id_t emma_get_resource_root(char* uri);

/**
	* Get the resource name (without ROOT) of a given URI.
	*
	* \param uri A NULL-terminated string representing an URI.
	*
	* \return A pointer on uri where the resource name starts.
	*/
char* emma_get_resource_name(char* uri);

/**
	* Tell if an URI represents a pure ROOT-resource.
	*
	* \param uri A NULL-terminated string representing an URI.
	*
	* \return 1 if true, 0 if false.
	*/
uint8_t emma_resource_is_root(char* uri);

/**
	* Call the open method of the given resource.
	*
	* \param uri A NULL-terminated string representing an URI.
	*
	* \return The return value of the open function, 1 if none, 0 if resource doesn't exists.
	*/
int emma_resource_open(char* uri);

/**
	* Call the close method of the given resource.
	*
	* \param uri A NULL-terminated string representing an URI.
	*
	* \return The return value of the close function, 1 if none, 0 if resource doesn't exists.
	*/
int emma_resource_close(char* uri);

/**
	* Get the list of resources names under a root.
	*
	* \param root The ROOT-resource identifier.
	*
	* \return A linked-list of resources names.
	*/
emma_resource_name_t* get_resources_names_by_root(emma_resource_root_id_t root);

/**
	* Get the list of resources names within an URI.
	*
	* \param uri The URI of the resource to get names
	*
	* \return A linked-list of resources names.
	*/
emma_resource_name_t* get_resources_names_by_uri(char* uri);

/**
	* Get ROOT-resource rights.
	*
	* \param root The ROOT-resource identifier.
	*
	* \return The rights of the specified ROOT-resource.
	*/
emma_resource_access_t get_resources_access(emma_resource_root_id_t root);

/**
	* Get ROOT-resource description.
	*
	* \param root The ROOT-resource identifier.
	*
	* \return A null-terminated string containing the description.
	*/
char* get_resources_root_description(emma_resource_root_id_t root);

/**
	* Check if resource exists
	*
	* \param uri A NULL-terminated string that represents the URI of the resource
	*
	* \return 1 if resource exists, 0 otherwise.
	*/
uint8_t emma_resource_exists(char* uri);

/**
	* Add an EMMA resource.
	*
	* \param root The ROOT-resource identifier.
	*
	* \param name A NULL-terminated string representing the name of the new resource
	*
	* \return 0 if successful, greater than 0 otherwise.
	*/
uint8_t emma_resource_add (emma_resource_root_id_t root, char* name);

/**
	* Delete an EMMA resource.
	*
	* \param uri A NULL-terminated string that represents the URI of the resource
	*
	* \return 0 if successful, greater than 0 otherwise.
	*/
uint8_t emma_resource_del (char* uri);

/**
	* Read a block from EMMA resource.
	*
	* \param uri A NULL-terminated string that represents the URI of the resource
	*
	* \param data_block The block to write resource in
	*
	* \param block_size The data_block size
	*
	* \param block_index The reading start-index in resource
	*
	* \return The number of bytes actually written in data_block.
	*/
int emma_resource_read (char* uri, uint8_t* data_block, emma_size_t block_size, emma_index_t block_index);

/**
	* Write a block in EMMA resource.
	*
	* \param uri A NULL-terminated string that represents the URI of the resource
	*
	* \param data_block The block to write in resource
	*
	* \param block_size The data_block size
	*
	* \param block_index The writing start-index in resource
	*
	* \return The number of bytes actually written in resource.
	*/
int emma_resource_write (char* uri, uint8_t* data_block, emma_size_t block_size, emma_index_t block_index);

/**
	* Lock an EMMA resource (embedded MUTEX).
	*
	* \param uri A NULL-terminated string that represents the URI of the resource
	*
	* \param key An opaque value to identify the resource locker.
	*
	* \return 1 if successful, 0 otherwise.
	*/
uint8_t emma_resource_lock (char* uri);

/**
	* Evaluate if an EMMA resource is locked (embedded MUTEX).
	*
	* \param uri A NULL-terminated string that represents the URI of the resource
	*
	* \return 1 if locked, 0 otherwise.
	*/
uint8_t emma_resource_is_locked(char* uri);

/**
	* Release an EMMA resource (embedded MUTEX).
	*
	* \param uri A NULL-terminated string that represents the URI of the resource
	*
	* \return 1 if successful, 0 otherwise.
	*/
uint8_t emma_resource_release (char* uri);

/**
	* Set the resource modifier identifier.
	*
	* \param uri A NULL-terminated string that represents the URI of the resource
	*
	* \param key An opaque value to identify the resource modifier.
	*
	* \return
	*/
void emma_resource_set_last_modified (char* uri, emma_resource_mutex_t key);

/**
	* Get the resource modifier identifier.
	*
	* \param uri A NULL-terminated string that represents the URI of the resource
	*
	* \return An opaque value to identify the resource modifier.
	*/
emma_resource_mutex_t emma_resource_get_last_modified (char* uri);

/**
	* Set the last time in which the resource has been modified.
	*
	* \param uri A NULL-terminated string that represents the URI of the resource
	*
	* \return
	*/
void emma_resource_set_clocktime (char* uri, uint32_t clocktime);

/**
	* Get the last time in which the resource has been modified.
	*
	* \param uri A NULL-terminated string that represents the URI of the resource
	*
	* \return The clocktime value 
	*/
uint32_t emma_resource_get_clocktime (char* uri);

/**
	* Get the index of pattern matched in resource.
	*
	* \param uri A NULL-terminated string that represents the URI of the resource
	*
	* \param pattern A NULL-terminated string that represents the pattern to match
	*
	* \param start The address (can be NULL) of an index to fill as result (start of matching)
	*
	* \param stop The address (can be NULL) of an index to fill as result (end of matching)
	*
	* \return 1 if pattern matched, 0 otherwise.
	*/
uint8_t emma_resource_get_index_of_pattern(char* uri, char* pattern, emma_index_t* start, emma_index_t* stop);

#endif // __EMMA_RESOURCE_H_INCLUDED__
