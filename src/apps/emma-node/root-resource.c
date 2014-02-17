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
 
#include "root-resource.h"
#include "emma-eval.h"
#include "memb.h"

#define LDEBUG 1
#if (LDEBUG | GDEBUG)
	#define PRINT(...) OUTPUT_METHOD("[ROOT RESOURCE] " __VA_ARGS__)
	#define PRINTS(...) OUTPUT_METHOD(__VA_ARGS__)
#else
	#define PRINT(...)
	#define PRINTS(...)
#endif


// TODO:
// Read and Write must manage JSON ...

typedef struct rootResource {
	uint16_t value;
} root_resource_t;

static num_eval_t root_resource_stack;

#define ROOT_RESOURCE_MAX_ALLOC 5
MEMB(root_resource_R, root_resource_t, ROOT_RESOURCE_MAX_ALLOC);
#define ROOT_MEM_INIT() memb_init(&root_resource_R)
#define ROOT_FREE(a) memb_free(&root_resource_R, (a))
#define ROOT_ALLOC(a) memb_alloc(&root_resource_R)

#define MIN(x,y) (((x)>(y))?(y):(x))
#define MAX(x,y) (((x)<(y))?(y):(x))

/* TODO */
#define EMMA_INT_SIZE	9

void arrayShift(char* array, uint8_t length, uint8_t pos){
	int i;
	for (i=pos; i < length - 1; i++)
		array[i] = array[i+1];
}

eval_status_t root_resource_solver (uint8_t* reference, uint8_t referenceSize, operand_t* value)
{
	char buffer[EMMA_MAX_URI_SIZE];
	char val[EMMA_INT_SIZE];
	uint8_t cnt = 0;
	int nbBytesRead = 0;
	if (((reference != NULL) && (referenceSize != 0)) && (value != NULL))
	{
		snprintf(buffer, MIN(referenceSize + 1, EMMA_MAX_URI_SIZE), "%s", (char*)reference);
		if (referenceSize >= 2)
		{
			// Switch R (remote) with L (local) upon packet reception
			if (buffer[0] == 'R' && buffer[1]=='#') buffer[0]='L';
			//else PRINT("[REFERENCE SOLVER] No translation R -> L\n");
		}
		while(cnt < MIN(referenceSize + 1, EMMA_MAX_URI_SIZE)) {
			if(buffer[cnt]=='#')buffer[cnt]='/';

			else if(buffer[cnt]==' '){
				arrayShift(buffer, EMMA_MAX_URI_SIZE, cnt);
			}
			else cnt++;
		}
//		for(cnt=strlen(buffer)-1; cnt > 0; cnt --)
//			if(buffer[cnt] == ' ') buffer[cnt] = '\0';
//			else break;

		PRINT("[REFERENCE SOLVER] Solving reference of '%s'\n", buffer);
		/*nbBytesRead = rest_get_data_block(buffer, (uint8_t*)value, sizeof(operand_t), 0, NULL);
		if (nbBytesRead == 4)
		{
			PRINTV2("[REFERENCE SOLVER] Reference solved !!\n");
			return EVAL_COMPLETE;
		}
		else
		{
			PRINTV2("[REFERENCE SOLVER] Reference unsolved\n");
		}*/
		//if (emma_resource_read(buffer, (uint8_t*)value, sizeof(operand_t), 0, NULL) == sizeof(operand_t)) return EVAL_COMPLETE;
		
		/* TODO: Lock resource before read */
		nbBytesRead = emma_resource_read(buffer, (uint8_t*)val, EMMA_INT_SIZE, 0);
		//PRINTV2("[REFERENCE SOLVER] nbBytesRead: %d\n", nbBytesRead);
		
		for (cnt=0 ; cnt<nbBytesRead ; cnt++) PRINTS("%c-", (char)(val[cnt]));
		
		/* TODO: Release resource after read */
		
		
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
 
void root_resource_init()
{
	ROOT_MEM_INIT();
	
	/* TODO */
	set_reference_solver(&root_resource_stack, root_resource_solver);
}
static void* root_resource_alloc() {return ROOT_ALLOC(0);}
void root_resource_free(char* uri, void* data) {ROOT_FREE(data);}

void root_resource_reset(char* uri, void* data)
{
	((root_resource_t*)data)->value = 0;
}

int root_resource_open(char* uri, void* data)
{
	//PRINT("open not implemented\n");
	// TODO: Implement Post-fixed evaluator, each resource should maintain a stack to avoid
	// data corruption
	/* TODO */
	clean_stack(&root_resource_stack);
	/* **** */
	return 1;
}

int root_resource_write(char* uri, void* user_data, uint8_t* data_block, emma_size_t block_size, emma_index_t block_index)
{
	eval_status_t err = EVAL_COMPLETE;
	
	if ((user_data==NULL) || (data_block==NULL)) return 0;
	//root_resource_t* res = (root_resource_t*)user_data;
	//PRINT("[WRITE] Address=&%d\n", (root_resource_t*)user_data);
	
	/*if (block_index == 0)
	{
		((root_resource_t*)user_data)->value = antoi((char*)data_block, block_size);
		PRINT("[WRITE] value=%d\n", ((root_resource_t*)user_data)->value);
		return block_size;
	} else return 0;*/
	
	/* TODO */
	//*
	err = evaluate_block(&root_resource_stack, data_block, block_size, NULL);
	if (err == EVAL_READY)
	{
		PRINT("[EMMA RES] Nothing to compute.\n");
		//eval_print_status(err);
		return block_size;
	}
	else return 0;//*/

	/*if ((sizeof(emma_resource_value_t) != block_size) || (block_index != 0)) {return 0;}
	else {memcpy(&(res->value), data_block, block_size); return block_size;}
	return 0;*/
}

int root_resource_read(char* uri, void* user_data, uint8_t* data_block, emma_size_t block_size, emma_index_t block_index)
{
	if ((user_data==NULL) || (data_block==NULL)) return 0;
	root_resource_t* res = (root_resource_t*)user_data;
	
	//PRINT("[READ] Address=&%d\n", (root_resource_t*)user_data);
	
	return intoa(res->value, data_block, block_size, block_index);
}

int root_resource_close(char* uri, void* data)
{
	//PRINT("close not implemented\n");
	/* TODO */
	// If resource was written only !
	//*
	eval_status_t err = EVAL_COMPLETE;
	operand_t result = 0;
	err = evaluate_block(&root_resource_stack, NULL, 0, &result);
	if (err != EVAL_COMPLETE)
	{
		PRINT("[EMMA RES] Invalid resource(2).\n");
		//eval_print_status(err);
	}
	else
	{
		PRINT("[EMMA RES] New resource value is %d\n", (operand_t)result);
		((root_resource_t*)data)->value = (int)result;
		//emma_agents_run(); // Poll EMMA agents process because resource has been modified
		//responseCode = REST.status.OK;
	}//*/
	return 1;
}

emma_resource_root_t root_resource = {
	0,
	"L",
	"ct=\"text/raw\";rt=\"Resource\"",
	//"",
	
	root_resource_init,
	root_resource_alloc,
	root_resource_free,
	root_resource_reset,
	root_resource_open,
	root_resource_read,
	root_resource_write,
	root_resource_close,
	NULL,

	NULL
};
