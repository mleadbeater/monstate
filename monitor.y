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
%{
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#define __MAIN__
#include "monitor.h"
#include "symboltable.h"
#include "condition.h"
#include "method.h"
#include "method.h"
#include "plugin.h"
#include "options.h"
#include "version.h"
#include "splitstring.h"
#include "buffers.h"

  extern int yylineno;
  int line_num = 1;   /* updated by the lexical analysis and used for error reporting */
  int num_errors = 0;
  int current_conditions;
  int current_handler; /* actions are stored into the current state handler */

  symbol_table variables;
  symbol_table states;

  char *current_method = NULL;
  char *current_state = NULL;
  const char *active_state = NULL; /* used when running the program */
  parameter_list params = NULL;

  time_t timer_val;

  extern FILE *yyin;
  int yylex(void);

  int set_current_method(const char *method, const char *state);
  int set_current_state(const char *new_name);
  void clear_current();

  void new_simple_condition(const char *lhs, int op, const char *rhs, parameter_list params);
  void new_pattern_condition(const char *lhs, const char *pattern);
  void new_inverted_pattern_condition(const char *lhs, const char *pattern);

  void new_symbol(const char *prefix, const char *name, int value);

  char *new_joined_string(const char *str1, char separator, const char *str2);
  char *swap(char *a, char *b);
  char *interpret_escapes(const char *str);

%}

/* we list all tokens here for the moment and have a corresponding
   definition in the lex file. This list may be reduced later to
   treat simple tokens more naturally.
 */

%token STATE ENTRY POLL NUMBER SEPARATOR
%token IPADDR PATTERN WORD LF OEXPR EEXPR OBRACE
%token EBRACE QUOTE LE LT GE GT NE MATCHES NOT_MATCHES EQ ASSIGNED
%token NOT SET AND OR SQUOTE LOG RESTART /*TOK_FILE*/ TOK_EXIT
%token PROPERTY DEFINE COLLECT FROM TEST EXECUTE SPAWN RUN
%token CALL TRIM LINE OF USING MATCH IN REPLACE WITH INTERPRET
%token JOINED EACH DO FUNCTION MATCHING VERSION


/* if you have an old bison or a real yacc, the %error-verbose setting will give
    an error and you will need to comment the following line out
%error-verbose
*/

%%

program:
program entry
| program state
| program poll
| program property
| program function
| error EBRACE
|
;

entry_header:
ENTRY WORD
{
  current_handler = set_current_method("ENTRY", $2.sVal);
  new_symbol("ENTRY", current_method, current_handler);
  free($2.sVal);
}
;

entry:
entry_header OBRACE action EBRACE
{
  clear_current();
}
| 	entry_header OBRACE EBRACE

;

function_header:
FUNCTION WORD
{
  current_handler = set_current_method("FUNCTION", $2.sVal);
  new_symbol("FUNCTION", current_method, current_handler);
  free($2.sVal);
}
;

function:
function_header OBRACE action EBRACE
{
  clear_current();
}
| 	function_header OBRACE EBRACE

;

state_header:
STATE WORD
{
  current_handler = create_method();
  current_conditions = set_current_state($2.sVal);
  set_integer_value(states, current_state, current_conditions);
  new_symbol("STATE", current_state, current_conditions);
  free($2.sVal);
}
;

state:
state_header OBRACE condition EBRACE
{
  clear_current();
}
|
state_header OBRACE  EBRACE
;

poll_header:
POLL WORD
{
  current_handler = set_current_method("POLL", $2.sVal);
  new_symbol("POLL", current_method, current_handler);
  free($2.sVal);
}
;

poll:
poll_header OBRACE action EBRACE
{
  clear_current();
}
|
poll_header OBRACE EBRACE
;

property_header:
PROPERTY WORD
{
  current_handler = set_current_method("ALIAS", $2.sVal);
  new_symbol("ALIAS", current_method, current_handler);
  free($2.sVal);
}
;

property:
property_header OBRACE property_list EBRACE
{
  clear_current();
}
;

property_list:
definition
| property_list SEPARATOR
| property_list SEPARATOR definition
| error SEPARATOR
;

definition:
DEFINE WORD ASSIGNED value
{
  char *interpreted_string = interpret_escapes($4.sVal);
  char *symbol_name = new_joined_string(current_method, '_', $2.sVal);
  set_string_value(variables, symbol_name, interpreted_string);
  free(interpreted_string);
  free($2.sVal);
  free($4.sVal);
}
| WORD ASSIGNED value
{
  char *interpreted_string = interpret_escapes($3.sVal);
  char *symbol_name = new_joined_string(current_method, '_', $1.sVal);
  set_string_value(variables, symbol_name, interpreted_string);
  free(interpreted_string);
  free($1.sVal);
  free($3.sVal);
}
;

action:
command
| action SEPARATOR              {   if (params)
{
free_parameter_list(params);
  params = NULL;
}
                                }
| action SEPARATOR command
| error SEPARATOR
;

command:
SET WORD ASSIGNED strval
{
  method *m;
  if (strcmp($2.sVal, "CYCLE") == 0)
  {
    /* backward compatibility */
    add_assign_action(current_handler, $2.sVal, $4.sVal);  /* old-style assignment is good enough here */
    m = add_assign_action(current_handler, "SYSTEM_DELAY", NULL);
  }
  else
    m = add_assign_action(current_handler, $2.sVal, NULL);
  free($2.sVal);
  free($4.sVal);
  add_method_parameters(m, params);
  free_parameter_list(params);
  params = NULL;
}
| WORD ASSIGNED strval
{
  method *m;
  if (strcmp($1.sVal, "CYCLE") == 0)
  {
    /* backward compatibility */
    add_assign_action(current_handler, $1.sVal, $3.sVal); /* old-style assignment is good enough here */
    m = add_assign_action(current_handler, "SYSTEM_DELAY", NULL);
  }
  else
    m = add_assign_action(current_handler, $1.sVal, NULL);
  free($1.sVal);
  free($3.sVal);
  add_method_parameters(m, params);
  free_parameter_list(params);
  params = NULL;
}
| TRIM WORD
{
  add_typed_action(current_handler, TRIM_ACTION, $2.sVal);
  free($2.sVal);
}
| LINE WORD WORD
{
  add_line_action(current_handler, $3.sVal, $2.sVal);
  free($2.sVal);
  free($3.sVal);
}
| LINE NUMBER WORD
{
  add_line_action(current_handler, $3.sVal, $2.sVal);
  free($2.sVal);
  free($3.sVal);
}
| MATCH PATTERN /* pattern */ IN WORD
{
  char **param_array = malloc( sizeof(char *) * 3);
  param_array[0] = $2.sVal;
  param_array[1] = $4.sVal;
  param_array[2] = 0;
  add_action_params(current_handler, "MATCH", param_array);
}
| MATCH WORD /* pattern */ IN WORD
{
  char **param_array = malloc( sizeof(char *) * 4);
  param_array[0] = strdup("-p");
  param_array[1] = $2.sVal;
  param_array[2] = $4.sVal;
  param_array[3] = 0;
  add_action_params(current_handler, "MATCH", param_array);
}
| REPLACE PATTERN /* pattern */ IN WORD /* text */ WITH WORD /* replacement */
{
  char **param_array = malloc( sizeof(char *) * 4);
  param_array[0] = $2.sVal;
  param_array[1] = $4.sVal;
  param_array[2] = $6.sVal;
  param_array[3] = 0;
  add_action_params(current_handler, "REPLACE", param_array);
}
| REPLACE WORD /* pattern */ IN WORD /* text */ WITH WORD /* replacement */
{
  char **param_array = malloc( sizeof(char *) * 5);
  param_array[0] = strdup("-p");
  param_array[1] = $2.sVal;
  param_array[2] = $4.sVal;
  param_array[3] = $6.sVal;
  param_array[4] = 0;
  add_action_params(current_handler, "REPLACE", param_array);
}
| INTERPRET WORD /* buffer */ USING WORD /* property name */
{
  char **param_array = malloc( sizeof(char *) * 3);
  param_array[0] = $2.sVal;
  param_array[1] = $4.sVal;
  param_array[2] = 0;
  add_action_params(current_handler, "INTERPRET", param_array);
}
| EACH WORD /*variable name*/ MATCHING PATTERN /* pattern */ IN WORD DO WORD
{
  char **param_array = malloc( sizeof(char *) * 5);
  param_array[0] = $2.sVal;
  param_array[1] = $4.sVal;
  param_array[2] = $6.sVal;
  param_array[3] = $8.sVal;
  param_array[4] = 0;
  add_action_params(current_handler, "EACH", param_array);
}
| DO WORD
{
  char **param_array = malloc( sizeof(char *) * 2);
  param_array[0] = $2.sVal;
  param_array[1] = 0;
  add_action_params(current_handler, "DO", param_array);
}
| DO WORD WITH WORD
{
  char **param_array = malloc( sizeof(char *) * 3);
  param_array[0] = $2.sVal;
  param_array[1] = $4.sVal;
  param_array[2] = 0;
  add_action_params(current_handler, "DO", param_array);
}

| LOG strval
{
  method *m = add_log_action(current_handler, NULL );
  free($2.sVal);
  add_method_parameters(m, params);
  free_parameter_list(params);
  params = NULL;
}
| EXECUTE WORD
{
  add_execute_action(current_handler, $2.sVal );
  free($2.sVal);
}
| SPAWN WORD
{
  add_spawn_action(current_handler, $2.sVal );
  free($2.sVal);
}
| RUN WORD
{
  add_run_action(current_handler, "RESULT", $2.sVal );
  free($2.sVal);
}
| SET WORD ASSIGNED RUN WORD
{
  add_run_action(current_handler, $2.sVal, $5.sVal );
  free($2.sVal);
  free($5.sVal);
}
| VERSION
{
  add_assign_action(current_handler, "RESULT", MONSTATE_VERSION);

}
| RESTART
{
  add_action(current_handler, "RESTART");
}
| TOK_EXIT
{
  add_action(current_handler, "EXIT");
}
| CALL strval /* calls a function defined in a property list, optionally including parameters */
{
  method *m = add_call_action(current_handler, NULL );
  free($2.sVal);
  add_method_parameters(m, params);
  free_parameter_list(params);
  params = NULL;
}
| CALL strval USING WORD /* calls a function defined in a property list, optionally including parameters */
{
  /*
  char *cmd = new_joined_string($2.sVal, ' ', "USING");
  char *cmd2 = new_joined_string(cmd, ' ', $4.sVal);
  */
  method *m = add_call_action(current_handler, NULL );
  add_parameter(params, "USING");
  add_parameter(params, $4.sVal);
  add_method_parameters(m, params);
  free_parameter_list(params);
  params = NULL;
  free($2.sVal);
  free($4.sVal);
  /*
  free(cmd);
  free(cmd2);
  */
}
;

condition:
expr
| strval
| collection
| condition conjunction expr
| condition conjunction strval
| condition conjunction collection
;

conjunction:
AND
| OR
| SEPARATOR
;

collection:
COLLECT WORD FROM strval		{ new_simple_condition($2.sVal, ASSIGNED, $4.sVal, params);
                            free($2.sVal);
                            free($4.sVal);
                            params = NULL;
                          }
;

strval:
value
{
  $$.sVal = $1.sVal;
  if (!params) params = init_parameter_list(4);
  add_parameter(params, $1.sVal);
}
| strval value
{
  $$.sVal = new_joined_string($1.sVal, ' ', $2.sVal);
  if (!params) params = init_parameter_list(4);
  add_parameter(params, $2.sVal);
  free($1.sVal);
  free($2.sVal);
}
| strval JOINED value
{

  char *result = malloc(strlen($1.sVal) + strlen($3.sVal) + 1);
  strcpy(result, $1.sVal);
  strcat(result, $3.sVal);

  if (!params) params = init_parameter_list(4);

  add_parameter(params, $3.sVal);

  free($1.sVal);
  free($3.sVal);
  $$.sVal = result;

}
;

value:
WORD
{
}
| NUMBER
| TEST WORD
| IPADDR
| OEXPR strval EEXPR		{ $$.sVal = $2.sVal;
                      }
;

expr:
value EQ value
{ new_simple_condition($1.sVal, EQ, $3.sVal, params);
  free($1.sVal);
  free($3.sVal);
  params = NULL;
}
| value NE value
{ new_simple_condition($1.sVal, NE, $3.sVal, params);
  free($1.sVal);
  free($3.sVal);
  params = NULL;
}
| value LE value
{ new_simple_condition($1.sVal, LE, $3.sVal, params);
  free($1.sVal);
  free($3.sVal);
  params = NULL;
}
| value LT value
{ new_simple_condition($1.sVal, LT, $3.sVal, params);
  free($1.sVal);
  free($3.sVal);
  params = NULL;
}
| value GE value
{ new_simple_condition($1.sVal, GE, $3.sVal, params);
  free($1.sVal);
  free($3.sVal);
  params = NULL;
}
| value GT value
{ new_simple_condition($1.sVal, GT, $3.sVal, params);
  free($1.sVal);
  free($3.sVal);
  params = NULL;
}
| value MATCHES PATTERN
{ new_pattern_condition($1.sVal, $3.sVal);
  free($1.sVal);
  free($3.sVal);
}
| value NOT MATCHES PATTERN
{ new_inverted_pattern_condition($1.sVal, $4.sVal);
  free($1.sVal);
  free($4.sVal);
}
;

%%

void yyerror(const char *str)
{
  fprintf(stderr, "### Error at line %d: %s\n", yylineno, str);
  num_errors++;
}

int yywrap()
{
  return 1;
}

const char *lookup_value(const char *prefix, const char *name)
{
  const char *result;
  char *format = "%s_%s";
  char *tmp_name = malloc(strlen(format) + strlen(prefix) + strlen(name));
  sprintf(tmp_name, format, prefix, name);
  result = get_string_value_ending(variables, tmp_name);
  free(tmp_name);
  return result;
}

int set_current_method(const char *method, const char *new_name)
{
  int method_id;
  const char *old = lookup_value(method, new_name);
  if (old != NULL)
  {
    method_id = get_integer_value(variables, old);
    /*yyerror("method already defined");*/
    printf("extending %s for %s (%d)\n", method, new_name, method_id);
  }
  else
  {
    method_id = create_method();
  }

  if (current_method != NULL)
    free(current_method);
  current_method = strdup(new_name);
  return method_id;
}

int set_current_state(const char *new_name)
{
  const char *old = lookup_value("", new_name);
  if (old != NULL)
  {
    /* find the old condition set as we are now extending an existing set */
    current_conditions = get_integer_value(states, new_name);
  }
  else
    current_conditions = create_condition_set();

  if (current_state != NULL)
    free(current_state);
  current_state = strdup(new_name);
  return current_conditions;
}

void clear_current()
{
  if (current_method != NULL)
  {
    free(current_method);
    current_method = NULL;
  }
  if (current_state != NULL)
  {
    free(current_state);
    current_state = NULL;
  }
  if (params)
  {
    free_parameter_list(params);
    params = NULL;
  }
}

void new_symbol(const char *prefix, const char *name, int value)
{
  char *format="%s_%s";
  char *full_name = malloc(strlen(format) + strlen(prefix) + strlen(name));
  sprintf(full_name, format, prefix, name);
  set_integer_value(variables, full_name, value);
  free(full_name);
}

void new_simple_condition(const char *lhs, int op, const char *rhs, parameter_list params)
{
  add_condition(current_conditions, lhs, op, rhs, params);
}

void new_pattern_condition(const char *lhs, const char *pattern)
{
  add_condition(current_conditions, lhs, MATCHES, pattern, NULL);
}

void new_inverted_pattern_condition(const char *lhs, const char *pattern)
{
  add_condition(current_conditions, lhs, NOT_MATCHES, pattern, NULL);
}

char *new_joined_string(const char *str1, char separator, const char *str2)
{
  char *result = malloc(strlen(str1) + strlen(str2) + 2);
  sprintf(result, "%s%c%s", str1, separator, str2);
  return result;
}

char *swap(char *a, char *b)
{
  char *tmp = a;
  a = b;
  b = tmp;
  return a;
}


int done = 0;

static void finish(int sig)
{
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);
  done = 1;
}

static void debug(int sig)
{
  if (active_state)
    printf("Current state: %s\n", active_state);
  set_verbose(!verbose());
}

static void showstate(int sig)
{
  if (active_state)
    printf("Current state: %s\n", active_state);
}

void process_method(const char *name)
{
  int id = get_integer_value(variables, name);
  if (id != 0)
  {
    if (verbose())
      printf("processing %s (%d)\n", name, id);
    execute_method(id);
  }
}

/*
char **state_table = NULL;
int allocated_states = 0;
int state_pages = 8;
int num_states = 0;

void add_state(const char *name, int id)
{
    if (num_states == allocated_states)
    {
	    int new_allocation = allocated_states + state_pages;
		char ** new_states = malloc(sizeof(char *) * allocated_states);
		if (allocated_states > 0)
		{
		    memcpy(new_states, state_table, sizeof(char *) * allocated_states);
			free(state_table);
		}
		state_table = new_states;
		allocated_states = new_allocation;
    }
    state_table[num_states] = strdup(name);
	num_states++;
}
*/

/* we do not want to generate zombie processes when we fork()
    but the following code would mean we cannot determine the 
    child exit status of our subcommands so it is currently disabled.
*/
void setup_signals()
{
  struct sigaction sa;
  memset(&sa, 0, sizeof(struct sigaction));
  sa.sa_handler = SIG_DFL;
  //sa.sa_flags = SA_NOCLDWAIT;
  sigaction(SIGCHLD, &sa, NULL);
}

void usage(int argc, char *argv[])
{
  fprintf(stderr, "Usage: %s [-v] [-t] [-l logfilename] [-s maxlogfilesize] \n", argv[0]);
}

int main(int argc, char *argv[])
{
  int start_state;
  int unknown_state;
  int i;
  int opened_file = 0;
  time_t now;
  struct timeval now_tv;
  const char *logfilename = NULL;
  int maxlogsize = 20000;
  pid_t err;
  int retain_terminal = 0;


  tzset(); /* this initialises the tz info required by ctime().  */
  /* Note that if this is not done here, the date plugin will leak */
  gettimeofday(&now_tv, NULL);
  srandom(now_tv.tv_usec);

  setup_signals();

  /* check for commandline options, later we process config files in the order they are named */
  i=1;
  while ( i<argc)
  {
    if (strcmp(argv[i], "-v") == 0)
      set_verbose(1);
    else if (strcmp(argv[i], "-t") == 0) /* retain the controlling terminal */
      retain_terminal = 1;
    else if (strcmp(argv[i], "-l") == 0 && i < argc-1)
      logfilename = argv[++i];
    else if (strcmp(argv[i], "-s") == 0 && i < argc-1)
      maxlogsize = strtol(argv[++i], NULL, 10);
    else if (*(argv[i]) == '-' && strlen(argv[i]) > 1)
    {
      usage(argc, argv);
      exit(2);
    }
    i++;
  }

  if (!retain_terminal)
  {
    err = setsid();
    if (err == (pid_t)-1)
    {
      fprintf(stderr, "setsid: error %s (%d) creating new session\n", strerror(errno), errno);
    }
  }
  if (logfilename)
  {
    int ferr;
    ferr = fflush(stdout);
    ferr = fflush(stderr);
    stdout = freopen(logfilename, "w+", stdout);
    ferr = dup2(1, 2);
  }

  init_plugins();

  variables = init_symbol_table();
  states = init_symbol_table();

  init_methods(variables);
  init_conditions();

  set_integer_value(variables, "SYSTEM_DELAY", 10);
  start_state = create_condition_set();
  unknown_state = create_condition_set();
  active_state = "START";
  timer_val = time(NULL);

  set_integer_value(variables, "STATE_START", start_state);
  set_integer_value(variables, "STATE_UNKNOWN", unknown_state);
  set_integer_value(states, "START", start_state);
  set_integer_value(states, "UNKNOWN", unknown_state);
  set_integer_value(variables, "TIME", timer_val);

  /* load configuration from files named on the commandline */
  i = 1;
  while (i<argc)
  {
    if (*(argv[i]) != '-')
    {
      opened_file = 1;
      yyin = fopen(argv[i], "r");
      if (yyin)
      {
        yylineno = 1;
        yyparse();
        fclose(yyin);
      }
      else
      {
        fprintf(stderr, "Warning: failed to load config: %s\n", argv[i]);
      }
    }
    else if (strlen(argv[i]) == 1) /* '-' means stdin */
    {
      yyin = stdin;
      yylineno = 1;
      yyparse();
    }
    else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "-s") == 0) i++;
    i++;
  }

  /* if we weren't given a config file to use, read config data from stdin */
  if (!opened_file)
    yyparse();

  if (num_errors > 0)
  {
    printf("Errors detected. Aborting\n");
    exit(2);
  }

  time(&now);
#ifdef MONSTATE_VERSION
  printf("\n%s Version %s-%d loaded at %s\n", argv[0], MONSTATE_VERSION, BUILD_NUMBER,  ctime(&now));
#else
  printf("\n%s Version %s loaded at %s\n", argv[0], "(unknown version)", ctime(&now));
#endif

  if (verbose())
  {
    printf("state table\n");
    dump_symbol_table(states);
  }


  if (verbose())
  {
    /* install these handlers so we can produce a report */
    signal(SIGINT, finish);
    signal(SIGTERM, finish);
    set_integer_value(variables, "SHOW_STATE_CHANGES", 1);
  }
  else
  {
    set_integer_value(variables, "SHOW_STATE_CHANGES", 0);
  }

  active_state = "START";
  signal(SIGUSR1, debug);
  signal(SIGUSR2, showstate);
  process_method("ENTRY_START");
  set_string_value(variables, "LAST", "");
  while (!done)
  {
    int method_id;
    int next_state;
    int method_result = 0;
    const char *next_state_name;
    char *method_name;
    int delay = 0;

    if (ftell(stdout) > maxlogsize)
    {
      rewind(stdout);
      rewind(stderr);
      ftruncate(1, 0);
    }
    if (verbose())
      printf("\n\nIn state %s\n", active_state);

    next_state = check_all_conditions(variables);
    next_state_name = find_symbol_with_int_value(states, next_state);
    set_integer_value(variables, "TIME", time(NULL));
    {
      int tracing;
      if ( (tracing = get_integer_value(variables, "TRACE_STEPS")) != action_tracing() )
        set_action_tracing( tracing );
    }
    if (next_state_name != NULL && strcmp(next_state_name, active_state) != 0)
    {
      /* change states and run the entry method */
      if (verbose())
        printf("next state: %s (%d)\n", next_state_name , next_state);
      method_name = new_joined_string("ENTRY", '_', next_state_name);
      method_id = get_integer_value(variables, method_name);
      timer_val = time(NULL);
      set_integer_value(variables, "TIMER", 0);
      if ( get_integer_value(variables, "SHOW_STATE_CHANGES") )
      {
        printf("changing state to %s\n", next_state_name);
        fflush(stdout);
      }
      if (method_id > 0)
        method_result = execute_method(method_id);
      free(method_name);
      set_string_value(variables, "LAST", active_state);
      active_state = next_state_name;
      set_string_value(variables, "CURRENT", active_state);
    }
    else
    {
      /* run the poll method */
      method_name = new_joined_string("POLL", '_', active_state);
      method_id = get_integer_value(variables, method_name);
      set_string_value(variables, "CURRENT", active_state);
      set_integer_value(variables, "TIMER", time(NULL) - timer_val);
      if (method_id > 0)
        method_result = execute_method(method_id);
      free(method_name);
    }

    delay = get_integer_value(variables, "SYSTEM_DELAY");
    if (delay <= 0) delay = 0; /* just in case. */

    if (method_result == -1)
      done = 1;
    else if (delay == 0)
      /* we do not permit the user to completely overload the machine */
      usleep(20000L);
    else
      sleep(delay);

    {
      const char *debug_on = get_string_value(variables, "DEBUG");
      if (debug_on && strcmp(debug_on, "true") == 0)
        set_verbose(1);
      else if (debug_on && strcmp(debug_on, "false") == 0)
        set_verbose(0);
    }
  }
  if ( verbose())
  {
    printf("\nsymbol table\n");
    dump_symbol_table(variables);

    printf("\nconditions\n");
    display_all_conditions();
  }
  release_plugins();

  release_all_conditions();
  release_all_methods();
  free_symbol_table(states);
  free_symbol_table(variables);
  return 0;
}

