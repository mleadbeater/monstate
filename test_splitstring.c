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
#include "splitstring.h"

/* tests the split_string() function */

void display_words(char *argv[])
{
	char **curr = argv;
	char *val = *curr++;
	int count = 0;
	while (val)
	{
		printf("%s\n", val);
		val = *curr++;
		count++;
	}
	printf("%d entries\n", count);
}

void release_words(char *argv[])
{
	char **curr = argv;
	char *val = *curr++;
	while (val)
	{
		free(val);
		val = *curr++;
	}
	free(argv);
}

int main(int argc, char *argv[])
{
	char **words;
	int i;
	
	for (i = 1; i<argc; i++) {
		printf("Processing: %s\n", argv[i]);
		words = split_string(argv[i]);
		printf("after splitting into parameters\n");
		display_words(words);
		fprintf(stderr, "processing redirections\n");
		words = perform_redirections(words);
		printf("after processing into parameters\n");
		display_words(words);
		release_words(words);
	}
	return 0;
}