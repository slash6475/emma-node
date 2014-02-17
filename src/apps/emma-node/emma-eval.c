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
#include "emma-eval.h"

#include <string.h>

#define LDEBUG 0
#if (LDEBUG | GDEBUG | EVAL_DEBUG)
	#define PRINT(...) OUTPUT_METHOD("[EMMA EVAL] " __VA_ARGS__)
	#define PRINTS(...) OUTPUT_METHOD(__VA_ARGS__)
#else
	#define PRINT(...)
	#define PRINTS(...)
#endif



int antoi(char* str, unsigned int size)
{
	int comp = 1;
	int val = 0;
	if (*str == '-') comp = -1;
	while ((size) && (*str))
	{
		if ((*str >= '0') && (*str <= '9'))
		{
			val *= 10;
			val += (*str - '0');
		}
		str++;
		size--;
	}
	return val*comp;
}

unsigned int intoa (int number, uint8_t* block, unsigned int blockSize, unsigned int blockIndex)
{
	// Variables
	unsigned int val = 0;
	int nbDigits = 0;
	int cnt = 0;
	
	// Secure inputs
	if (!block) return 0;
	
	// I) Compute total number of digits
	val = number;
	nbDigits = 0;
	if (val==0) {nbDigits = 1;}
	else {while(val>0){val/=10;nbDigits++;}}
	
	// Protect from too large sizes or too large indexes
	if ((blockIndex+blockSize) > nbDigits){
		if (blockIndex > nbDigits) blockSize=0;
		else blockSize=nbDigits-blockIndex;
	}
	
	// II) Extract right digits
	val=number;
	cnt=nbDigits;
	while(cnt>blockIndex){val/=10;cnt--;}
	
	// III) Suppress leftmost digits
	while (nbDigits > cnt) {val*=10;cnt++;}
	val = number - val;
	
	// IV) Suppress rightmost digits
	cnt = 0;
	while(cnt < (nbDigits - blockIndex - blockSize)){val/=10;cnt++;}
	
	// V) Convert val into characters
	if (blockSize) cnt = blockSize-1;
	else cnt=0;
	while (cnt > 0) {block[cnt]=val%10+'0';val/=10;cnt--;}
	block[cnt]=val%10+'0';
	
	//return cnt;
	return blockSize;
}

int axntoi(char* str, unsigned int size)
{
	int res = 0;
	while ((size) && (*str))
	{
		res = res << 4;
		if ((*str >= '0') && (*str <= '9'))
		{
			res |= (*str - '0');
		}
		else if ((*str >= 'a') && (*str <= 'f'))
		{
			res |= (*str - 'a' + 10);
		}
		else if ((*str >= 'A') && (*str <= 'F'))
		{
			res |= (*str - 'A' + 10);
		}
		else return 0;
		str++;
		size--;
	}
	return res;
}


// TODO: Intégrer (voire ré-implémenter) proprement les fonctionnalités suivantes dans l'architecture EMMA
//Passer en évaluation d'expressions post-fixées
//Généraliser la notion de flux par blocs à tous les modificateurs de données
//Moteur de REGEX ?
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------




//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------

#define MAX_IP_CHUNKS	8 // 8 for IPv6, 
uint8_t getIPaddress (char c, uip_ipaddr_t* address, uint16_t* port)
{
	//uint16_t cnt = 0;
	uint16_t cnt2 = 0;
	static uint8_t digitBuf[6];
	static uint8_t digitBufIndex = 0;
	static uint8_t addIndex = 0;
	static uint8_t addExtendIndex = 0;
	static uint8_t valid=0;
	
	if (!address) {valid=0;return EMMA_EVAL_INVALID;}
	
	//for (cnt=0 ; cnt<blockSize ; cnt++)
	{
		if (c == '[')
		{
			//PRINT("Start\n");
			digitBufIndex = 0;
			addIndex = 0;
			addExtendIndex = 0;
			if (port) *port = 0;
			for(cnt2=0 ; cnt2<MAX_IP_CHUNKS ; cnt2++){address->u16[cnt2]=UIP_HTONS(0);}
			valid=1;
		}
		else if (c == ':')
		{
			//PRINT("Separator\n");
			if (addIndex<MAX_IP_CHUNKS)
			{
				if (digitBufIndex==0) addExtendIndex = addIndex;
				address->u16[addIndex] = UIP_HTONS(axntoi((char*)digitBuf, digitBufIndex));
				addIndex++;
				digitBufIndex = 0;
			}
		}
		else if ((c>='0'&&c<='9') || (c>='a'&&c<='f') || (c>='A'&&c<='F'))
		{
			//PRINT("Digit\n");
			if (digitBufIndex<6) {digitBuf[digitBufIndex] = c;digitBufIndex++;}
			else {PRINT("Too many digits\n"); valid=0; return EMMA_EVAL_INVALID;}
		}
		else if (c == ']')
		{
			//PRINT("End\n");
			if (!valid) return EMMA_EVAL_INVALID;
			else valid=2;
			// Get Last element
			if (addIndex<MAX_IP_CHUNKS)
			{
				if (digitBufIndex==0) addExtendIndex = addIndex;
				address->u16[addIndex] = UIP_HTONS(axntoi((char*)digitBuf, digitBufIndex));
				addIndex++;
				digitBufIndex = 0;
			}
			
			// Expand address
			if (addIndex != MAX_IP_CHUNKS)
			{
				addIndex--;
				while (addIndex < 7)
				{
					cnt2 = addIndex;
					while(cnt2 >= addExtendIndex)
					{
						address->u16[cnt2 + 1] = address->u16[cnt2];
						cnt2--;
					}
					addIndex++;
				}
				addIndex = MAX_IP_CHUNKS;
			}
			if (!port && valid) {valid=0;return EMMA_EVAL_END;}
		}
		else if (c == '/' || c == '"')
		{
			//PRINT("Stop '/'\n");
			*port = UIP_HTONS(antoi((char*)digitBuf, digitBufIndex));
			if (valid == 2){valid=0;return EMMA_EVAL_END;}
			else {valid=0; return EMMA_EVAL_INVALID;}
		}
		else
		{
			//PRINT("Start 'unknown'\n");
			valid = 0;
			return EMMA_EVAL_INVALID;
		}
	}
	return EMMA_EVAL_MORE;
}




//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------

//*
#define MIN(x,y) (((x)>(y))?(y):(x))
#define MAX(x,y) (((x)<(y))?(y):(x))

operator_t operators[EVAL_NB_OPERATORS] = {
	{"||", 	EVAL_PRECEDENCE(10), 												operator_logical_or},
	{"&&", 	EVAL_PRECEDENCE(9), 												operator_logical_and},
	{"==", 	EVAL_PRECEDENCE(8), 												operator_equals},
	{">", 	EVAL_PRECEDENCE(7), 												operator_higher_than},
	{"<", 	EVAL_PRECEDENCE(6), 												operator_lower_than},
	{"+", 	EVAL_PRECEDENCE(5), 												operator_plus},
	{"-", 	EVAL_PRECEDENCE(5), 												operator_minus},
	{"/", 	EVAL_PRECEDENCE(4), 												operator_divide},
	{"%", 	EVAL_PRECEDENCE(4), 												operator_modulo},
	{"*", 	EVAL_PRECEDENCE(4), 												operator_time},
	{"!", 	EVAL_PRECEDENCE(3), 												operator_not},
	{".", 	EVAL_PRECEDENCE(2), 												operator_reference},
	{")", 	EVAL_EARLY_OPERATOR | EVAL_PRECEDENCE(1), 	operator_FSeparator},
	{"(", 	EVAL_EARLY_OPERATOR | EVAL_PRECEDENCE(1), 	operator_OSeparator}
};

// Functions

void clean_stack(num_eval_t* stack)
{
	PRINT("[CALCULATOR] Cleaning stack.\n");
	// Do not reset solveReference pointer
	stack->operatorIndex = 0;
	stack->operandIndex = 0;
	stack->bufferIndex = 0;
}

void set_reference_solver(num_eval_t* stack, reference_solver solver)
{
	if (stack != NULL)
	{
		PRINT("[CALCULATOR] Reference solver set.\n");
		stack->solveReference = solver;
	}
	else PRINT("[CALCULATOR] [WARNING] Reference solver not set.\n");
}

static eval_status_t pop_operator(num_eval_t* stack, operator_t** operator)
{
	if (stack != NULL)
	{
		if (stack->operatorIndex > 0)
		{
			stack->operatorIndex--;
			*operator = stack->operators[stack->operatorIndex];
			return EVAL_READY;
		}
		else return EVAL_OPERATOR_LIFO_OVF;
	}
	else return EVAL_INVALID_ARG;
}

static eval_status_t push_operator(num_eval_t* stack, operator_t* operator)
{
	if (stack != NULL)
	{
		if (stack->operatorIndex < EVAL_MAX_OPERATORS)
		{
			stack->operators[stack->operatorIndex] = operator;
			stack->operatorIndex++;
			return EVAL_READY;
		}
		else return EVAL_OPERATOR_LIFO_OVF;
	}
	else return EVAL_INVALID_ARG;
}

static eval_status_t pop_operand(num_eval_t* stack, operand_t* operand)
{
	if (stack != NULL)
	{
		if (stack->operandIndex > 0)
		{
			stack->operandIndex--;
			*operand = stack->operands[stack->operandIndex];
			return EVAL_READY;
		}
		else return EVAL_OPERAND_LIFO_OVF;
	}
	else return EVAL_INVALID_ARG;
}

static eval_status_t push_operand(num_eval_t* stack, operand_t operand)
{
	if (stack != NULL)
	{
		if (stack->operandIndex < EVAL_MAX_OPERANDS)
		{
			stack->operands[stack->operandIndex] = operand;
			stack->operandIndex++;
			return EVAL_READY;
		}
		else return EVAL_OPERAND_LIFO_OVF;
	}
	else return EVAL_INVALID_ARG;
}


eval_status_t solve_reference (char* strReference, operand_t* intReference, operand_t* value)
{
	if ((strReference != NULL) || (intReference != NULL))
	{
		if (strReference != NULL)
		{
			// rest_get_resource() ...
			// this = 0, other = 1
			if (strncmp(strReference, "this", 4) == 0) {*value = 0; return EVAL_COMPLETE;}
			else return EVAL_BAD_REFERENCE;
			//else if  rest_get_resource ... to set the good value
		}
		else
		{
			// Search value by reference ID (intReference)
			// Set result in *value
			*value = 0;
			return EVAL_BAD_REFERENCE;
		}
	}
	else return EVAL_INVALID_ARG;
}


// A NULL operator will tend to simplify stack (final computing)
eval_status_t eval_add_operator (num_eval_t* stack, operator_t* operator)
{
	// Variables
	eval_status_t error;
	operator_t* prevOperator = NULL;
	
	// Evaluate stack
	error = pop_operator(stack, &prevOperator);
	while (error == EVAL_READY)
	{
		if (((operator == NULL)?(1):(EVAL_OPERATOR_PRECEDENCE(*prevOperator) <= EVAL_OPERATOR_PRECEDENCE(*operator))) && (!EVAL_OPERATOR_IS_EARLY(*prevOperator)))
		{
			// Execute prev operator function
			if (prevOperator->operate != NULL)
			{
				error = prevOperator->operate(stack);
				if (error != EVAL_COMPLETE) return error;
			}
			else PRINT("No operation associated with operator '%s'.\n", prevOperator->symbol);
		}
		else
		{
			// Do re-push prevOperator
			push_operator(stack, prevOperator);
			break;
		}
		error = pop_operator(stack, &prevOperator);
	}
	
	// Push operator and evaluate if is early operator
	if (operator != NULL)
	{
		error = push_operator(stack, operator); // TODO vérifier erreurs
		if (error != EVAL_READY) return error;
		if (EVAL_OPERATOR_IS_EARLY(*operator))
		{
			if (operator->operate != NULL)
			{
				error = operator->operate(stack);
				if (error != EVAL_COMPLETE) return error;
			}
			else PRINT("No operation associated with operator '%s'.\n", operator->symbol);
		}
	}
		
	return EVAL_READY;
}

eval_status_t eval_add_operand (num_eval_t* stack, uint8_t* block, uint8_t blockSize)
{
	// Variable
	operand_t newOperand = 0;
	
	if (stack == NULL) return EVAL_INVALID_ARG;
	
	// Evaluate and push operand
	if (((block != NULL) && (blockSize != 0)) && (block[0] != '\0'))
	{
		if (((block[0] < '0') || (block[0] > '9')) && (block[0] != '-'))
		{
			// Get associated number from list
			if (stack->solveReference != NULL)
			{
				 if (stack->solveReference(block, blockSize, &newOperand) != EVAL_COMPLETE) return EVAL_OPERAND_PARSE_ERROR;
			}
			else
			{
				PRINT("[CALCULATOR][ERROR] No reference solver.\n");
				return EVAL_OPERAND_PARSE_ERROR;
			}
		}
		else {newOperand = (operand_t)antoi((char*)block, (unsigned int)blockSize);}
	}
	else return EVAL_EMPTY_OPERAND;
	
	PRINT("[CALCULATOR] Operand evaluated as '%d'.\n", newOperand);

	if (push_operand(stack, newOperand) != EVAL_READY) return EVAL_OPERAND_LIFO_OVF;
	else return EVAL_READY;
}

eval_status_t evaluate_block(num_eval_t* stack, uint8_t* block, uint8_t blockSize, operand_t* result)
{
	// Variables
	uint8_t cnt = 0;
	uint8_t cnt2 = 0;
	uint8_t increment = 0;
	uint8_t operatorLength = 0;
	
	static uint8_t operandIndex = 0;
	
	eval_status_t error = EVAL_READY;
	if (stack == NULL) {operandIndex=0; return EVAL_EMPTY_OPERAND;}

	// --------------------------------------
	// Evaluate block character by character
	cnt = 0;
	while (cnt < blockSize)
	{
		// Fill buffer with block content, except '\0' character (transformed in '0')
		PRINT("[CALCULATOR] Copying to buffer (operandIndex = %d) () ... \n", operandIndex);
		cnt2 = stack->bufferIndex - operandIndex;
		operandIndex = 0; // Reset operandIndex to detect "Out Of Buffer" elements (operand or operator)
		while((cnt2 < EVAL_BUFFER_SIZE) && (cnt < blockSize))
		{
			if (block[cnt] == '\0') {
				stack->buffer[cnt2] = '0';
				cnt++;
				cnt2++;
			}
			else if (block[cnt] == ' ') cnt++;
			else {
				stack->buffer[cnt2] = block[cnt];
				cnt2++;
				cnt++;
			}
		}
		PRINT("\n");
		PRINT("End of copy (cnt2=%d) (blockSize=%d)\n", cnt2, blockSize);
		if ((cnt >= blockSize) && (cnt2 < EVAL_BUFFER_SIZE)) stack->buffer[cnt2] = '\0';
		
		cnt2=0;
		PRINT("[CALCULATOR] Buffer: ");
		while ((cnt2 < EVAL_BUFFER_SIZE) && (stack->buffer[cnt2] != '\0'))
		{
			PRINT("%c-", stack->buffer[cnt2]);
			cnt2++;
		}
		PRINT("\n\n");
		
		// Reset buffer index
		stack->bufferIndex = 0;
		
		PRINT("[CALCULATOR] Evaluating buffer ...\n");
		while ((stack->bufferIndex < EVAL_BUFFER_SIZE) && (stack->buffer[stack->bufferIndex] != '\0'))
		{
			increment = 1;
			PRINT("[CALCULATOR]      Watching '%c'.\n", stack->buffer[stack->bufferIndex]);
			// Is it an operator ?
			for (cnt2=0 ; cnt2<EVAL_NB_OPERATORS ; cnt2++)
			{
				operatorLength = strlen(operators[cnt2].symbol);
				if (strncmp((char*)(stack->buffer + stack->bufferIndex), operators[cnt2].symbol, operatorLength) == 0)
				{
					PRINT("[CALCULATOR] Operator '%s' found.\n", operators[cnt2].symbol);
					
					// We have found an operator
					PRINT("[CALCULATOR] Computing operand ...\n");
					// Non-empty operand
					error = eval_add_operand (stack, (stack->buffer + operandIndex), (stack->bufferIndex - operandIndex));
					if ((error != EVAL_READY) && (error != EVAL_EMPTY_OPERAND)) {operandIndex=0; return error;}
					
					PRINT("[CALCULATOR] Computing operator ...\n");
					error = eval_add_operator (stack, &(operators[cnt2]));
					if (error != EVAL_READY) {operandIndex=0; printf("ERROR:%d \n", error); return error;}
					PRINT("\n");
				
					// Increment
					operandIndex = stack->bufferIndex + operatorLength;
					increment = operatorLength;
				}
			}
		
			stack->bufferIndex += increment;
		}
		PRINT("[CALCULATOR] ... Done.\n");
		
		// No operator has been found in the whole buffer
		if (operandIndex == 0)
		{
			PRINT("[WARNING] No operator found in whole buffer.\n");
			if (stack->bufferIndex >= (EVAL_BUFFER_SIZE - 1))
			{
				// Try to evaluate it as an operand
				error = eval_add_operand (stack, stack->buffer, EVAL_BUFFER_SIZE - 1);
				if (error != EVAL_READY) {operandIndex=0; return EVAL_BUFFER_OVF;}
				// else we can continue to evaluate input block ...
				operandIndex = stack->bufferIndex;
			}
		}
		
		// Move trailing bytes to the start of buffer
		cnt2=operandIndex;
		while (cnt2 < stack->bufferIndex)
		{
			stack->buffer[cnt2-operandIndex] = stack->buffer[cnt2];
			cnt2++;
		}
	}
	
	// --------------------------------------
	// If it was the last turn, conclude on result
	if (result != NULL)
	{
		PRINT("[CALCULATOR] Last turn (operandIndex=%d) (bufferIndex=%d).\n", operandIndex, stack->bufferIndex);
		if (operandIndex < stack->bufferIndex)
		{
			// The last element was an operand
			error = eval_add_operand (stack, (stack->buffer + operandIndex), (stack->bufferIndex - operandIndex));
			if (error != EVAL_READY) {
				operandIndex=0; 
				PRINT("PLOP\n");
				return error;
			}
		}
		
		// Reset operand index: block-wise evalution finished
		operandIndex = 0;
		
		// Try to simplify actual stack
		error = eval_add_operator(stack, NULL);
		if (error != EVAL_READY) {
			operandIndex=0; 
			PRINT("PLOP2 %d\n", error);
			return error;
		}
		else
		{
			if (pop_operand(stack, result) == EVAL_READY)
			{
				// Remaining operand in stack at the end of evaluation is a parse error
				if (pop_operand(stack, result) == EVAL_OPERAND_LIFO_OVF)
				{
					clean_stack(stack);
					{
						operandIndex=0;
					PRINT("PLOP3\n"); 
					return EVAL_COMPLETE;
					}
				}
				else {
					operandIndex=0;
					PRINT("PLOP4\n"); 
					return EVAL_PARSE_ERROR;
				}
			}
			else {
				operandIndex=0;
				PRINT("PLOP5\n"); 
				return EVAL_PARSE_ERROR;
			}
		}
		
		// Clean the stack
		clean_stack(stack);
	}
	return EVAL_READY;
}

operator_t* find_operator(uint8_t* block, uint8_t blockSize, uint8_t* lastComputedIndex)
{
	uint8_t cnt = 0;
	uint8_t cnt2 = 0;
	if (lastComputedIndex == NULL) return 0;
	while(cnt < blockSize)
	{
		// Is it an operator ?
		for (cnt2=0 ; cnt2<EVAL_NB_OPERATORS ; cnt2++)
		{
			if (strncmp((char*)(block + cnt), operators[cnt2].symbol, strlen(operators[cnt2].symbol)) == 0)
			{
				PRINT("[CALCULATOR] Operator '%s' found.\n", operators[cnt2].symbol);
			
				// Increment
				//increment = strlen(operators[cnt2].symbol);
				//operandIndex = stack->bufferIndex + increment;
				
				*lastComputedIndex = cnt;// + strlen(operators[cnt2].symbol); // or last operand index !!
				return operators+cnt2;
			}
		}
		cnt++;
	}
	
	// No operator found in whole buffer
	// Return 0 and set lastComputedIndex to (blockSize - maxOperatorLength) (-1 ?)
	*lastComputedIndex = blockSize;
	return NULL;
}







void eval_print_status (eval_status_t status)
{
	if (status == EVAL_READY) 						{PRINT("[STATUS] EVAL_READY.\n");}
	else if (status == EVAL_COMPLETE) 				{PRINT("[STATUS] EVAL_COMPLETE.\n");}
	else if (status == EVAL_OPERAND_LIFO_OVF) 		{PRINT("[STATUS] EVAL_COMPLETE.\n");}
	else if (status == EVAL_OPERATOR_LIFO_OVF) 		{PRINT("[STATUS] EVAL_OPERATOR_LIFO_OVF.\n");}
	else if (status == EVAL_RESULT_OVF) 			{PRINT("[STATUS] EVAL_RESULT_OVF.\n");}
	else if (status == EVAL_PARSE_ERROR) 			{PRINT("[STATUS] EVAL_PARSE_ERROR.\n");}
	else if (status == EVAL_INVALID_ARG) 			{PRINT("[STATUS] EVAL_INVALID_ARG.\n");}
	else if (status == EVAL_OPERAND_PARSE_ERROR) 	{PRINT("[STATUS] EVAL_OPERAND_PARSE_ERROR.\n");}
	else if (status == EVAL_BAD_OPERATOR) 			{PRINT("[ERROR] EVAL_BAD_OPERATOR.\n");}
	else if (status == EVAL_EMPTY_OPERAND) 			{PRINT("[ERROR] EVAL_EMPTY_OPERAND.\n");}
	else if (status == EVAL_OPERATE_ERROR) 			{PRINT("[ERROR] EVAL_OPERATE_ERROR.\n");}
	else if (status == EVAL_DIV_BY_ZERO) 			{PRINT("[ERROR] EVAL_DIV_BY_ZERO.\n");}
	else 											{PRINT("[STATUS] Unknown status code %d.\n", status);}
}
void eval_print_stack(num_eval_t* stack)
{
	// Variables
	int maxIndex = MAX(stack->operatorIndex, stack->operandIndex);
	int commonIndex = 0;
	if (stack != NULL)
	{
		PRINT("\n\nOperators:\t\tOperands:\n");
		for (commonIndex=0 ; commonIndex<maxIndex ; commonIndex++)
		{
			if (stack->operatorIndex > commonIndex) PRINT("%s (%d)\t\t\t", (stack->operators[commonIndex])->symbol, (stack->operators[commonIndex])->precedence);
			else PRINT("\t\t\t");
			if (stack->operandIndex > commonIndex) PRINT("%d\n", stack->operands[commonIndex]);
			else PRINT("\n");
		}
		PRINT("\n");
	}
	else PRINT("[STACK] No input stack, argument is NULL.\n");
}





// Default calculator operators

eval_status_t operator_logical_or (num_eval_t* stack)
{
	// Variables
	operand_t op1, op2;
	
	if (stack != NULL)
	{
		if (pop_operand(stack, &op1) != EVAL_READY) return EVAL_OPERATE_ERROR;
		if (pop_operand(stack, &op2) != EVAL_READY) return EVAL_OPERATE_ERROR;
		PRINT("Operating '%d||%d' ...\n", op2, op1);
		if (push_operand(stack, (op2 || op1)) != EVAL_READY) return EVAL_OPERATE_ERROR;
		return EVAL_COMPLETE;
	}
	else return EVAL_INVALID_ARG;
}

eval_status_t operator_logical_and (num_eval_t* stack)
{
	// Variables
	operand_t op1, op2;
	
	if (stack != NULL)
	{
		if (pop_operand(stack, &op1) != EVAL_READY) return EVAL_OPERATE_ERROR;
		if (pop_operand(stack, &op2) != EVAL_READY) return EVAL_OPERATE_ERROR;
		PRINT("Operating '%d&&%d' ...\n", op2, op1);
		if (push_operand(stack, (op2 && op1)) != EVAL_READY) return EVAL_OPERATE_ERROR;
		return EVAL_COMPLETE;
	}
	else return EVAL_INVALID_ARG;
}

eval_status_t operator_equals (num_eval_t* stack)
{
	// Variables
	operand_t op1, op2;
	
	if (stack != NULL)
	{
		if (pop_operand(stack, &op1) != EVAL_READY) return EVAL_OPERATE_ERROR;
		if (pop_operand(stack, &op2) != EVAL_READY) return EVAL_OPERATE_ERROR;
		PRINT("Operating '%d==%d' ...\n", op2, op1);
		if (push_operand(stack, (op2 == op1)) != EVAL_READY) return EVAL_OPERATE_ERROR;
		return EVAL_COMPLETE;
	}
	else return EVAL_INVALID_ARG;
}

eval_status_t operator_higher_than (num_eval_t* stack)
{
	// Variables
	operand_t op1, op2;
	
	if (stack != NULL)
	{
		if (pop_operand(stack, &op1) != EVAL_READY) return EVAL_OPERATE_ERROR;
		if (pop_operand(stack, &op2) != EVAL_READY) return EVAL_OPERATE_ERROR;
		PRINT("Operating '%d>%d' ...\n", op2, op1);
		if (push_operand(stack, (op2 > op1)) != EVAL_READY) return EVAL_OPERATE_ERROR;
		return EVAL_COMPLETE;
	}
	else return EVAL_INVALID_ARG;
}

eval_status_t operator_lower_than (num_eval_t* stack)
{
	// Variables
	operand_t op1, op2;
	
	if (stack != NULL)
	{
		if (pop_operand(stack, &op1) != EVAL_READY) return EVAL_OPERATE_ERROR;
		if (pop_operand(stack, &op2) != EVAL_READY) return EVAL_OPERATE_ERROR;
		PRINT("Operating '%d<%d' ...\n", op2, op1);
		if (push_operand(stack, (op2 < op1)) != EVAL_READY) return EVAL_OPERATE_ERROR;
		return EVAL_COMPLETE;
	}
	else return EVAL_INVALID_ARG;
}

eval_status_t operator_plus (num_eval_t* stack)
{
	// Variables
	operand_t op1, op2;
	
	if (stack != NULL)
	{
		if (pop_operand(stack, &op1) != EVAL_READY) return EVAL_OPERATE_ERROR;
		if (pop_operand(stack, &op2) != EVAL_READY) return EVAL_OPERATE_ERROR;
		PRINT("Operating '%d+%d' ...\n", op2, op1);
		if (push_operand(stack, (op1 + op2)) != EVAL_READY) return EVAL_OPERATE_ERROR;
		return EVAL_COMPLETE;
	}
	else return EVAL_INVALID_ARG;
}

eval_status_t operator_minus (num_eval_t* stack)
{
	// Variables
	operand_t op1, op2;
	
	if (stack != NULL)
	{
		if (pop_operand(stack, &op1) != EVAL_READY) return EVAL_OPERATE_ERROR;
		if (pop_operand(stack, &op2) != EVAL_READY) return EVAL_OPERATE_ERROR;
		PRINT("Operating '%d-%d' ...\n", op2, op1);
		if (push_operand(stack, (op2 - op1)) != EVAL_READY) return EVAL_OPERATE_ERROR;
		return EVAL_COMPLETE;
	}
	else return EVAL_INVALID_ARG;
}

eval_status_t operator_time (num_eval_t* stack)
{
	// Variables
	operand_t op1, op2;
	
	if (stack != NULL)
	{
		if (pop_operand(stack, &op1) != EVAL_READY) return EVAL_OPERATE_ERROR;
		if (pop_operand(stack, &op2) != EVAL_READY) return EVAL_OPERATE_ERROR;
		PRINT("Operating '%d*%d' ...\n", op2, op1);
		if (push_operand(stack, (op1 * op2)) != EVAL_READY) return EVAL_OPERATE_ERROR;
		return EVAL_COMPLETE;
	}
	else return EVAL_INVALID_ARG;
}

eval_status_t operator_divide (num_eval_t* stack)
{
	// Variables
	operand_t op1, op2;
	
	if (stack != NULL)
	{
		if (pop_operand(stack, &op1) != EVAL_READY) return EVAL_OPERATE_ERROR;
		if (pop_operand(stack, &op2) != EVAL_READY) return EVAL_OPERATE_ERROR;
		if (op1 == 0) return EVAL_DIV_BY_ZERO;
		PRINT("Operating '%d/%d' ...\n", op2, op1);
		if (push_operand(stack, (op2 / op1)) != EVAL_READY) return EVAL_OPERATE_ERROR;
		return EVAL_COMPLETE;
	}
	else return EVAL_INVALID_ARG;
}

eval_status_t operator_modulo (num_eval_t* stack)
{
	// Variables
	operand_t op1, op2;
	
	if (stack != NULL)
	{
		if (pop_operand(stack, &op1) != EVAL_READY) return EVAL_OPERATE_ERROR;
		if (pop_operand(stack, &op2) != EVAL_READY) return EVAL_OPERATE_ERROR;
		PRINT("Operating '%d%%%d' ...\n", op2, op1);
		if (push_operand(stack, (op2 % op1)) != EVAL_READY) return EVAL_OPERATE_ERROR;
		return EVAL_COMPLETE;
	}
	else return EVAL_INVALID_ARG;
}

eval_status_t operator_not (num_eval_t* stack)
{
	// Variables
	operand_t op1;
	
	if (stack != NULL)
	{
		if (pop_operand(stack, &op1) != EVAL_READY) return EVAL_OPERATE_ERROR;
		PRINT("Operating '!%d' ...\n", op1);
		if (push_operand(stack, (! op1)) != EVAL_READY) return EVAL_OPERATE_ERROR;
		return EVAL_COMPLETE;
	}
	else return EVAL_INVALID_ARG;
}

eval_status_t operator_reference (num_eval_t* stack)
{
	// Variables
	//operand_t op1, op2;
	
	if (stack != NULL)
	{
		return EVAL_OPERATE_ERROR;
	}
	else return EVAL_INVALID_ARG;
}

eval_status_t operator_OSeparator (num_eval_t* stack)
{
	// Nothing to do here.
	if (stack != NULL)
	{
		return EVAL_COMPLETE;
	}
	else return EVAL_INVALID_ARG;
}

eval_status_t operator_FSeparator (num_eval_t* stack)
{
	// Variables
	operator_t* op = NULL;
	eval_status_t error = EVAL_COMPLETE;
	
	if (stack != NULL)
	{
		// Pop itself (to avoid recursive calls)
		pop_operator(stack, &op);
		
		// Unstack til' the first '(' operator is found
		error = pop_operator(stack, &op);
		while ((error == EVAL_READY) && ((op->symbol[0]!='(') || (op->symbol[1]!='\0')))
		{
			// Run operators
			if ((op->operate != NULL)) op->operate(stack);
			else PRINT("[FSeparator] No operation associated with operator '%s'.\n", op->symbol);
			error = pop_operator(stack, &op);
		}
		if (error != EVAL_READY) return EVAL_PARSE_ERROR;
		else return EVAL_COMPLETE;
	}
	else return EVAL_INVALID_ARG;
}
//*/





