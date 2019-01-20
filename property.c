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
#include <string.h>
#include <stdio.h>
#include "symboltable.h"
#include "options.h"
#include "property.h"
#include "regular_expressions.h"

symbol_table collect_properties(symbol_table symbols, const char *property_group)
{
	symbol_table result;
	char *tmp_str = malloc(strlen(property_group) + 3);
	sprintf(tmp_str, "^%s_", property_group);
	result = collect_matching(symbols, tmp_str);
    free(tmp_str);
	return result;
}

const char *lookup_string_property(symbol_table variables, 
		const char *property_group, const char *property, const char *default_value)
{
	const char *value;
    char *tmp_str;
    if (!property) return default_value;
    if (property_group == NULL)
        property_group = "";
	tmp_str = malloc(strlen(property_group) + strlen(property) + 2);
	sprintf(tmp_str, "%s_%s", property_group, property);
	value = get_string_value(variables, tmp_str);
	if (value == NULL)
		value = default_value;
	free(tmp_str);
	
	return value;
}

int lookup_int_property(symbol_table variables, const char *property_group, const char *property, int default_value)
{
	int value;
	char *tmp_str = malloc(strlen(property_group) + strlen(property) + 2);
	sprintf(tmp_str, "%s_%s", property_group, property);
	if (verbose())
		printf("looking for property: %s\n", tmp_str);
	value = get_integer_value(variables, tmp_str);
	if (!found_key(variables))
		value = default_value;
	free(tmp_str);
	return value;
}

int lookup_boolean_property(symbol_table variables, 
		const char *property_group, const char *property, int default_value)
{
	const char *value;
	int result = default_value;
	char *tmp_str = malloc(strlen(property_group) + strlen(property) + 2);
	sprintf(tmp_str, "%s_%s", property_group, property);
	if (verbose())
		printf("looking for property: %s\n", tmp_str);
	value = get_string_value(variables, tmp_str);
	if (value)
	{
		if (is_integer(value))
			result = atoi(value) != 0;
		else if (strcmp(value, "YES") == 0 || strcmp(value, "TRUE") == 0)
			result = 1;
		else if (strcmp(value, "NO") == 0 || strcmp(value, "FALSE") == 0)
			result = 0;
	}
	free(tmp_str);
	return result;
}

void set_int_property(symbol_table variables, const char *property_group, const char *property, int value)
{
	char *tmp_str = malloc(strlen(property_group) + strlen(property) + 2);
	sprintf(tmp_str, "%s_%s", property_group, property);
	if (verbose())
		printf("setting  property: %s to %d\n", tmp_str, value);
	set_integer_value(variables, tmp_str, value);
	free(tmp_str);
}

void set_string_property(symbol_table variables, const char *property_group, const char *property, const char *value)
{
	char *tmp_str = malloc(strlen(property_group) + strlen(property) + 2);
	sprintf(tmp_str, "%s_%s", property_group, property);
	if (verbose())
		printf("setting  property: %s to %s\n", tmp_str, value);
	set_string_value(variables, tmp_str, value);
	free(tmp_str);
}

struct iterator_data
{
	const char *group;
	void *user;
	property_func *func;
};

/* here is a way for users to iterate through each property of a group */
void property_handler(const char *name, const char *value, void *user_data)
{
	struct iterator_data *data = (struct iterator_data *) user_data;
	data->func(data->group, name + strlen(data->group)+1, value, data->user);
}

void each_property(symbol_table st, const char *property_group, property_func f, void *user_data)
{
	struct iterator_data data;
	symbol_table properties;
	char *tmp_str = malloc(strlen(property_group) + 3);
	sprintf(tmp_str, "^%s_", property_group);
	properties = collect_matching(st, tmp_str);
	data.group = property_group;
	data.user = user_data;
	data.func = f;
	each_symbol(properties, property_handler, &data);
	free_symbol_table(properties);
	free(tmp_str);
}

void remove_properties(symbol_table st, const char *property_group)
{
	char *tmp_str = malloc(strlen(property_group) + 3);
	sprintf(tmp_str, "^%s_", property_group);
	remove_matching(st, tmp_str);
	remove_symbol(st, property_group);
	free(tmp_str);
}
