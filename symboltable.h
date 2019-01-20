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
#ifndef __SYMBOLTABLE_H__
#define __SYMBOLTABLE_H__

/* a simple symbol table */
typedef struct symbol_table { long unused; } *symbol_table;

/* initialisation of the class */
symbol_table init_symbol_table();

/* display the symbol table to stdout */
void dump_symbol_table(symbol_table st);

/* release all memory used by the symbol table */
void free_symbol_table(symbol_table st);

/* returns the value of the symbol if the name is found, otherways the symbol name */
const char *name_lookup(symbol_table st, const char *name);

/* here is a way for users to iterate through each element of a symbol table */
typedef void (symbol_func)(const char *name, const char *value, void *user_data);

void each_symbol(symbol_table st, symbol_func f, void *user_data);

void remove_symbol(symbol_table st, const char *name);

/* remove all the symbols with a name matching the pattern */
void remove_matching(symbol_table st, const char *pattern);

/* return a new symbol table with all the symbols with a name matching the pattern */
symbol_table collect_matching(symbol_table st, const char *pattern);


/*
   query whether the last lookup on a symbol table successfully found a key
 */
int found_key(symbol_table st);

/* attempt to access the given symbol as an integer.
   if the symbol is not known or is not a number, zero is returned.
*/
int get_integer_value(symbol_table st, const char *name);

/* 
   attempt to access the given symbol as a string.
   if the symbol is not known, NULL is returned.
 */
const char *get_string_value(symbol_table st, const char *name);

/* 
   attempt to access any symbol ending with the given string.
   if the symbol is not known, NULL is returned.
 */
const char *get_string_value_ending(symbol_table st, const char *suffix);

/* 
   attempt to access any symbol beginning with the given string.
   if the symbol is not known, NULL is returned.
 */
const char *get_string_value_beginning(symbol_table st, const char *prefix);

/*
   update the given string symbol with a new value
   if the symbol is not known, it is added to the symbol table
 */
void set_string_value(symbol_table st, const char *name, const char *value);

/*
   update the given string symbol with a new value from an integer
   if the symbol is not known, it is added to the symbol table
 */
void set_integer_value(symbol_table st, const char *name, int value);


/* search the symbol table for items matching given values */
const char *find_symbol_with_int_value(symbol_table st, int value);
const char *find_symbol_with_string_value(symbol_table st, const char *value);

#endif
