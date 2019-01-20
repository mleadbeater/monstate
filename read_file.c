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
#include "symboltable.h"
#include "options.h"
#include "plugin.h"
#include "property.h"

#ifdef BUILD_PLUGIN
EXPORT
#else
static
#endif
char *plugin_func(symbol_table variables, char *user_buf, int user_buflen, int argc, const char *argv[])
{
    char *buf = user_buf;
    int buflen = user_buflen;
	if (argc != 2)
	{
		return NULL;
	}
	else
	{
		const char *filename = name_lookup(variables, argv[1]);
		FILE *f = fopen(filename, "r");
		if (verbose())
			printf("priming buffer from file %s\n", filename);
		if (f != NULL)
		{
			int num_read;
            long offset;
            if (!buf) {
                if (!buflen)
                    buflen = lookup_int_property(variables, argv[0], "MAXBUFSIZE", 5000);
                buf = malloc(buflen);
            }
            fseek(f, 0, SEEK_END);
            offset = ftell(f);
            if (offset > buflen)
                fseek(f, -buflen+1, SEEK_CUR);
            else
                rewind(f);

			num_read = fread(buf, 1, buflen-1, f);
			if (num_read < 0)
			{
				perror("reading");
				if (buf != user_buf) 
                {
                    free(buf);
                    buf = NULL;
                }
			}
			else
				buf[num_read] = 0;
			fclose(f);
            return buf;
		}
		else
			return NULL;
	}
	return buf;
}

int read_file(symbol_table variables, char *buf, int buflen, int argc, const char *argv[])
{
    return (plugin_func(variables, buf, buflen, argc, argv)) ? 0 : -1;
}

#ifdef TESTING_PLUGIN
int main(int argc, const char *argv[])
{
	int buflen = 0;
	char *buf = NULL;
    char *res = NULL;
    symbol_table symbols = init_symbol_table();
	/*buflen = 500;
	buf = malloc(buflen);
    */
	res = plugin_func(symbols, buf, buflen, argc, argv);
    set_verbose(1);
	if (!res) return 1;
	printf("%s", res);
	free_symbol_table(symbols);
    if (res!= buf) free(res);
	if (buf)free(buf);
	return 0;
}
#endif
