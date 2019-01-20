#include <stdlib.h>
#include <stdio.h>
#include <time.h>
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
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include "symboltable.h"
#include "plugin.h"

/*
	WriteFile - writes a variable to a file
 */

EXPORT
char *plugin_func(symbol_table variables, char *buf, int buflen, int argc, char *argv[])
{
	const char *usage_message = "WriteFile usage: WriteFile variable filename\n";
	if (argc != 3)
	{
		if (buf && buflen > strlen(usage_message))
		{
			sprintf(buf, "WriteFile usage: WriteFile variable filename\n");
		}
        return NULL;
	}
	{
		int res;
		int outf;
		const char *data = name_lookup(variables, argv[1]);
		const char *filename = name_lookup(variables, argv[2]);
		int n;
		outf = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (outf == -1)
		{
			fprintf(stderr, "open: %s writing to %s\n", strerror(errno), filename);
			return NULL;
		}
		n = strlen(data);
		res = write(outf, data, n);
		if (res == -1)
		{
			perror("write");
			return NULL;
		}
		if (res != n)
		{
			fprintf(stderr, "warning, incorrect amount of data written. Was %d, should be %d\n", res, n);
			return NULL;
		}
		res = close(outf);
		if (res == -1)
		{
			perror("close");
			return NULL;
		}
	}
	return buf;	
}

#ifdef TESTING_PLUGIN
int main(int argc, const char *argv[])
{
	int buflen = 0;
	char *buf = NULL;
	symbol_table symbols = init_symbol_table();
	buf = plugin_func(symbols, buf, buflen, argc, argv);
	free_symbol_table(symbols);
	if (buf) {
		printf("%s", buf);
		free(buf);
	}
	return 0;
}
#endif
