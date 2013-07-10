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
 
#ifndef __JSONNPARSE_H_INCLUDED__
#define __JSONNPARSE_H_INCLUDED__

#include <stdint.h>

typedef enum {
	JSON_OBJ_START,
	JSON_OBJ_END,
	JSON_ARR_START,
	JSON_ARR_END,
	JSON_STR_START,
	JSON_STR_END,
	JSON_SEPARATOR,
	JSON_PAIR_SEPARATOR,
	JSON_CONTENT,
	JSON_BEGIN,
	JSON_ERROR
} json_state_t;

enum {
	JSON_NAME,
	JSON_VALUE,
	JSON_OTHER
};

typedef uint8_t depth_t;

typedef struct JSONParser {
	depth_t objDepth;
	depth_t arrDepth;
	depth_t strDepth;
	//uint8_t name_value;
} json_parser_t;



void JSONnParse_clean(json_parser_t* parser);
json_state_t JSONnParse(json_parser_t* parser, char c, depth_t depth);
uint8_t JSONnParse_check(json_parser_t* parser);
uint8_t JSONnParse_pair(json_parser_t* parser);
char* JSONnParse_print(json_state_t state);

#endif // __JSONNPARSE_H_INCLUDED__
