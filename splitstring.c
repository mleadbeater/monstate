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
#include <ctype.h>
#include "symboltable.h"
#include "property.h"
#include "buffers.h"


char *interpret_escapes(const char *str)
{
	char ESCAPE = '\\';
	const char *in = str;
	char *result;
    char *out;
    if (!str) return NULL;
	result = malloc(strlen(str) + 1);
	out = result;
	while (*in)
	{
		char ch = *in++;
		if (ch == ESCAPE && *in)
		{
			ch = *in++;
			if (ch == 't')
				ch = '\t';
			else if (ch == 'n')
				ch = '\n';
			else if (ch == 'r')
				ch = '\r';
			else if (ch == '0')
				ch = '\0';
		}
		*out++ = ch;
	}
	*out = 0;
	return result;
}

void display_params(char *argv[])
{
	char **curr = argv;
	char *val = *curr;
	int count = 0;
	while (val)
	{
		printf("%s ", val);
		val = *(++curr);
		count++;
	}
/*	printf("(%d entries)", count);*/
}

void release_params(char *argv[])
{
	char **curr = argv;
	char *val = *curr;
	while (val)
	{
		free(val);
		val = *(++curr);
	}
	free(argv);
}

char **duplicate_params(const char *argv[])
{
	char **curr = argv;
	char *val = *curr;
	int i = 0;
	while (val)
	{
		val = *(++curr);
		i++;
	}
    curr = malloc( (i+1) * sizeof(char *));
    curr[i] = 0;
	while (--i >= 0)
        curr[i] = strdup(argv[i]); 
    return curr;
}


static void splitstr_finish(symbol_table params, int id, char *val)
{
	char sym_name[5];
	sprintf(sym_name, "%03d", id);
	set_string_value(params, sym_name, val);
}

/* 
  sometimes we need to execute other programs and we provide
  a redirection facility by looking through the parameter list
  for parameters beginning with '<' or '>'. 

  Note: pipes are not supported.
 */
char ** perform_redirections(const char *argv[])
{
	char **result;
	char **retained_param;
	const char **curr = argv;
	const char *val = *curr++;
	int params = 0;
	/* count parameters */
	while (val)
	{
		val = *curr++;
		params++;
	}
	curr = argv;
	val = *curr++;
	
	/* perform redirections and copy other parameters to the new result list */
	
	result = (char **) malloc( (params + 1) * sizeof(char *));
	retained_param = result;
	while (val)
	{
		if (*val == '<' || *val == '>')
		{
			int redirect_ok = 0;
			/* redirection */
			if (*val == '<') 
			{
				/* skip spaces after the '<' */
				const char *fname = val;
				fname++;
				while ( *fname && isspace(*fname) ) fname++;
				if (*fname) 
				{
					FILE *new_stdin;
					/* fprintf(stderr, "redirecting input from %s\n", fname); */
					new_stdin = freopen(fname, "r", stdin);
					if (new_stdin) {
						/* successful open, move other parameters */
						redirect_ok = 1;
					}
					else
					{
						perror("freopen"); /* TBD printf a better error message */
 					}
				}
			}
			else if (*val == '>') 
			{
				char *mode = "w";
				const char *fname = val;
				
				
				if (val[1] == '>') /* support '>>' for append redirection */
				{
					mode = "a";
					fname++; /* skip the extra '>' */
				}
				/* skip spaces after the '>' */
				fname++;
				while ( *fname && isspace(*fname) ) fname++;
				if (*fname) 
				{
					FILE *new_stdout;
					/* fprintf(stderr, "redirecting output to %s (%s)\n", fname, mode); */
					new_stdout = freopen(fname, mode, stdout);
					if (new_stdout) {
						/* successful open, move other parameters */
						redirect_ok = 1;
					}
					else
					{
						perror("freopen"); /* TBD printf a better error message */
 					}
				}
			}
			if (!redirect_ok)
			{
				*retained_param++ = strdup(val);
			}
		}
		else
			*retained_param++ = strdup(val);
			
		val = *curr++;
	}
	*retained_param = NULL;
	return result;
}


char ** split_string(const char *str)
{
	enum states { START, IN, IN_QUOTE, IN_DBLQUOTE, BETWEEN };
	enum toks { CHAR, SPC, TERM};
	int QUOTE = 0x27;
	int DBLQUOTE = '"';
	
	int state = START;
	int tok;
	const char *p = str;
	
	/* buffer to collect individual parameters into */

	/* start bufsize with a reasonable guess at the length of individual 
	    parameters within the string this value will grow it turns out to be too small
	*/
	int bufsize = 30; 
	char *buf = malloc(bufsize);
	char *out = buf;
	
	symbol_table params = init_symbol_table();
	int cur_param = 0;
	char **result = NULL;
	
	while (*p)
	{
		if (state == IN_QUOTE) 
		{
		    if (*p != QUOTE)
				tok = CHAR;
			else 
				tok = TERM;
		}
		else if (state == IN_DBLQUOTE) {
			if (*p != DBLQUOTE)
				tok = CHAR;
			else
				tok = TERM;
		}
		else if (!isspace(*p)  )
			tok = CHAR;
		else
			tok = SPC;

		switch (state)
		{
			case START:
				if (*p == QUOTE)
					state = IN_QUOTE;
				else if (*p == DBLQUOTE)
					state = IN_DBLQUOTE;
				else if (tok == CHAR) 
				{
					*out++ = *p;
					state = IN;
				}
				else if (tok == SPC)
					state = BETWEEN;					
				break;
			case IN:
				if (*p == QUOTE)
					state = IN_QUOTE;
				else if (*p == DBLQUOTE)
					state = IN_DBLQUOTE;
			    else if (tok == CHAR)
					*out++ = *p;
				else if (tok == SPC) {
					/* finished a token */
					*out = 0;
					splitstr_finish(params, cur_param, buf);
					cur_param++;
					out = buf;
					state = BETWEEN;
				}
				break;
			case BETWEEN:
			    if (*p == QUOTE)
					state = IN_QUOTE;
				else if (*p == DBLQUOTE)
					state = IN_DBLQUOTE;
				else if (tok == CHAR)
				{
					*out++ = *p;
					state = IN;
				}
				break;
			case IN_QUOTE:
				if (tok == TERM)
					state = IN;
				else if (tok == CHAR)
					*out++ = *p;
				break;
			case IN_DBLQUOTE:
				if (tok == TERM)
					state = IN;
				else if (tok == CHAR)
					*out++ = *p;
			    break;
			default:
			    printf("Error: unknown state %d when parsing a string.\n", state);
		}
		if ( (out - buf) == bufsize ) 
		{
			/* make more space for parameters */
			char *saved = buf;
			buf = malloc(bufsize + 128);
			memcpy(buf, saved, bufsize);
			out = buf + bufsize;
			bufsize += 128;
			free(saved);
		}
		p++;
	}
	*out = 0;
	if (state == IN) {
		splitstr_finish(params, cur_param, buf);
		cur_param++;
	}
	else if (state == IN_QUOTE || state == IN_DBLQUOTE) 
	{
		printf("Warning: unterminated string '%s' is ignored while parsing: '%s'\n", buf, str);
	}
	/*dump_symbol_table(params);*/
	result = (char **)malloc( (cur_param + 1) * sizeof(char *));
	result[cur_param] = NULL;
	while (--cur_param >= 0) {
		char sym_name[5];
		sprintf(sym_name, "%03d", cur_param);
		result[cur_param] = strdup(get_string_value(params, sym_name));
	}
	free_symbol_table(params);
    free(buf);
	return result;
}

void interpret_text(symbol_table variables, const char *property_group, const char *result_group, const char *buf)
{
    symbol_table properties = collect_properties(variables, property_group);

/*    const char *message_header = lookup_string_property(properties, property_group, "MESSAGE_HEADER", "");
    const char *message_separator = lookup_string_property(properties, property_group, "MESSAGE_SEPARATOR", "\r\n");
    const char *message_tail = lookup_string_property(properties, property_group, "MESSAGE_TERMINATOR", "");
    const char *field_header = lookup_string_property(properties, property_group, "FIELD_HEADER", "");
*/    const char *field_separator = lookup_string_property(properties, property_group, "FIELD_SEPARATOR", ": ");
    const char *field_tail = lookup_string_property(properties, property_group, "FIELD_TERMINATOR", "\r\n");

    const char *next = buf;
    char *fldsep = strstr(next, field_separator);
    char *fldterm = strstr(next, field_tail);
    int seplen = strlen(field_separator);
    int termlen = strlen(field_tail);
    /* if there are no more field terminators, we are done */
    while (fldterm) 
    {
        if (!fldsep) break;
        
        /* normally a field terminator wouldn't appear before the next separator
            but this can happen if not all sections of the output are in the 
            key-value form (eg if the response has header text)
         */
        if (fldterm - next > fldsep - next)
        {
            int keylen = fldsep - next;
            int valuelen = fldterm - fldsep - seplen;
            char *key = malloc(keylen + 1);
            char *value = malloc(valuelen + 1);
            memmove(key, next, keylen);
            key[keylen] = 0;
            memmove(value, fldsep + seplen, valuelen);
            value[valuelen] = 0;
            set_string_property(variables, result_group, key, value);
            free(key);
            free(value);
        }
        next = fldterm + termlen;
        fldsep = strstr(next, field_separator);
        fldterm = strstr(next, field_tail);
    }
    free_symbol_table(properties);
}

#ifdef TESTING

int main(int argc, char *argv[])
{
    symbol_table symbols = init_symbol_table();
    symbol_table results;
    const char *property_group = "MAIL";
    const char *result_group = "RESULTS";
    set_string_property(symbols, property_group, "MESSAGE_SEPARATOR", "\n");
    set_string_property(symbols, property_group, "MESSAGE_TERMINATOR", ": ");
    set_string_property(symbols, property_group, "FIELD_TERMINATOR", "\n");
    set_string_property(symbols, property_group, "FIELD_SEPARATOR", ": ");

    char *buf = strdup("From: x@y\nTo: a@b\nSubject: test\n");
    
    printf("%s", buf);

    interpret_text(symbols, property_group, result_group, buf);
    results = collect_matching(symbols, result_group);

    printf("results:\n");
    dump_symbol_table(results);
    free_symbol_table(results);
    free_symbol_table(symbols);
    
    free(buf);
}
#endif

