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
 
#ifndef __EMMA_CLIENT_H_INCLUDED__
#define __EMMA_CLIENT_H_INCLUDED__

enum {
	EMMA_CLIENT_WAITING_MUTEX,
	EMMA_CLIENT_RUNNING,
	EMMA_CLIENT_IDLE,
	EMMA_CLIENT_START,
	EMMA_CLIENT_RELEASE
};

uint8_t emma_client_state();

void emma_client_init();

#endif // __EMMA_CLIENT_H_INCLUDED__
