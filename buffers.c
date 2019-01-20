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
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "buffers.h"

parameter_list init_parameter_list(int start_size)
{
    int len = (start_size+1) * sizeof(char *);
    parameter_list result = malloc( sizeof (struct parameter_list) );
    result->num_elements = start_size;
    result->used = 0;
    result->elements = malloc(len);
    memset(result->elements, 0, len);
    return result;
}

void add_parameter(parameter_list params, const char *new_value)
{
    int i;
	if ( params->num_elements == params->used)
    {
        int len = (params->num_elements + 8 + 1) * sizeof(char *);
        char **new_elements = malloc( len);
        memset(new_elements, 0, len);
        for(i = 0; i<params->used; i++)
            new_elements[i] = params->elements[i];
        free(params->elements);
        params->elements = new_elements;
        params->num_elements += 8;
    }
#if 0
    for (i=0; i<params->used; i++)
        params->elements[i+1] = params->elements[i];
    params->elements[0] = strdup(new_value);
    params->used++;
#endif
    params->elements[params->used++] = strdup(new_value);
}

void free_parameter_list(parameter_list params)
{
    int i;
    for (i=0; i<params->used; i++)
        free(params->elements[i]);
    free(params->elements);
    free(params);
}


char *extend_buffer(char *buffer, int n)
{
	char *result;
	if (buffer && n>0)
	{	
		result = malloc(strlen(buffer) + n + 1);
		strcpy(result, buffer);
		free(buffer);
		return result;
	}
	else
		return buffer;
}

char *append_buffer(char *output, char *new_output)
{
	char *result;
	if (output && new_output)
	{	
		result = malloc(strlen(output) + strlen(new_output) + 1);
		strcpy(result, output);
		strcat(result, new_output);
		free(output);
		free(new_output);
		return result;
	}
	else if (new_output)
		return new_output;
	else
		return output;
}

/* replace_buffer is intended to be called just after allocating space
   for a new buffer via asprintf.  It includes a warning message if the
   malloc failed.

   Do not feel obliged to use this.
*/

char *replace_buffer(char *buf, char *new_buf)
{
	if (!new_buf)
		fprintf(stderr, "WARNING, unable to allocate space for printf");
	else
	{
		if (buf) free(buf);
		buf = new_buf;
	}
	return buf;
}
