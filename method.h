/*
Copyright (c) 2009-2019, Martin Leadbeater
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef __ACTION_H__
#define __ACTION_H__

#include "symboltable.h"
#include "buffers.h"

enum action_type {
	NULL_ACTION,      /* do nothing */
	LOG_ACTION,       /* log a text string */
	SET_ACTION,       /* assign a text string to a variable */
	EXIT_ACTION,      /* exit the program immediately */
	RESTART_ACTION,   /* restart the system (may not be required) */
	CONTROL_ACTION,   /* not implemented */
	GENERIC_ACTION,   /* not implemented */
	SPAWN_ACTION,     /* spawn a background command based on supplied text string (io redirection is supported) */
	RUN_ACTION,        /* execute a command based on supplied text string 
	                      and store its output in the variable provided.  */
	EXECUTE_ACTION,    /* exec() a command based on supplied text string (io redirection is supported)  */
	CALL_ACTION,       /* load a dynamic library and call a plugin defined within it  */
	TRIM_ACTION,       /* trims trailing whitespace from a variable  */
	LINE_ACTION        /* selects one line from a variable  */
};

/* A method is a command identifier with optional parameters, which can 
    be retained for later execution.
 */


typedef struct method
{
	struct method *next;
	int kind;
	int id;
	char *action;
	char **params; /* deprecated */
    parameter_list parameters;
} method;

void init_actions();

void init_methods(symbol_table variables_table);

void release_all_methods();

void display_all_methods();

void add_method_parameters(method *m, parameter_list params);

void add_method_parameter(method *m, const char *param);

int create_method();

int execute_method(int id);

method *add_typed_action(int id, enum action_type kind, const char *action);

void add_action(int method_id, const char *action);

/* adds an action with a list of parameters. 

    The list must be NULL terminated
*/
void add_action_params(int method_id, const char *action, char **params);

method *add_log_action(int method_id, const char *data);

void add_line_action(int id, const char *var_name, const char *line);

method *add_assign_action(int id,const char *var_name, const char *value);

void add_execute_action(int method_id, const char *data);

method *add_call_action(int method_id, const char *data);

void add_run_action(int method_id, const char *var_name, const char *command);

void add_spawn_action(int method_id, const char *data);

void release_method(int method_id);

#endif
