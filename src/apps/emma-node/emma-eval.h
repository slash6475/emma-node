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
 
#ifndef __EMMA_EVAL_H_INCLUDED__
#define __EMMA_EVAL_H_INCLUDED__


// Helper functions
int antoi(char* str, unsigned int size);
unsigned int intoa (int number, uint8_t* block, unsigned int blockSize, unsigned int blockIndex);
int axntoi(char* str, unsigned int size);


//---------------------------------------------------------------
//---------------------------------------------------------------
//---------------------------------------------------------------
//---------------------------------------------------------------

/*
#include "emma-resource.h"
#include <string.h>

#define REPL_BUFFER 16
#define REPL_EOB '"'
// End Of Buffer character

//typedef uint8_t erd_index_t;
typedef resource_t* (* repl_solver) (char* symbol);
typedef int (* repl_getter) (char* name, uint8_t* data_block, uint16_t block_size, erd_index_t block_index, erd_resource_id* cache_id);

uint8_t pre_evaluate (char* name, erd_index_t* block, uint8_t* output, uint8_t outputSize, repl_solver solver, repl_getter getter);
//*/

//---------------------------------------------------------------
//---------------------------------------------------------------
//---------------------------------------------------------------
//---------------------------------------------------------------

enum {
	EMMA_EVAL_MORE,
	EMMA_EVAL_END,
	EMMA_EVAL_INVALID
};

#include "contiki-net.h"
//uint8_t getIPaddress (uint8_t* block, uint16_t blockSize, uip_ipaddr_t* address, uint16_t* port, uint16_t* stopIndex);
uint8_t getIPaddress (char c, uip_ipaddr_t* address, uint16_t* port);

//---------------------------------------------------------------
//---------------------------------------------------------------
//---------------------------------------------------------------
//---------------------------------------------------------------

//*
// EMMA Calculator
#include "contiki.h"
#include <string.h>

// ----------------------------------------------------------------------
// Defines
#define EVAL_MAX_OPERATORS		10			// Operators stack depth
#define EVAL_MAX_OPERANDS			10			// Operands stack depth
#define EVAL_BUFFER_SIZE			15			// Analyze window width

#define EVAL_NB_OPERATORS 14
#define EVAL_MAX_OPERATOR_LENGTH	3
#define EVAL_EARLY_OPERATOR					(128)
#define EVAL_PRECEDENCE(x)					((x) & 127)

#define EVAL_OPERATOR_PRECEDENCE(x)	(((x).precedence) & 127)
#define EVAL_OPERATOR_IS_EARLY(x)		(((x).precedence) & 128)
// ----------------------------------------------------------------------



// ----------------------------------------------------------------------
// Types defines
typedef enum {
	EVAL_READY = 0,									// Ready to use for evaluation
	EVAL_COMPLETE = 1,							// Evaluation successfully completed
	EVAL_OPERAND_LIFO_OVF = 2,			// Operand stack overflow
	EVAL_OPERATOR_LIFO_OVF = 3,			// Operator stack overflow
	EVAL_RESULT_OVF = 4,						// Result overflow
	EVAL_PARSE_ERROR = 5,						// Block syntax error
	EVAL_INVALID_ARG = 6,						// Function argument is invalid
	EVAL_OPERAND_PARSE_ERROR = 7,		// Unable to resolve operand
	EVAL_BAD_OPERATOR = 8,
	EVAL_EMPTY_OPERAND = 9,					// Operand is empty
	EVAL_OPERATE_ERROR = 10,
	EVAL_DIV_BY_ZERO = 11,
	EVAL_BAD_REFERENCE = 12,
	EVAL_BUFFER_OVF = 13
} eval_status_t;

typedef int operand_t;
typedef uint8_t index_t;
typedef uint8_t buffer_index_t;
typedef eval_status_t (* reference_solver) (uint8_t* reference, uint8_t referenceSize, operand_t* value);
typedef struct num_eval {
	// Function pointer to resolve 'reference operator'
	reference_solver solveReference;
	
	struct operator{
		char symbol[EVAL_MAX_OPERATOR_LENGTH];
		uint8_t precedence;
		eval_status_t (* operate) (struct num_eval *stack);
	} *operators[EVAL_MAX_OPERATORS];
	index_t operatorIndex;
	
	operand_t operands[EVAL_MAX_OPERANDS];
	index_t operandIndex;
	
	uint8_t buffer[EVAL_BUFFER_SIZE];
	buffer_index_t bufferIndex;
	
	//operand_t result;
} num_eval_t; // 'Numeric Evaluation' structure
typedef struct operator operator_t;
typedef eval_status_t (* operator_function) (num_eval_t* stack);
// ----------------------------------------------------------------------



// ----------------------------------------------------------------------
// Function declarations
//void eval_init();
void clean_stack(num_eval_t* stack);
void set_reference_solver(num_eval_t* stack, reference_solver solver);
eval_status_t evaluate_block(num_eval_t* stack, uint8_t* block, uint8_t blockSize, operand_t* result);

operator_t* find_operator(uint8_t* block, uint8_t blockSize, uint8_t* lastComputedIndex);

int antoi(char* str, unsigned int size);
unsigned int intoa (int number, uint8_t* block, unsigned int blockSize, unsigned int blockIndex);
int axntoi(char* str, unsigned int size);

void eval_print_status(eval_status_t status);
void eval_print_stack(num_eval_t* stack);
// ----------------------------------------------------------------------

// Global variables
// Declare a stack to use with previous functions
#define CALCULATOR_STACK(name, referenceSolver)\
	num_eval_t name##_stack={referenceSolver}


// About operators
// ----------------------------------------------------------------------
// Function declarations
eval_status_t operator_logical_or (num_eval_t* stack);
eval_status_t operator_logical_and (num_eval_t* stack);
eval_status_t operator_equals (num_eval_t* stack);
eval_status_t operator_higher_than (num_eval_t* stack);
eval_status_t operator_lower_than (num_eval_t* stack);

eval_status_t operator_plus (num_eval_t* stack);
eval_status_t operator_minus (num_eval_t* stack);
eval_status_t operator_time (num_eval_t* stack);
eval_status_t operator_divide (num_eval_t* stack);
eval_status_t operator_modulo (num_eval_t* stack);
eval_status_t operator_not (num_eval_t* stack);
eval_status_t operator_reference (num_eval_t* stack);

eval_status_t operator_FSeparator (num_eval_t* stack);
eval_status_t operator_OSeparator (num_eval_t* stack);
// ----------------------------------------------------------------------
//*/

#endif // __EMMA_EVAL_H_INCLUDED__
