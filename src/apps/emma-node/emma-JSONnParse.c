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
#include "emma-JSONnParse.h"

#define LDEBUG 0
#if (LDEBUG | GDEBUG)
	#define PRINT(...) OUTPUT_METHOD("[JSONnParse] " __VA_ARGS__)
	#define PRINTS(...) OUTPUT_METHOD(__VA_ARGS__)
#else
	#define PRINT(...)
	#define PRINTS(...)
#endif

//TODO: Re-code a safe algorithm (this one as bugs on some cases)

void JSONnParse_clean(json_parser_t* parser)
{
	parser->objDepth = 0;
	parser->arrDepth = 0;
	parser->strDepth = 0;
	//parser->name_value = JSON_OTHER;
}

json_state_t JSONnParse(json_parser_t* parser, char c, depth_t depth)
{
	json_state_t ret = JSON_ERROR;
	if ((parser->strDepth) && (c != '"')) {ret = JSON_CONTENT;}
	else
	{
		if (c == '"')
		{
			parser->strDepth = ! parser->strDepth;
			if (parser->strDepth) ret = JSON_STR_START;
			else ret = JSON_STR_END;
		}
		else if (c == '[')
		{
			parser->arrDepth++;
			ret = JSON_ARR_START;
		}
		else if (c == ']')
		{
			if (parser->arrDepth) {parser->arrDepth--;ret = JSON_ARR_END;}
			//else ret = JSON_ERROR;
		}
		else if (c == '{')
		{
			parser->objDepth++;
			ret = JSON_OBJ_START;
			//return JSON_OBJ_START;
		}
		else if (c == '}')
		{
			if (parser->objDepth) {parser->objDepth--;ret = JSON_OBJ_END;}
			//else ret = JSON_ERROR;
		}
		else if (c == ':')
		{
			ret = JSON_PAIR_SEPARATOR;
		}
		else if ((c == ',') || (c == ' '))
		{
			ret = JSON_SEPARATOR;
		}
		else
		{
			ret = JSON_CONTENT;
		}
		
		if ((parser->objDepth + parser->arrDepth) > depth){ret = JSON_CONTENT;}
		if (((parser->objDepth + parser->arrDepth) == depth+1) && (c == '{')){ret = JSON_OBJ_START;}
		if (((parser->objDepth + parser->arrDepth) == depth+1) && (c == '[')){ret = JSON_ARR_START;}
		
		/*if (parser->objDepth > 0)
		{
			if (ret == JSON_PAIR_SEPARATOR) parser->name_value = JSON_VALUE;
			else if (ret == JSON_SEPARATOR) parser->name_value = JSON_NAME;
			else if (ret == JSON_OBJ_START) parser->name_value = JSON_NAME;
			else if (ret == JSON_OBJ_END) parser->name_value = JSON_OTHER;
		}*/
	}
	return ret;
}

uint8_t JSONnParse_check(json_parser_t* parser)
{
	if (!parser->objDepth && !parser->arrDepth && !parser->strDepth) return 1;
	else return 0;
}

/*uint8_t JSONnParse_pair(json_parser_t* parser)
{
	return parser->name_value;
}*/

char* JSONnParse_print(json_state_t state)
{
	if (state == JSON_OBJ_START){return "[JSONnParse] START OF OBJECT";}
	else if (state == JSON_OBJ_END){return "[JSONnParse] END OF OBJECT";}
	else if (state == JSON_ARR_START){return "[JSONnParse] START OF ARRAY";}
	else if (state == JSON_ARR_END){return "[JSONnParse] END OF ARRAY";}
	else if (state == JSON_STR_START){return "[JSONnParse] START OF STRING";}
	else if (state == JSON_STR_END){return "[JSONnParse] END OF STRING";}
	else if (state == JSON_SEPARATOR){return "[JSONnParse] SEPARATOR";}
	else if (state == JSON_PAIR_SEPARATOR){return "[JSONnParse] PAIR SEPARATOR";}
	else if (state == JSON_CONTENT){return "[JSONnParse] CONTENT";}
	else if (state == JSON_BEGIN){return "[JSONnParse] BEGIN";}
	else if (state == JSON_ERROR){return "[JSONnParse] PARSE ERROR";}
	else{return "[JSONnParse] UNKNOWN";}
}
