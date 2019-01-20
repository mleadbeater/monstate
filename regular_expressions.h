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
#ifndef __REGULAR_EXPRESSIONS_H__
#define __REGULAR_EXPRESSIONS_H__

#include <regex.h>
#include "symboltable.h"

typedef struct rexp_info
{
  char *pattern;
  int compilation_result;
  char *compilation_error;
  regex_t regex;
  regmatch_t *matches;
} rexp_info;

rexp_info *create_pattern(const char *pat);

int execute_pattern(rexp_info *info, const char *string);

void release_pattern(rexp_info *info);

int matches(const char *string, const char *pattern);

int is_integer(const char *string);

/* tests the precompiled pattern against the text provided and return zero if the
    match succeeded.
    
    If the entire matched pattern is returned in the symbol REXP_0 and 
    if there are any subexpressions, the corresponding matches are returned in REXP_1..REXP..n
*/
int find_matches(rexp_info *info, symbol_table variables, const char *text);

/* substitute the subt string into the text. Using the precompiled pattern. 

    If the precompiled pattern contains subexpressions, these are first extracted
    into variables by the use of find_matches().
*/
char * substitute_pattern(rexp_info *info, symbol_table variables, const char *text, const char *subst);

/* here is a way for users to iterate through each match */
typedef int (match_func)(const char *match, void *user_data);

int each_match(rexp_info *info, const char *text, match_func f, void *user_data);


#endif
