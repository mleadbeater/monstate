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
#include "symboltable.h"

int main(int argc, char *argv[])
{
	symbol_table st = init_symbol_table();
	
	set_string_value(st, "A", "100");
	set_string_value(st, "B", "50");
	set_integer_value(st, "C", get_integer_value(st, "A") + get_integer_value(st, "B"));
	
	printf("A (%d) + B (%d) = C (%d) \n", 
	        get_integer_value(st, "A"), 
	        get_integer_value(st, "B"), 
			get_integer_value(st, "C"));
	
    {
		int i = 0;
		while (i < argc) {
			set_integer_value(st, argv[i], i);
			i++;
		}
	}
	
	printf("All symbols:\n");
	dump_symbol_table(st);
	printf("After removing symbol A:\n");
	remove_symbol(st, "A");
	dump_symbol_table(st);
	free_symbol_table(st);	
	return 0;
}