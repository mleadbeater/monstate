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
#include "symboltable.h"
#include "regular_expressions.h"

/*
This symbol table is intended to handle only a small number of symbols (half a dozen or so)
but it should handle more if performance isn't a major factor
*/
#define _SYMBOLTABLE_TYPECHECK_ 4393824L
	
typedef struct var_symbol
{
	char *name;
	char *value;
} var_symbol;


typedef struct symbol_table_internal
{
	long typecheck_id;
	int num_entries;
	int page_size; /* grow the symbol table in this size chunks */
	int table_size;
	int found_key;
	var_symbol *sym;
} symbol_table_internal;
typedef symbol_table_internal *stp;

symbol_table_internal *reveal(symbol_table hidden)
{
	long *check = (long*)hidden;
	if ( check != NULL && *check == _SYMBOLTABLE_TYPECHECK_)
		return (symbol_table_internal *)hidden;
	else
	{
		fprintf(stderr, "error: invalid symboltable value\n");
		return NULL;
	}
}

/* initialisation of the class */
symbol_table init_symbol_table()
{
	symbol_table_internal *result = malloc(sizeof(symbol_table_internal));
	result->typecheck_id = _SYMBOLTABLE_TYPECHECK_;
	result->num_entries = 0;
	result->table_size = 0;
	result->page_size = 8;
	result->sym = NULL;
	return (symbol_table)result;
}

/* release the memory occupied by the symbol table */
void free_symbol_table(symbol_table st)
{
	symbol_table_internal *symbol_table_p = reveal(st);
	int num_entries = symbol_table_p->num_entries;
	int i;
	for (i=0; i<num_entries; i++)
	{
		free(symbol_table_p->sym[i].name);
		free(symbol_table_p->sym[i].value);
	}
	free(symbol_table_p->sym);
	free(symbol_table_p);
}

/* display entire symbol table */
void dump_symbol_table(symbol_table st)
{
	symbol_table_internal *symbol_table_p = reveal(st);
	int num_entries = symbol_table_p->num_entries;
	int i;
	for (i=0; i<num_entries; i++)
		printf("%s: %s\n", symbol_table_p->sym[i].name, symbol_table_p->sym[i].value);
}

const char *name_lookup(symbol_table st, const char *name)
{
    const char *result = get_string_value(st, name);
    if (!result)
        return name;
    return result;
}

int found_key(symbol_table st)
{
	symbol_table_internal *symbol_table_p = reveal(st);
	return symbol_table_p->found_key;
}

/* return a new symbol table with all the symbols with a name matching the pattern */
symbol_table collect_matching(symbol_table st, const char *pattern)
{
	rexp_info *info = create_pattern(pattern);
	symbol_table result = init_symbol_table();
	
	/*fprintf(stderr, "attempting to find symbols matching %s\n", pattern);*/
	
	symbol_table_internal *symbol_table_p = reveal(st);
	int num_entries = symbol_table_p->num_entries;
	int i;
	for (i=0; i<num_entries; i++)
	{
		if (execute_pattern(info, symbol_table_p->sym[i].name) == 0)
		{
			set_string_value(result, symbol_table_p->sym[i].name, symbol_table_p->sym[i].value);
		}
	}
    release_pattern(info);
	return result;	
}



/* returns the address of the symbol if found, otherwise, 
  the address of the end of the table 
 */

static int find_symbol_address(symbol_table st, const char *name)
{
	symbol_table_internal *symbol_table_p = reveal(st);
	int num_entries = symbol_table_p->num_entries;
	int i;
	symbol_table_p->found_key = 0;
	for (i=0; i<num_entries; i++)
	{
		if (strcmp(symbol_table_p->sym[i].name, name) == 0)
		{
			symbol_table_p->found_key = 1;
			return i;
		}
	}
	return num_entries;
}


void remove_entry(symbol_table st, int pos)
{
	symbol_table_internal *symbol_table_p = reveal(st);
	if (pos < symbol_table_p->num_entries)
	{
		free(symbol_table_p->sym[pos].name);
		free(symbol_table_p->sym[pos].value);
		while (pos < symbol_table_p->num_entries-1)
		{
			symbol_table_p->sym[pos] = symbol_table_p->sym[pos+1];
			pos++;
		}
		symbol_table_p->num_entries--;
	}
	
}

void remove_symbol(symbol_table st, const char *name)
{
	remove_entry(st, find_symbol_address(st, name));
}

/* return a new symbol table with all the symbols with a name matching the pattern */
void remove_matching(symbol_table st, const char *pattern)
{
	rexp_info *info = create_pattern(pattern);	
	symbol_table_internal *symbol_table_p = reveal(st);
	int i = 0;
	while (i < symbol_table_p->num_entries)
	{
		if (execute_pattern(info, symbol_table_p->sym[i].name) == 0)
			remove_entry(st, i);
		else
			i++;
	}
    release_pattern(info);
	return;	
}

void each_symbol(symbol_table st, symbol_func f, void *user_data)
{
	symbol_table_internal *symbol_table_p = reveal(st);
	int num_entries = symbol_table_p->num_entries;
	int i;
	for (i=0; i<num_entries; i++)
		f(symbol_table_p->sym[i].name, symbol_table_p->sym[i].value, user_data);
}


/* 
   attempt to access any symbol ending with the given string.
   if the symbol is not known, NULL is returned otherwise the 
   matching name is returned
 */
const char *get_string_value_ending(symbol_table st, const char *suffix)
{
	symbol_table_internal *symbol_table_p = reveal(st);
	int num_entries = symbol_table_p->num_entries;
	int i;
	symbol_table_p->found_key = 0;
	for (i=0; i<num_entries; i++)
	{
		const char *name = symbol_table_p->sym[i].name;
		int offset = 0;
		if (strlen(name) > strlen(suffix))
			offset += (strlen(name) - strlen(suffix));
		if (strcmp(name + offset, suffix) == 0)
		{
			symbol_table_p->found_key = 1;
			return name;
		}
	}
	return NULL;	
}


/* 
   attempt to access any symbol ending with the given string.
   if the symbol is not known, NULL is returned otherwise the 
   matching name is returned
 */
const char *get_string_value_beginning(symbol_table st, const char *prefix)
{
	symbol_table_internal *symbol_table_p = reveal(st);
	int num_entries = symbol_table_p->num_entries;
	int i;
	symbol_table_p->found_key = 0;
	for (i=0; i<num_entries; i++)
	{
		const char *name = symbol_table_p->sym[i].name;
		if (strncmp(name, prefix, strlen(prefix)) == 0)
		{
			symbol_table_p->found_key = 1;
			return name;
		}
	}
	return NULL;	
}

static void set_entry_value(symbol_table st, int address, const char *value)
{
	symbol_table_internal *symbol_table_p = reveal(st);
	char *old;
	if (value == NULL) return; /* do not want null entries */	
	if (address == symbol_table_p->table_size) return; /* just in case */
	
	old = symbol_table_p->sym[address].value;
	symbol_table_p->sym[address].value = strdup(value);
	if (old != NULL)
		free(old);
}

static void set_entry_name(symbol_table st, int address, const char *name)
{
	symbol_table_internal *symbol_table_p = reveal(st);
	/* we have to be careful in the situation where we are 
	    adding a new name.  Don't free() the old, empty one.
	 */
	if (name == NULL) return; /* do not want null entries */	
	if (address == symbol_table_p->table_size) return; /* just in case */
	if (address == symbol_table_p->num_entries) 
	{
		symbol_table_p->num_entries++;
		symbol_table_p->sym[address].name = strdup(name);
		symbol_table_p->sym[address].value = NULL;
	}
	else 
	{
		char *old = symbol_table_p->sym[address].name;
		symbol_table_p->sym[address].name = strdup(name);
		free(old);
	}
}

/* attempt to access the given symbol as an integer.
   if the symbol is not known or is not a number, zero is returned.
*/
int get_integer_value(symbol_table st, const char *name)
{
	symbol_table_internal *symbol_table_p = reveal(st);
	int pos = find_symbol_address(st, name);
	if (pos < symbol_table_p->num_entries)
		return atoi(symbol_table_p->sym[pos].value);
	return 0;
}

/* 
   attempt to access the given symbol as a string.
   if the symbol is not known, NULL is returned.
 */
const char *get_string_value(symbol_table st, const char *name)
{
	symbol_table_internal *symbol_table_p = reveal(st);
	int pos = find_symbol_address(st, name);
	if (pos < symbol_table_p->num_entries)
		return symbol_table_p->sym[pos].value;
	return NULL;
}

/*
   update the given string symbol with a new value
   if the symbol is not known, it is added to the symbol table
 */
void set_string_value(symbol_table st, const char *name, const char *value)
{
	symbol_table_internal *symbol_table_p = reveal(st);
	int pos = find_symbol_address(st, name);
	if (pos == symbol_table_p->num_entries) /* did not find the symbol */
	{
		if (pos == symbol_table_p->table_size) /* table is full */
		{
			long unit_size = sizeof(struct var_symbol);
			/* grow the symbol table */
			var_symbol *new_table;
			int bytes_used = unit_size * symbol_table_p->table_size;
			int new_bytes;
			symbol_table_p->table_size += symbol_table_p->page_size;
			new_bytes = unit_size * symbol_table_p->table_size;

			new_table = malloc(new_bytes);
			memset(new_table, 0, new_bytes);
			if (bytes_used > 0)
			{
				memcpy(new_table, symbol_table_p->sym, bytes_used);
				free(symbol_table_p->sym);
			}
			symbol_table_p->sym = new_table;
		}
		set_entry_name(st, pos, name);
	}
	set_entry_value(st, pos, value);
}

/*
   update the given string symbol with a new value from an integer
   if the symbol is not known, it is added to the symbol table
 */
void set_integer_value(symbol_table st, const char *name, int value)
{
	char *tmp = malloc(20); /* should be large enough for most values */
	sprintf(tmp, "%d", value);
	set_string_value(st, name, tmp);
	free(tmp);
}

/* search the symbol table for items matching given values */
const char *find_symbol_with_int_value(symbol_table st, int search)
{
	symbol_table_internal *symbol_table_p = reveal(st);
	int num_entries = symbol_table_p->num_entries;
	int i;
    char *result = NULL;
	char *tmp = malloc(20); /* should be large enough for most values */
	sprintf(tmp, "%d", search);
	symbol_table_p->found_key = 0;
	for (i=0; i<num_entries; i++)
	{
		const char *value = symbol_table_p->sym[i].value;
		if (strcmp(value, tmp) == 0)
		{
			symbol_table_p->found_key = 1;
			result = symbol_table_p->sym[i].name;
            break;
		}
	}
    free(tmp);
	return result;	
}

const char *find_symbol_with_string_value(symbol_table st, const char *search)
{
	symbol_table_internal *symbol_table_p = reveal(st);
	int num_entries = symbol_table_p->num_entries;
	int i;
	for (i=0; i<num_entries; i++)
	{
		const char *value = symbol_table_p->sym[i].value;
		if (strcmp(value, search) == 0)
		{
			symbol_table_p->found_key = 1;
			return symbol_table_p->sym[i].name;
		}
	}
	return NULL;
}
