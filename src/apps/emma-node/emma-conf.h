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
 
#ifndef __EMMA_CONF_H_INCLUDED__
#define __EMMA_CONF_H_INCLUDED__

#define TRUE	1
#define FALSE	0
#define DYNAMIC 1
#define STATIC  0

#include "contiki.h"

#define WATCHDOG_CONF_TIMEOUT -1

/* Define the way memory is used */
#define MEMORY STATIC

/* Global debug definition */
#define GDEBUG TRUE

/* Process debug level */
//#define PROCESS_DEBUG TRUE

/* Client debug level */
#define CLIENT_DEBUG FALSE

/* Client server level */
#define SERVER_DEBUG FALSE

/* Resource debug level */
#define RESOURCE_DEBUG FALSE

/* Evaluator debug level */
#define EVAL_DEBUG FALSE

/* Local-link debug level */
//#define LO_DEBUG TRUE

/* Define standard output method */
#include <stdio.h>
#if defined(__AVR__)
	#include <avr/pgmspace.h>
	#include <avr/io.h>
	#define OUTPUT_METHOD(FORMAT,args...) printf_P(PSTR(FORMAT),##args)
#else // defined(__AVR__)
	#define OUTPUT_METHOD(...) printf(__VA_ARGS__)
#endif // defined(__AVR__)

/* Machine-dependent definitions */
//#include <stdint.h>

/* EMMA Constants */
#define EMMA_MAX_RESOURCES 18 		//(3*5 + 3roots)
#define EMMA_MAX_URI_SIZE  32 		// (4*8 <=> depth of 4)
#define	EMMA_NOBODY_ID		0		// Nobody process ID
#define EMMA_SERVER_ID		1		// Process ID used to specify the resource modifier
#define EMMA_CLIENT_ID		2		// Process ID used to specify the resource modifier
#define EMMA_SYSTEM_ID      3       // Process ID used to specify the resource modifier

#define EMMA_SERVER_TIMEOUT_SECOND 		(COAP_RESPONSE_TIMEOUT * COAP_MAX_RETRANSMIT)
#define EMMA_CLIENT_POLLING_INTERVAL	5
#define COAP_MAX_ATTEMPTS             	4

/* Tests on configuration (do not modify) */
#if GDEBUG | CLIENT_DEBUG | SERVER_DEBUG | RESOURCE_DEBUG
	#ifndef OUTPUT_METHOD
		#error No OUTPUT method specified.
	#endif
#else
	#ifdef OUTPUT_METHOD
		#undef OUTPUT_METHOD
	#endif
	#define OUTPUT_METHOD(...)
#endif

#ifndef NULL
	#define NULL 0
#endif

#endif // __EMMA_CONF_H_INCLUDED__
