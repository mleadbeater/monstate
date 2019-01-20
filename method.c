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
#include <sys/stat.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <ctype.h>
#include <errno.h>

#include "method.h"
#include "symboltable.h"
#include "splitstring.h"
#include "condition.h"
#include "property.h"
#include "options.h"
#include "plugin.h"
#include "regular_expressions.h"


method *method_table;
static int num_entries;
static symbol_table variables;

static int method_id_number;

void init_methods(symbol_table variables_table)
{
  method_id_number = 0;
  method_table = NULL;
  num_entries = 0;
  variables = variables_table;
}

char *action_name(enum action_type action)
{
  switch (action)
  {
  case NULL_ACTION:
    return "NULL";
    break;
  case LOG_ACTION:
    return "LOG";
    break;
  case SET_ACTION:
    return "ASSIGN";
    break;
  case EXIT_ACTION:
    return "EXIT";
    break;
  case RESTART_ACTION:
    return "RESTART";
    break;
  case CONTROL_ACTION:
    return "CONTROL";
    break;
  case EXECUTE_ACTION:
    return "EXECUTE";
    break;
  case RUN_ACTION:
    return "RUN";
    break;
  case CALL_ACTION:
    return "CALL";
    break;
  case LINE_ACTION:
    return "LINE";
    break;
  case TRIM_ACTION:
    return "TRIM";
    break;
  case GENERIC_ACTION:
    return "UNKNOWN";
    break;
  default:
    printf("unknown action type %d\n", action);
  }
  return "UNKNOWN";
}

void release_all_methods()
{
  method *curr = method_table;
  while (curr != NULL)
  {
    method_table = method_table->next;
    free(curr->action);
    free_parameter_list(curr->parameters);
    if (curr->params) free(curr->params);
    free(curr);
    curr = method_table;
  }
  init_methods(NULL);
}

void display_all_methods()
{
  method *curr = method_table;
  while (curr != NULL)
  {
    printf("%d: %s\n", curr->id, curr->action);
    curr = curr->next;
  }
}

int create_method()
{
  method_id_number++;
  return method_id_number;
}

/* return a pointer to the last element in the list
    where the element group id matches that given.

     * returns NULL if nomatch can be found.
     * the special id -1 causes this to return the last element of the list
 */
method *find_last_element_of(int id)
{
  method *pos = method_table;
  method *last = NULL;
  while (pos != NULL)
  {
    if (id == -1 || pos->id == id)
      last = pos;
    pos = pos->next;
  }
  return last;
}

method *find_last_element()
{
  return find_last_element_of(-1);
}

method *create_typed_action(int id, enum action_type kind, const char *action)
{
  method *new_method = malloc(sizeof(struct method));
  /* insert this method at the end of the list
     so actions are executed in the order they are written
   */
  new_method->next = NULL;
  new_method->kind = kind;
  new_method->id = id;
  new_method->action = strdup(action);
  new_method->params = NULL;
  new_method->parameters = init_parameter_list(4);
  return new_method;
}

void add_method_parameters(method *m, parameter_list params)
{
  int i;
  if (!params) return;
  for (i=0; i<params->used; i++)
    add_parameter(m->parameters, params->elements[i]);
}

void add_method_parameter(method *m, const char *param)
{
  if (!param) return;
  add_parameter(m->parameters, param);
}

method *add_typed_action(int id, enum action_type kind, const char *action)
{
  method *new_method = create_typed_action(id, kind, action);
  /* insert this method at the end of the list
     so actions are executed in the order they are written
   */
  method *last = find_last_element();
  if (last == NULL)
    method_table = new_method;
  else
    last->next = new_method;
  num_entries++;
  return new_method;
}

/* adds an action with a list of parameters.

    The list must be NULL terminated
*/
void add_action_params(int method_id, const char *action, char **params)
{
  method *m = add_typed_action(method_id, GENERIC_ACTION, action);
  m->params = params;
}

method *add_log_action(int id,const char *message)
{
  method *result = add_typed_action(id, LOG_ACTION, "LOG");
  if (message)
    add_method_parameter(result, message);
  return result;
}

void add_execute_action(int id,const char *message)
{
  add_typed_action(id, EXECUTE_ACTION, message);
}

void add_spawn_action(int id,const char *message)
{
  add_typed_action(id, SPAWN_ACTION, message);
}

method *add_call_action(int id,const char *message)
{
  method *m = add_typed_action(id, CALL_ACTION, "CALL");
  if (message)
    add_method_parameter(m, message);
  return m;
}

method *add_assign_action(int id,const char *var_name, const char *value)
{
  method * new_method = add_typed_action(id, SET_ACTION, var_name);
  if (value) add_method_parameter(new_method, value);
//	new_method->params = malloc(sizeof(char *)*2);
//	new_method->params[0] = strdup(value);
//	new_method->params[1] = NULL;
  return new_method;
}

void add_line_action(int id, const char *var_name, const char *line)
{
  method * new_method = add_typed_action(id, LINE_ACTION, var_name);
  new_method->params = malloc(sizeof(char *)*2);
  new_method->params[0] = strdup(line);
  new_method->params[1] = NULL;
}

void add_run_action(int method_id, const char *var_name, const char *command)
{
  /* note: we store the command with the action and the result variable name as a parameter.
  	This is the opposite of the assignment action.
   */
  method * new_method = add_typed_action(method_id, RUN_ACTION, command);
  if (var_name)
  {
    new_method->params = malloc(sizeof(char *)*2);
    new_method->params[0] = strdup(var_name);
    new_method->params[1] = NULL;
  }
}


void add_action(int id, const char *action)
{
  if (strcmp(action, "EXIT") == 0)
    add_typed_action(id, EXIT_ACTION, action);
  else
    add_typed_action(id, GENERIC_ACTION, action);
}

char **copy_environment()
{
  extern char** environ;
  char **curr = environ;
  char **result;
  char **out;
  int count = 0;
  while (*curr++) count++;
  count++;
  result = (char**) malloc(count * sizeof(char*));
  out = result;
  curr = environ;

  while (*curr)
    *out++ = strdup(*curr++);
  *out = NULL;
  return result;
}

char * new_temp_filename(const char *prefix, const char *suffix)
{
  char tmp_filename[20];
  int res;

  do
  {
    struct stat fs;
    sprintf(tmp_filename, "%s%04ld%s", prefix, random() % 10000, suffix);
    res = stat(tmp_filename, &fs);
  }
  while (res != -1);
  return strdup(tmp_filename);
}

void load_file_to_variable(symbol_table symbols, const char *var_name, const char *filename)
{
  condition_function read_file;

  int buflen = 2000;
  int argc = 2;
  const char *argv[] = { "", NULL, NULL};

  char *buf;
  int res;
  argv[1] = filename;
  buf = malloc(buflen);
  res = read_file(variables, buf, buflen, argc, argv);
  if (res != -1)
    set_string_value(variables, var_name, buf);
  free(buf);
}

char *select_line(int n, const char *data)
{
  char term='\n';
  int current = 1;
  const char *start = data;
  const char *end = NULL;
  if (!data)
    return strdup("");

  while (*start && current < n)
  {
    if (*start == term) current++;
    start++;
  }
  if (*start)
  {
    end = strchr(start, term);
  }
  if (end)
  {
    char *result = malloc(end - start + 1);
    if (!result)
    {
      perror("malloc");
      return NULL;
    }
    memmove(result, start, end-start);
    result[end-start] = 0;
    return result;
  }
  return strdup(start);
}

void trim(char *value)
{
  char *p ;
  if (!value || !*value) return;
  p = value + strlen(value) -1;
  while ( p >= value && isspace(*p) ) p--;
  *(++p) = 0;
}


struct my_match_data
{
  const char *symbol_name;
  int method_id;
};

int each_match_do(const char *match, void *data )
{
  struct my_match_data *info = data;
  int return_val = 0;
  if (info && info->method_id > 0)
  {
    char *new_result;
    const char *old_result = get_string_value(variables, "RESULT");
    char *saved_result;
    if (old_result)
      saved_result = strdup(old_result);
    else
      saved_result = strdup("");
    set_string_value(variables, info->symbol_name, match);
    return_val = execute_method(info->method_id);
    asprintf(&new_result, "%s%s", saved_result, get_string_value(variables, "RESULT"));
    saved_result = replace_buffer(saved_result, new_result);
    set_string_value(variables, "RESULT", saved_result);
    free(saved_result);
  }
  return return_val;
}

void copy_property(const char *property_group, const char *name,
                   const char *value, void *user_data)
{
  set_string_property(variables, "PARAM", name, value);
}


int execute_method(int id)
{
  int res;
  method *curr = method_table;
  while (curr != NULL)
  {
    if (curr->id == id)
    {
      if (action_tracing())
      {
        if (curr->kind != GENERIC_ACTION
            && curr->kind != EXIT_ACTION
            && curr->kind != LOG_ACTION
            && curr->kind != CALL_ACTION)
          printf("ACTION: %s %s ", action_name(curr->kind), curr->action);
        else
          printf("ACTION: %s ", curr->action);
        if (curr->params) display_params(curr->params);
        if (curr->parameters) display_params(curr->parameters->elements);
        printf("\n");
      }
      /* run this action */
      switch (curr->kind)
      {
      case GENERIC_ACTION:
        if (strncmp(curr->action, "SET ", 4) == 0)
        {
          /* assignment */
          /*const char *rhs_value;*/
          char *cmd = strdup(curr->action);
          char *lhs = cmd + 4;
          char *op_pos = strchr(lhs, '=');
          /*char *rhs = op_pos+1;*/
          int i;
          *op_pos = 0;
          if (curr->parameters)
          {
            char *rhs = strdup("");
            for (i=0; i<curr->parameters->used; i++)
              asprintf(&rhs, "%s%s", rhs, name_lookup(variables, curr->parameters->elements[i]));
            if (curr->parameters->used > 1)
              set_string_value(variables, lhs, name_lookup(variables, rhs));
            else
              set_string_value(variables, lhs, rhs);
            free(rhs);
          }
#if 0
          rhs_value = get_string_value(variables, rhs);
          if (rhs_value)
            set_string_value(variables, lhs, rhs_value);
          else
            set_string_value(variables, lhs, rhs);
#endif
          free(cmd);
        }
        else if (strcmp(curr->action, "MATCH") == 0 )
        {
          int idx = 0;
          const char *pattern;
          const char *text;
          rexp_info *info;
          if (strcmp(curr->params[0], "-p") == 0)
          {
            idx++; /* skip the -p */
            pattern = name_lookup(variables, curr->params[idx++]);
          }
          else
            pattern = curr->params[idx++];
          text = name_lookup(variables, curr->params[idx++]);
          info = create_pattern(pattern);
          if (find_matches(info, variables, text) == 0)
          {
            const char *matched = get_string_value(variables, "REXP_0");
            if (!matched) matched = ""; /* surely this cannot happen */
            set_string_value(variables, "RESULT", matched);
          }
          else
            set_string_value(variables, "RESULT", "fail");
          release_pattern(info);
        }
        else if (strcmp(curr->action, "REPLACE") == 0 )
        {
          int idx = 0;
          const char *pattern;
          const char *text;
          const char *subst;
          rexp_info *info;
          char *new_text;
          if (strcmp(curr->params[0], "-p") == 0)
          {
            idx++;
            pattern = name_lookup(variables, curr->params[idx++]);
          }
          else
            pattern = curr->params[idx++];
          text = name_lookup(variables, curr->params[idx++]);
          subst = name_lookup(variables, curr->params[idx++]);

          info = create_pattern(pattern);
          new_text = substitute_pattern(info, variables, text, subst);
          if (new_text)
            set_string_value(variables, "RESULT", new_text);
          else
            set_string_value(variables, "RESULT", "");
          free(new_text);
          release_pattern(info);

        }
        else if (strcmp(curr->action, "INTERPRET") == 0 )
        {
          const char *text = name_lookup(variables, curr->params[0]);
          const char *properties = name_lookup(variables, curr->params[1]);
          interpret_text(variables, properties, "RESULT", text);
        }
        else if (strcmp(curr->action, "EACH") == 0 )
        {
          struct my_match_data data;
          const char *pattern = name_lookup(variables, curr->params[1]);
          const char *text = name_lookup(variables, curr->params[2]);
          const char *short_name = name_lookup(variables, curr->params[3]);
          char *function_name = malloc(strlen("FUNCTION_") + strlen(short_name) + 1);
          data.symbol_name = curr->params[0]; /* variable name; don't look for its value */
          sprintf(function_name, "FUNCTION_%s", short_name);

          data.method_id = get_integer_value(variables, function_name);
          free(function_name);
          set_string_value(variables, "RESULT", "");
          if (data.method_id > 0)
          {
            rexp_info *info = create_pattern(pattern);
            each_match(info, text, each_match_do, &data);
            release_pattern(info);
          }
        }
        else if (strcmp(curr->action, "DO") == 0 )
        {
          int method_id;
          const char *short_name = name_lookup(variables, curr->params[0]);
          char *function_name = malloc(strlen("FUNCTION_") + strlen(short_name) + 1);
          sprintf(function_name, "FUNCTION_%s", short_name);
          method_id = get_integer_value(variables, function_name);
          free(function_name);
          if (curr->params[1])
          {

            symbol_table method_params = collect_properties(variables, curr->params[1]);
            remove_matching(variables, "^PARAM_");
            each_property(method_params, curr->params[1], copy_property, NULL);
            free_symbol_table(method_params);
          }
          set_string_value(variables, "RESULT", "");
          if (method_id > 0)
            execute_method(method_id);
        }
        else
          printf("should run action: %s\n", curr->action);
        break;
      case LOG_ACTION:
      {
        {
          int i;
          if (curr->parameters)
          {
            for (i=0; i<curr->parameters->used; i++)
              printf("%s", name_lookup(variables, curr->parameters->elements[i]));
            printf("\n");
          }
        }
        fflush(stdout);
      }
      break;

      case TRIM_ACTION:
      {
        const char *old_value = get_string_value(variables, curr->action);
        char *value;
        if (!old_value || strlen(old_value) == 0)
          break;
        value = strdup(old_value);
        {
          trim(value);
          set_string_value(variables, curr->action, value);
        }
        free(value);
      }
      break;
      case CALL_ACTION:
      {
        if (curr->parameters)
        {
          char **elements = duplicate_params(curr->parameters->elements);
          int plugin_result = plugin(variables, curr->parameters->elements[0], elements);
          release_params(elements);
          set_integer_value(variables, "RESULT_STATUS", plugin_result);
        }
        else
        {
          fprintf(stderr, "Warning: call action is missing parameters\n");
          int plugin_result = plugin(variables, curr->action, NULL);
          set_integer_value(variables, "RESULT_STATUS", plugin_result);
        }

      }
      break;
      case RUN_ACTION:
      {
        /* similar to spawning a command except we manually redirect output to
         * a temporary file and also wait for the spawned command to finish
         */
        char *cmd_str;
        char **parameters;
        char **newenv;
        char **newparams;
        int child;
        char *tmpfile = new_temp_filename("/tmp/mon-", ".txt");
        const char *program = get_string_value(variables, curr->action);
        set_integer_value(variables, "RESULT_STATUS", 0);
        if (!program)
          program = curr->action;
        cmd_str = malloc(strlen(program) + strlen(tmpfile) + 5);
        sprintf(cmd_str,"%s >%s", program, tmpfile);
        parameters = split_string(cmd_str);
        free(cmd_str);
        newenv = copy_environment();

        child = vfork();
        if (child == -1) /* error */
        {
          perror("vfork");
        }
        else if (child == 0) /* child */
        {
          newparams = perform_redirections(parameters);
          res = execve(newparams[0], newparams, newenv);
          if (res == -1)
          {
            perror("execve");
            display_params(newparams);
            release_params(newparams);
            _exit(2);
          }
        }
        else /* parent */
        {
          int stat;
          int err;
          printf("waitpid...\n");
          /* Using waitpid when we have set SA_NOCLDSTOP
                for SIGCHILD seems to set 'status' to indicate
                WIFSIGNALED() and WTERMSIG to 89 or 95, which are out of range.
             This works fine if we do not set SA_NOCLDSTOP

            use err = wait(&stat), below, if you want to experiment 
            with the idea again and don't forget that err will be -1 with
            errno = ECHILD if SA_NOCLDSTOP is set.

           */
          err = waitpid(child, &stat, 0);
          if (err == -1)
          {
            perror("waitpid");
            /*  see comments above. */
            /*
            if (errno == ECHILD)
              printf("child exited %d (stat = %d)\n", WEXITSTATUS(stat), stat);
            */
          }
          if (stat == 0)
          {
            set_integer_value(variables, "RESULT_STATUS", 0);
          }
          else if (WIFEXITED(stat))
          {
            set_integer_value(variables, "RESULT_STATUS", WEXITSTATUS(stat));
            if (verbose()) printf("%s returned (exit %d): %d\n", 
              program, WEXITSTATUS(stat), stat);
          }
          else if (WIFSIGNALED(stat))
          {
            set_integer_value(variables, "RESULT_STATUS", WTERMSIG(stat));
            if (verbose()) printf("%s returned (signal %d): %d\n",
                                    program, WTERMSIG(stat), stat);
          }
          else if (WIFSTOPPED(stat))
          {
            set_integer_value(variables, "RESULT_STATUS", WSTOPSIG(stat));
            if (verbose()) printf("%s returned (stop signal): %d\n", program, stat);
          }
          release_params(newparams);
        }
        release_params(newenv);
        release_params(parameters);

        /*  the command output should now be available in the temporary file */
        load_file_to_variable(variables, curr->params[0], tmpfile);
        res = unlink(tmpfile);
        free(tmpfile);
        {
          const char *res = get_string_value(variables, curr->params[0]);
          if (res && strlen(res) > 1 && res[strlen(res)-1] == '\n')
          {
            char *new_val = strdup(res);
            new_val[strlen(new_val)-1] = 0;
            set_string_value(variables, curr->params[0], new_val);
            free(new_val);
          }
        }
      }
      break;
      case SPAWN_ACTION:
      {
        int child;
        char **parameters;
        char **newenv = copy_environment();
        const char *program = get_string_value(variables, curr->action);
        if (!program)
          program = curr->action;
        parameters = split_string(program);
        child = vfork();
        if (child == -1) /* error */
        {
          perror("vfork");
        }
        else if (child == 0) /* child */
        {
          char **newparams = perform_redirections(parameters);
          res = execve(newparams[0], newparams, newenv);
          if (res == -1)
          {
            perror("execvp");
            release_params(newparams);
            _exit(2);
          }
          /*release_params(newparams);*/
        }
        else /* parent */
        {
        }
        release_params(newenv);
        release_params(parameters);
      }
      break;
      case EXECUTE_ACTION:
      {
        char **parameters = split_string(curr->action);
        char **newparams = perform_redirections(parameters);
        /* handle redirections */

        res = execv(newparams[0], newparams);
        if (res == -1)
        {
          perror("execvp");
          display_params(newparams);
          release_params(parameters);
        }
      }
      break;
      case SET_ACTION:
      {
        if (curr->parameters)
        {
          int i;
          char *tmp = NULL;
          char *val = strdup("");
          for (i=0; i<curr->parameters->used; i++)
          {
            asprintf(&tmp, "%s%s", val, name_lookup(variables, curr->parameters->elements[i]));
            val = replace_buffer(val, tmp);
          }
          if (curr->parameters->used > 1)
            set_string_value(variables, curr->action, name_lookup(variables, val));
          else
            set_string_value(variables, curr->action, val);
          free(val);
        }
        else
        {
          const char *rhs_value = name_lookup(variables, curr->params[0]);
          if (verbose())
            printf("setting %s to %s (%s)\n", curr->action, curr->params[0],
                   (rhs_value) ? rhs_value : "");
          set_string_value(variables, curr->action, rhs_value);
        }
        if ( strcmp(curr->action, "TRACE_STEPS") == 0)
          set_action_tracing(get_integer_value(variables, "TRACE_STEPS"));

      }
      break;
      case LINE_ACTION:
      {
        int linenum = 0;
        const char *lookup = get_string_value(variables, curr->params[0]);
        const char *data = get_string_value(variables, curr->action);
        char *result = NULL;
        if (!data)
          data = curr->action;
        if (lookup)
          linenum = atoi(lookup);
        else
          linenum = atoi(curr->params[0]);

        if (verbose())
          printf("getting line %d of %s\n", linenum, data);
        result = select_line(linenum, data);
        set_string_value(variables, "RESULT", result);
        free(result);
      }
      break;
      case EXIT_ACTION:
        printf("exiting\n");
        fflush(stdout);
        return -1;
        break;
      default:
        printf("unknown action type in execute_method\n");
      }
      if (action_tracing())
      {
        const char *action_result = get_string_value(variables, "RESULT");
        if (action_result) printf("RESULT: %s", action_result);
        printf("\n");
      }
    }

    curr = curr->next;
  }
  return 0;
}

void release_method(int id)
{
  /* remove all entries matching this id */
  method *curr = method_table;
  while (curr != NULL)
  {
    if (curr->id == id)
    {
      method *old = curr;

      /* special case removal from the head of the list */
      if (old == method_table)
        method_table = method_table->next;

      free(old->action);
      /* release the action parameters */
      if (old->params != NULL)
      {
        int p = 0;
        while (old->params[p] != NULL)
        {
          free(old->params[p]);
          p++;
        }
      }
      free(old->params);
      free(old);
    }
    curr = curr->next;
  }
}
