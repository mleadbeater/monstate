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
#ifndef __PROPERTY_H__
#define __PROPERTY_H__

/* properties are simply symbols with names of the form:

      groupname '_' propertyname
*/

symbol_table collect_properties(symbol_table symbols, const char *property_group);

const char *lookup_string_property(symbol_table symbols, 
	const char *property_group, const char *property, const char *default_value);
	
int lookup_int_property(symbol_table symbols, const char *property_group, 
    const char *property, int default_value);

/* lookup_boolean_property returns either 0 or 1, depending on whether the 
     property has a value of YES, TRUE or a nonzero value */
int lookup_boolean_property(symbol_table symbols, const char *property_group, 
    const char *property, int default_value);
	
void set_int_property(symbol_table symbols, const char *property_group, 
    const char *property, int value);
	
void set_string_property(symbol_table symbols, const char *property_group, 
    const char *property, const char *value);

/* here is a way for users to iterate through each property of a group */
typedef void (property_func)(const char *property_group, const char *name, const char *value, void *user_data);

void each_property(symbol_table st, const char *property_group, property_func f, void *user_data);

void remove_properties(symbol_table st, const char *property_group);

#endif
