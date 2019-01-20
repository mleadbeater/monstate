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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "y.tab.h"
#include "symboltable.h"
#include "condition.h"
#include "regular_expressions.h"
#include "symboltable.h"
#include "options.h"
#include "property.h"
#include "plugin.h"

/*
condition_function socket_script;
condition_function read_file;
*/

/*

Two optimisations would be nice for this module:

   - reuse similar tests (ie a test will need to be shared by multiple states
   - short circuit evaluation
  
Currently each condition has a single set and there is a single list of, 
conditions. There are two obvious ways to change the program; have each 
condition maintain a list of sets it is part of or build a list of 
condition sets which link to shared condition items. Since many operations
are based on condition sets, the latter seems appropriate.

*/

typedef struct condition
{
	struct condition *next;
	int set;
	char *test;
	int operation;
	char *check;
	rexp_info *rexp;
    parameter_list parameters;
} condition;

condition *condition_table = NULL;
static int num_entries;

static int condition_set_number;

/* a condition set does not need to be in a 'states' symbol table
    it is convenient, however, to use the fact it is to make our
    verbose reporting a little easier to comprehend 
*/
extern symbol_table states;

void init_conditions()
{
	condition_set_number = 0;
	condition_table = NULL;
	num_entries = 0;
}

void release_all_conditions()
{
	condition *curr = condition_table;
	while (curr != NULL) 
	{
		condition_table = condition_table->next;
		free(curr->test);
		free(curr->check);
		if (curr->rexp != NULL)
			release_pattern(curr->rexp);
        if (curr->parameters)
            free_parameter_list(curr->parameters);
		free(curr);
		curr = condition_table;
	}
	init_conditions();
}

const char *op_name(int op)
{
    if (op == LE)
        return "<=";
    else if (op == LT)
        return "<";
    else if (op == GE)
        return ">=";
    else if (op == GT)
        return ">";
    else if (op == NE)
        return "!=";
    else if (op == MATCHES)
        return "~";
    else if (op == NOT_MATCHES)
        return "!~";
    else if (op == EQ)
        return "==";
    else return "";
}

void display_all_conditions()
{
	condition *curr = condition_table;
	while (curr != NULL) 
	{
        const char *op = op_name(curr->operation);
        if (op && *op)
		   printf("%d: %s %s", curr->set, curr->test, op);
        else
		   printf("%d: %s %d", curr->set, curr->test, curr->operation);
		if (curr->rexp && (curr->operation == MATCHES || curr->operation == NOT_MATCHES) ) 
		{
			printf(" pattern: %s is %scompiled\n", curr->rexp->pattern, (curr->rexp->compilation_result != 0) ? "not" : "");
		}
		else
			printf(" %s\n", curr->check);
		curr = curr->next;
	}
}

int create_condition_set()
{
	int result = condition_set_number;
	condition_set_number++;
	return result;
}

void add_condition(int set, const char *test, int op, const char *check, parameter_list params)
{
	/*
	 Conditions can be tested in any order except that conditions 
	 which collect data into symbols must be evaluated first. 
	 We push those to the head of the condition list.
	*/
	condition *new_condition = malloc(sizeof(struct condition));
	if (op == ASSIGNED)
	{
		new_condition->next = condition_table;
		condition_table = new_condition;
	}
	else
	{
		condition *next_pos = condition_table;
		condition *last_pos = next_pos;
		while (next_pos != NULL) {
			last_pos = next_pos;
			next_pos = next_pos->next;
		}
		if (last_pos) {
			last_pos->next = new_condition;
		}
		else
			condition_table = new_condition;
		new_condition->next = NULL;
	}
	new_condition->set = set;
	new_condition->test = strdup(test);
	new_condition->operation = op;
    new_condition->parameters = params;
	if (check != NULL) {
		new_condition->check = strdup(check);
        if (op == MATCHES || op == NOT_MATCHES)
            new_condition->rexp = create_pattern(check);
        else 
            new_condition->rexp = NULL;
	}
	else {
		new_condition->check = strdup("");
		new_condition->rexp = NULL;
	}
	num_entries++;
}

long integer_value(symbol_table variables, const char *str)
{
	char *err_char = NULL;
	long result = strtol(str, &err_char, 10);
	/* if the string is not a number, perhaps the caller gave us a variable name */
	if (*err_char != 0) {
		if (verbose())
			printf("parsing integer value; looking for a variable called '%s'\n", str);
		return get_integer_value(variables, str);
	}
	return result;
}

int perform_integer_compare(int a, int op, int b)
{
	switch(op)
	{
		case EQ:  
			if (a == b) return 1;
			break;
		case LE:  
			if (a <= b) return 1;
			break;
		case LT:  
			if (a < b) return 1;
			break;
		case GE:  
			if (a >= b) return 1;
			break;
		case GT:  
			if (a > b) return 1;
			break;
		case NE:  
			if (a != b) return 1;
			break;
	}
	return 0;	
}

int is_comparison_op(int op)
{
	switch(op)
	{
		case EQ:  
		case LE:  
		case LT:  
		case GE:  
		case GT:  
		case NE:
			return 1;
			break;
	}
	return 0;
}

const char *string_value(symbol_table variables, const char *str)
{
	const char *value = get_string_value(variables, str);
	if (value)
		return value;
	return str;
}

int perform_string_compare(const char *s1, int op, const char * s2)
{
	int res = strcmp(s1, s2);
	return perform_integer_compare(res, op, 0);
}

static char * collect_data(symbol_table variables, const char *command_string)
{
	char *buf = NULL;
    const char *start_p = command_string;
    while (start_p && *start_p && isspace(*start_p)) start_p++;
#if 0
	int buf_size = 1024;
    /* the FILE and SCRIPT conditions are now implemented via plugins */
    
	if (strncmp(command_string, "FILE ", 5) == 0)
    {
        const char *args[] = { "FILE", "", 0 };
        int argc = 2;
        int res;
        args[1] = command_string+5;
        buf = malloc(buf_size);
        res = read_file(variables, buf, buf_size, argc, args);
        if (res == -1) {
            free(buf);
            return NULL;
        }
    }
    else if (strncmp(command_string, "SCRIPT ", 7) == 0)
    {
        const char *args[] = { "SCRIPT", "", "",  0};
        int argc = 3;
        int tmp;
        char *property_name = (char *)command_string + 7;
        char *command = strchr(property_name, ' ');
        buf = malloc(buf_size);

        if (command == NULL)
        {
            fprintf(stderr, "error: script usage:  SCRIPT PropertyName command_string");
            free(buf);
            return NULL;
        }
        tmp = command - property_name;
        property_name = malloc(tmp+1);
        memcpy(property_name, command_string + 7, tmp);
        property_name[tmp] = 0;
    
        args[1] = property_name;
        args[2] = command;
    
        tmp = socket_script(variables, buf, buf_size, argc, args);
    
        free(property_name); 

        if (tmp == -1) 
        {
            free(buf);
            return NULL;
        }
    }
    else 
#endif
    if (strncmp(start_p, "CALL ", 5) == 0)
    {
        const char *command = start_p+5;
        int plugin_result = plugin(variables, command, NULL);
        set_integer_value(variables, "RESULT_STATUS", plugin_result);
        if (plugin_result == PLUGIN_COMPLETED)
        {
            const char *data = get_string_value(variables, "RESULT");
            if (data != NULL)
                buf = strdup(data);
 			/* Note: null data from an explicit plugin call is a failure condition */
        }
    }
    else
    {
        const char *data = get_string_value(variables, start_p);
        if (data != NULL)
            buf = strdup(data);
        else
        {
            int plugin_result;
            /* no variable with this name, try running a command */
            plugin_result = plugin(variables, start_p, NULL);
            set_integer_value(variables, "RESULT_STATUS", plugin_result);
            if ( plugin_result == PLUGIN_COMPLETED)
            {
                const char *data = get_string_value(variables, "RESULT");
                if (data != NULL)
                    buf = strdup(data);
            }
            else if (plugin_result == NO_PLUGIN_AVAILABLE)
            {
                /* there is no plugin, use the string value for the lhs */
                buf = strdup(command_string);
            }
        }
#if 0
		/* we permit the user to refer to variables before they are defined
		   by providing an empty string result 
           
           (no longer true)
		*/
		if (!buf) buf = strdup("");
#endif
    }
    return buf;	
}

static void display_condition(condition *curr)
{
    const char *op = op_name(curr->operation);
    const char *state_name = find_symbol_with_int_value(states, curr->set);
    if (op && *op)
        printf("test: %s %s %s from state %s. ", curr->test, 
            op, curr->check, state_name);
    else
        printf("test: %s %d %s from state %s. ", curr->test, 
            curr->operation, curr->check, state_name);
}

static int check_one_condition(symbol_table variables, condition *curr)
{
	char *buf = NULL;
	int result = 1;  /* set this to zero if the check passes */
	/* run this test */
	if (verbose() || action_tracing()) 
        display_condition(curr);

    
    /* phase 1. execute the actual test. Most of these are in external routines 
          with an interface a bit like main()
	 */

	if (curr->operation == ASSIGNED /*&& curr->rexp*/)
	{
		/* in this case, the command to execute is the RHS of our condition. */
		buf = collect_data(variables, curr->check);
		if (buf)
		{
			set_string_value(variables, curr->test, buf);
			free(buf);
            if (verbose() || action_tracing()) printf("\n");
			return 0; /* gotcha: 0 means success! */
		}
		else
			return result;
	}
	else
	{
		/* try to collect data using a variable or by running a plugin. */
		buf = collect_data(variables, curr->test);
		if (buf == NULL) buf = strdup(""); 
	}
	
	/* perform the test. Note that some tests do not need a buffer
	   they are tested directly below */
	if ( (curr->operation == MATCHES || curr->operation == NOT_MATCHES)  && curr->rexp)
	{
	
		 if ( curr->rexp->compilation_result == 0)
			{
				int res;
				if (buf == NULL)
				 	res = execute_pattern(curr->rexp, curr->test);
				else
				 	res = execute_pattern(curr->rexp, buf);
				
                if (res == 0)
                {
                    if (curr->operation == NOT_MATCHES)
                    {
                        if (verbose() || action_tracing()) printf("test does not match\n");
                    }
					else 
                    {
                        if (verbose() || action_tracing()) printf("test matches\n");
                        result = 0;
                    }
                }
				else if (res == REG_NOMATCH)
				{
                    if (curr->operation == NOT_MATCHES)
                    {
                        if (verbose() || action_tracing()) printf("test matches\n");
                        result = 0;
                    }
					else
                    {
                        if (verbose() || action_tracing()) printf("test does not match\n");
                    }
				}
				else 
					printf("error %d running the test\n", res);

			}
	}
	else if (strcmp(curr->test, "TIMER") == 0)
	{
		int timer_val = get_integer_value(variables, "TIMER");
		int check_val = integer_value(variables, curr->check);
		if (verbose() || action_tracing())
			printf("timer check %d %d %d", timer_val, curr->operation, check_val);
		if (perform_integer_compare(timer_val, curr->operation, check_val) )
		{
			result = 0;
			if (verbose() || action_tracing())printf(" passed\n");
		}
		else {
			if (verbose() || action_tracing()) printf(" failed\n");
		}
	}
	else if (is_comparison_op(curr->operation))
	{
        /* if the rhs is a variable name, use the contents of the variable in the test */
        const char *check_value = get_string_value(variables, curr->check);
        if (check_value == NULL)
            check_value = curr->check;
		if (is_integer(buf))
			result = perform_integer_compare(atoi(buf), curr->operation, atoi(check_value));
		else
			result = perform_string_compare(buf, curr->operation, check_value);
		result = !result; /* cater for the strange intersion of return value for these conditions */
        if (verbose() || action_tracing())
            printf("%s\n", (result==0) ? " passed\n" : " failed\n");
	}
	if (buf != NULL)
		free(buf);
	return result;
}

int check_condition(symbol_table variables, int set)
{
	condition *curr = condition_table;
	while (curr != NULL) 
	{
		if (curr->set == set) 
			check_one_condition(variables, curr);
	    curr = curr->next;
	}
	return 0;
}

/**
  check conditions for all states to find a set which are currently valid. At present, all 
  conditions are checked, however this is not guaranteed. In the future, this process will
  be optimised.
 */
int check_all_conditions(symbol_table variables)
{
	/* note: the last condition set is held in condition_set_number.  There is 
		never any condition numbered zero but we ignore that and prepare a 
		slot anyway.
	 */
	int *failed = malloc((condition_set_number+1) * sizeof(int));
	int *conditions_run = malloc((condition_set_number+1) * sizeof(int)); /* counts how many conditions were run for each state */
	int i;
	int result = -1;
	condition *curr = condition_table;
    int unknownStateConditionSet = get_integer_value(states, "UNKNOWN");
	
	for (i=0; i<= condition_set_number; i++)
	{
		failed[i] = 0;
		conditions_run[i] = 0;
	}
    /* there is no way back to the start state */
    failed[get_integer_value(states, "START")] = 1; 
	while (curr != NULL) 
	{
		int res = check_one_condition(variables, curr);
		conditions_run[curr->set]++;
		if (res != 0)
			failed[curr->set] = 1;
		curr = curr->next;
	}
    {
        int max = 0;
        /* return the first set that has not failed */
        for (i=0; i< condition_set_number; i++)
        {
            if (i != unknownStateConditionSet && !failed[i])
            {
                if (result<0 || max < conditions_run[i])
                {
                    max = conditions_run[i];
                    result =  i;
                }
            }
            /*else
            {
                printf("condition set %d failed after %d conditions\n", i, conditions_run[i]);
            }*/
        }
    }
	if (result == -1) /* no states match, switch to the unknown state */
		result = unknownStateConditionSet;
	free(failed);
	free(conditions_run);
	return result;
}

void release_condition_set(int set)
{
	/* remove all entries matching this set */
	condition *curr = condition_table;
	while (curr != NULL) 
	{
		if (curr->set == set) 
		{
			condition *old = curr;

			/* special case removal from the head of the list */
			if (old == condition_table)
				condition_table = condition_table->next;
				
			free(old->test);
		    free(old->check);
			free(old);
		}
	    curr = curr->next;
	}
}
