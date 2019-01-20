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
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include "symboltable.h"
#include "plugin.h"

#if 0
static int alternate_method(symbol_table variables, char *buf, int buflen, int argc, char *argv[])
{
	struct tm tm_buf;
	time_t now;
	now = time(NULL);
	localtime_r(&now, &tm_buf);
	sprintf(buf, "%s", asctime(&tm_buf));
	return 0;	
}
#endif

EXPORT
char *plugin_func(symbol_table variables, char *buf, int buflen, int argc, char *argv[])
{
    int numeric = 0;
	time_t now = time(NULL);
    if (argc > 1 && strcmp(argv[1],"-n") == 0)
        numeric = 1;
	if (!buf || buflen < 30)
        buf = malloc(30);

    if (numeric)
        sprintf(buf, "%ld", (long)now);
    else
        ctime_r(&now, buf);
	return buf;	
}

#ifdef TESTING_PLUGIN

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
	const char **curr = argv;
	const char *val = *curr;
	char **res = 0;
	int i = 0;
	while (val)
	{
		val = *(++curr);
		i++;
	}
    res = (char **)malloc( (i+1) * sizeof(char *));
    res[i] = 0;
	while (--i >= 0)
        res[i] = strdup(argv[i]); 
    return res;
}


int main(int argc, const char *argv[])
{
	int buflen = 500;
	char *buf = malloc(buflen);
	symbol_table symbols;
	char *res;
    char **args = duplicate_params(argv);
	
	symbols = init_symbol_table();
	res = plugin_func(symbols, buf, buflen, argc, args);
	free_symbol_table(symbols);
	if (res)
	{
		printf("%s\n", res);
		if (res != buf)
			free(res);
	}
	free(buf);
    release_params(args);
	return 0;
}
#endif
