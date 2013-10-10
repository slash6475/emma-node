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
#include "emma-resource.h"
#include "emma-client.h"
#include "emma-server.h"

#define LDEBUG 1
#if (LDEBUG | GDEBUG)
	#define PRINT(...) OUTPUT_METHOD("[EMMA INIT] " __VA_ARGS__)
	#define PRINTS(...) OUTPUT_METHOD(__VA_ARGS__)
#else
	#define PRINT(...)
	#define PRINTS(...)
#endif

#define MIN(x,y) (((x)>(y))?(y):(x))
#define MAX(x,y) (((x)<(y))?(y):(x))

//CALCULATOR_STACK(emma_server_vm, NULL);
//CALCULATOR_STACK(emma_client_vm, NULL);

void emma_init()
{
	// Activate ROOT resources (CoAP resources)
	PRINT("init not implemented.\n");
	emma_resource_init();
	emma_client_init();
	emma_server_init();
	
	//emma_server_init();
	
	// Init evaluation stacks
	//set_reference_solver(&emma_server_vm_stack, emma_reference_solver);
	//set_reference_solver(&emma_client_vm_stack, emma_reference_solver);
}

// Should return EVAL_COMPLETE in case of success.
// All other status code is considered as an error.
/*eval_status_t emma_reference_solver (uint8_t* reference, uint8_t referenceSize, operand_t* value)
{
	char buffer[ERD_RES_NAME_SIZE];
	char val[EMMA_INT_SIZE];
	uint8_t cnt = 0;
	int nbBytesRead = 0;
	if (((reference != NULL) && (referenceSize != 0)) && (value != NULL))
	{
		snprintf(buffer, MIN(referenceSize + 1, ERD_RES_NAME_SIZE), "%s", (char*)reference);
		while(cnt < MIN(referenceSize + 1, ERD_RES_NAME_SIZE)) {if(buffer[cnt]=='#')buffer[cnt]='/';cnt++;}
		PRINT("[REFERENCE SOLVER] Solving reference of '%s'\n", buffer);
		//if (rest_get_data_block(buffer, (uint8_t*)value, sizeof(operand_t), 0, NULL) == sizeof(operand_t)) return EVAL_COMPLETE;
		nbBytesRead = rest_get_data_block(buffer, (uint8_t*)val, EMMA_INT_SIZE, 0, NULL);
		PRINT("[REFERENCE SOLVER] nbBytesRead: %d\n", nbBytesRead);
		
		for (cnt=0 ; cnt<nbBytesRead ; cnt++) PRINT("%c-", (char)(val[cnt]));
		
		if (nbBytesRead == 0) {return EVAL_BAD_REFERENCE;}
		else
		{
			*value = (operand_t)antoi(val, nbBytesRead);
			return EVAL_COMPLETE;
		}
	}
	else return EVAL_INVALID_ARG;
	PRINT("emma_reference_solver not implemented.\n");
	return EVAL_BAD_REFERENCE;
}*/
