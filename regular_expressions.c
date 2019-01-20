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
#include <unistd.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "regular_expressions.h"
#include "symboltable.h"

rexp_info *create_pattern(const char *pat)
{
  rexp_info *info = malloc(sizeof(rexp_info));
  info->pattern = strdup(pat);
  info->compilation_error = NULL;
  info->matches = NULL;
  info->compilation_result = regcomp(&info->regex, pat, REG_EXTENDED);
  if (info->compilation_result != 0)
  {
    char buf[80];
    regerror(info->compilation_result, &info->regex, buf, 80);
    info->compilation_error = strdup(buf);
  }
  else
  {
    /* compiled ok, make space for matches */
    info->matches = malloc(sizeof(regmatch_t) * (info->regex.re_nsub+1));
  }
  return info;
}

int execute_pattern(rexp_info *info, const char *string)
{
	int res = 0;
	if (info->compilation_result == 0)
	 	res = regexec(&info->regex, string, info->regex.re_nsub+1, info->matches, 0);
    return res;
}

/* tests the precompiled pattern against the text provided and return zero if the
    match succeeded.
    
    If the entire matched pattern is returned in the symbol REXP_0 and 
    if there are any subexpressions, the corresponding matches are returned in REXP_1..REXP..n
*/
int find_matches(rexp_info *info, symbol_table variables, const char *string)
{
	int res = 0;
    remove_matching(variables, "REXP_"); /* clear any previous matches */
	if (info->compilation_result == 0)
	 	res = regexec(&info->regex, string, info->regex.re_nsub+1, info->matches, 0);
    
    if (res == 0)
    {
        int i;
        char match_name[15];
        /* using length of entire match for a string buffer to avoid remallocing buf for each one */
        int len = info->matches[0].rm_eo - info->matches[0].rm_so + 1;
        char *match_buf = malloc(len);
        
        for (i=0; i <= info->regex.re_nsub; i++)
        {
            int so = info->matches[i].rm_so;
            int eo = info->matches[i].rm_eo;
            if (so != -1 && eo != -1) {
                sprintf(match_name, "REXP_%d", i);
                memcpy(match_buf, string+so, eo - so);
                match_buf[eo-so] = 0;
                set_string_value(variables, match_name, match_buf);
            }
        }
        free(match_buf);
    }
    
	return res;
}

/* substitute the subt string into the text. Using the precompiled pattern. 

    If the precompiled pattern contains subexpressions, these are first extracted
    into variables by the use of find_matches().
*/
char * substitute_pattern(rexp_info *info, symbol_table variables, const char *string, const char *subst)
{
    int matched_ok;
    if (info->regex.re_nsub > 0)
        matched_ok = find_matches(info, variables, string);
    else
        matched_ok = execute_pattern(info, string);

    if (matched_ok == 0)
    {
        int subst_len = strlen(subst);
        int str_len = strlen(string);
        int remainder = strlen(string);
        char *result = malloc(remainder + (info->regex.re_nsub+1)*subst_len + 1);
        char *p = result;
        const char *q = string;
        int prev_eo = 0;
        int i = 0;
        /* normally this substitution is intended to replace the entire 
            match with a substitute string. The code below allows for
            an idea where we might substitute just the subexpressions
            with the replacement string. That idea is flawed, however,
            since regexec only returns the last matching substring anyway.
            
            If you want to enable that behaviour, insert the following
            to create a loop.
            
                    i = 1;
                    while (i <= info->regex.re_nsub)

         */
        {
            int so = info->matches[i].rm_so;
            int eo = info->matches[i].rm_eo;
            if (so != -1 && eo != -1)
            {
                if (so > 0) memcpy(p, q, so-prev_eo);
                p += so - prev_eo;
                /*prev_eo = eo;*/
                strcpy(p, subst);
                p += subst_len;
                q = string + eo;
                remainder = str_len - eo;
            }
            /*i++; */
        }
        memcpy(p, q, remainder );
        p += remainder;
        *p = 0;
        return result;
    }
    return strdup(string);
}

int each_match(rexp_info *info, const char *text, match_func f, void *user_data)
{
    int matched_ok;
    matched_ok = execute_pattern(info, text);
    int offset = 0;
    int result = 0;
    while (matched_ok == 0)
    {
        char *match;
        int so = info->matches[0].rm_so;
        int eo = info->matches[0].rm_eo;
        if (eo != -1 && so != -1 && eo>so && result == 0)
        {  
            match = malloc(eo - so + 1);
            memmove(match, text + offset + so, eo - so);
            match[eo-so] = 0;
            result = f(match, user_data);
            free(match);
            offset += eo;
            matched_ok = execute_pattern(info, text+offset);
        }
        else {
            matched_ok = 1;
        }
    }
    return result;
}

void release_pattern(rexp_info *info)
{
  regfree(&info->regex);
  if (info->compilation_error != NULL)
    free(info->compilation_error);
  free(info->pattern);
  if (info->matches != NULL)
    free(info->matches);
  free(info);
}

int matches(const char *string, const char *pattern)
{
	int result = 1;
	rexp_info *info = create_pattern(pattern);
	if (info->compilation_result == 0)
		result = execute_pattern(info, string);
	else
		fprintf(stderr, "failed to compile regexp\n");
	release_pattern(info);
	return result == 0;
}

int is_integer(const char *string)
{
	return matches(string, "^[-]{0,1}[0-9]+$");
}

#ifdef TESTING


static char *interpret_escapes(const char *str)
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

void display_matches(rexp_info *info, symbol_table symbols)
{
    /* find_matches will have created symbols REXP_0..REXP_n if 
         the respective subexpression matched */
    int i;
    const char *match = NULL;
    for (i=0; i<= info->regex.re_nsub; i++)
    {
        char name[15];
        sprintf(name, "REXP_%d", i);
        match = get_string_value(symbols, name);
        if (!match) match = "";
        printf("match: %s = %s\n", name, match);
    }
}

/* test routine.

  usage: test_regexp [-p pattern] [-s substitution] text ...
  
  example usage:
  
  test_regexp -p '[a-z]+[ ]*=[ ]*([0-9]+)[ ]*;' -s '' 'a=1; b=2'
  
  which should give: 
  
  a=1; b=2 matches [a-z]+[ ]*=[ ]*([0-9]+)[ ]*; 
  substuting :  b=2
  match: REXP_0 = a=1;
  match: REXP_1 = 1

*/

void my_match_func(const char *match, void *data)
{
    printf("match: %s\n", match);
}

int main(int argc, char *argv[])
{
	int i;
    char *pattern = strdup("^((-[0-9]+)|([0-9]+))$");
    char *subs = NULL;
    int expecting_pattern = 0;
    int expecting_subst = 0;
    symbol_table symbols = init_symbol_table();
	for (i=1; i<argc; i++)
	{
		if (expecting_pattern)
        {
            free(pattern);
            pattern = interpret_escapes(argv[i]);
            expecting_pattern = 0;
        }
		else if (expecting_subst)
        {
            if (subs) free(subs);
            subs = interpret_escapes(argv[i]);
            expecting_subst = 0;
        }
        else if (strcmp(argv[i], "-p") == 0)
		    expecting_pattern = 1;
        else if (strcmp(argv[i], "-s") == 0)
		    expecting_subst = 1;
		else 
        {
            char *text = interpret_escapes(argv[i]);
            rexp_info *info;
            if (matches(text, pattern))
                printf("%s matches %s \n", text, pattern);

            info = create_pattern(pattern);
            each_match(info, text, my_match_func, NULL);
            if (subs)
            {
                char *s = substitute_pattern(info, symbols, text, subs);
                if (s) {
                    printf("substuting %s: %s\n", subs, s);
                    free(s);
                }
                display_matches(info, symbols);
            }
            else
                if (find_matches(info, symbols, text) == 0)
                {
                    display_matches(info, symbols);
                }

            
            free(text);
        }
	}
    free_symbol_table(symbols);
	return 0;
}
#endif


