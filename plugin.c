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
#include <dlfcn.h>
#include <signal.h>
#include <unistd.h>

#include "symboltable.h"
#include "splitstring.h"
#include "property.h"
#include "options.h"
#include "plugin.h"

/* a list used to map plugin names to handles returned from dlopen */

struct plugin_info
{
  struct plugin_info *next;
  struct plugin_info *prev;
  char *library_name;
  void *library_handle;
};

struct plugin_info *plugins = NULL;

/* initialise the plugin list. Must be called before other routines are used */
void init_plugins()
{
  plugins = NULL;
}

/* create_plugin_record creates a record and adds it to the head of the list */

struct plugin_info *create_plugin_record(const char *library_name, void *library_handle)
{
  struct plugin_info *result = malloc(sizeof(struct plugin_info));
  if (!result)
    fprintf(stderr, "Unable to allocate plugin reference\n");
  else
  {
    result->next = NULL;
    result->prev = NULL;
    result->library_name = strdup(library_name);
    result->library_handle = library_handle;
    if (!plugins)
      plugins = result;
    else
    {
      result->next = plugins;
      plugins->prev = result;
      plugins = result;
    }
  }
  return result;
}

/* returns the plugin record for the given library name */

struct plugin_info *find_plugin_record(const char *library_name)
{
  struct plugin_info *pii = plugins;
  if (!library_name) return NULL;
  while (pii && strcmp(pii->library_name, library_name) != 0)
    pii = pii->next;
  return pii;
}

void remove_plugin_record(const char *library_name)
{
  struct plugin_info *result = find_plugin_record(library_name);
  if (result)
  {
    if (result->prev)
      result->prev->next = result->next;
    if (result->next)
      result->next->prev = result->prev;
    if (result == plugins)
      plugins = result->next;
    free(result->library_name);
    free(result);
  }
}

/* we can retain plugins, expecting it to improve performance at the cost of a little ram.
   These retained plugins can be released at any time, however they are likely to be retained again
   on next use.
 */
void release_plugins()
{
  struct plugin_info *pii = plugins;
  while (pii)
  {
    struct plugin_info *next = pii->next;
    free(pii->library_name);
    dlclose(pii->library_handle);
    free(pii);
    pii = next;
  }
  plugins = NULL;
}

/* we set a timer which we use to abort our plugin if necessary */

static sig_t saved_alarm_sig = NULL;
static char *timeout_message = NULL;

static void timeout_handler(int sig)
{
  if (timeout_message)
    fprintf(stderr,"%s\n",timeout_message);
  exit(7);
}

#if 0
static void setup_signals()
{
  siginterrupt(SIGALRM, 1); /* this code handles EINTR results */
  saved_alarm_sig = signal(SIGALRM, timeout_handler);
}

static void cleanup_signals()
{
  alarm(0);
  if (saved_alarm_sig)
    signal(SIGALRM, saved_alarm_sig);
  else
    signal(SIGALRM, SIG_DFL);

  saved_alarm_sig = 0;
  siginterrupt(SIGALRM, 0); /* restore default behaviour of restarting system calls */
}
#endif

int count_params(char *argv[])
{
  char **curr = argv;
  char *val = *curr;
  int count = 0;
  while (val)
  {
    val = *(++curr);
    count++;
  }
  return count;
}

int plugin(symbol_table variables, const char *command, const char **params)
{
  int result = 0;
  char **parameters;

  /*
      For backward compatibility, we manually split parameters if the
      user wrote something like: CALL plugin; or CALL "plugin -x"
  */
  if (params && params[0] && params[1])
    parameters = duplicate_params(params);
  else
    parameters = split_string(command);
  if (parameters != NULL)
  {
    void *mylib_handle = NULL;
    const char *library_name;
    const char *property_group = parameters[0];
    plugin_function func;

    library_name = lookup_string_property(variables, property_group, "LIBRARY", NULL);
    if (library_name)
    {
      const char *retain;
      struct plugin_info *pii = find_plugin_record(library_name);
      if (!pii)
      {
        retain = lookup_string_property(variables, property_group, "RETAIN", "YES");
        mylib_handle = dlopen(library_name, RTLD_LAZY);
        if (mylib_handle)
          pii = create_plugin_record(library_name, mylib_handle);
      }
      else
      {
        retain = "YES";
        mylib_handle = pii->library_handle;
      }
      if (mylib_handle == NULL)
      {
        fprintf(stderr, "unable to open library %s: %s\n", library_name, dlerror());
      }
      else
      {
        int buflen = lookup_int_property(variables, property_group, "MAXBUFSIZE", 0);
        char *buf = NULL;
        remove_properties(variables, "RESULT");
        if (buflen)
          buf = malloc(buflen);
        dlerror(); /* reset errors */
        func = dlsym((void *)mylib_handle, "plugin_func");
        if (func)
        {
          sig_t saved_alarm_sig = NULL;
          char *data;

          int timeout_secs = get_integer_value(variables, "TIMEOUT");
          if (!found_key(variables))
            timeout_secs = 5;
          if ( timeout_secs == 0)
          {
            saved_alarm_sig = signal(SIGALRM, SIG_DFL);
            alarm(0);
          }
          else
          {
            siginterrupt(SIGALRM, 1); /* this code causes EINTR results rather than restarts */
            saved_alarm_sig = signal(SIGALRM, timeout_handler);
            asprintf(&timeout_message, "Plugin timeout (%d sec). Aborting.", timeout_secs);
            alarm(timeout_secs);
          }
          data = func(variables, buf, buflen, count_params(parameters), parameters);
          alarm(0); /* end any outstanding alarm */
          if (saved_alarm_sig)
            signal(SIGALRM, saved_alarm_sig);
          if (timeout_message)
          {
            char *msg = timeout_message;
            timeout_message = NULL;
            free(msg);
          }
          if (verbose() && data)
          {
            printf("result of %s: %s\n", command, data);
          }
          if (data)
          {
            set_string_value(variables, "RESULT", data);
            if (data != buf)
              free(data);
            result = PLUGIN_COMPLETED;
          }
          else
          {
            set_string_value(variables, "RESULT", "");
            result = PLUGIN_ERROR;
          }
        }
        else
        {
          const char *template = "unable to find symbol 'plugin_func' in library.\n%s\n";
          char *message;
          const char *errtext = dlerror();
          message = malloc(strlen(template) + strlen(library_name) + strlen(errtext));
          sprintf(message, template, library_name, errtext);
          fprintf(stderr, message, template, library_name, errtext);

          set_string_value(variables, "RESULT", message);
          free(message);
          result = PLUGIN_ERROR;
        }
        if (!retain || (strcmp(retain, "YES") != 0 && strcmp(retain, "TRUE") != 0) )
        {
          dlclose(mylib_handle);
          remove_plugin_record(library_name);
        }
        if (buf)
          free(buf);
      }
    }
    else
      result = NO_PLUGIN_AVAILABLE;

    release_params(parameters);
  }
  return result;
}
