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
/*
 *  calc.c
 *  ExpressionParser
 *
 *  Created by Martin Leadbeater on 30/12/09.
 *  Copyright 2009 Martin Leadbeater. All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "expr.h"
#include "y.tab.h"
#include "symboltable.h"
#include "buffers.h"
#include "options.h"
#include "regular_expressions.h"

#include "plugin.h"
#include "property.h"

extern FILE *yyin;
symbol_table variables;
char *output;

nodeType *duplicate(nodeType *p)
{
	nodeType *result;
    if (!p) return 0;
	result = malloc(p->size);
	memmove(result, p, p->size);
    switch(p->type) {
        case typeCon:       break;
        case typeStr:       result->node.str.value = strdup(p->node.str.value);
							/*(stderr, "strduped %lx\n", (long)result->node.str.value);*/
							break;
        case typeId:        result->node.sym.name = strdup(p->node.sym.name);
							/*fprintf(stderr, "strduped %lx\n", (long)result->node.sym.name);*/
							break;
		case typeOpr:		break;
	}
	return result;
}

/* iVal will extract the integer value from the node */
int iVal(nodeType *v)
{
	int result = 0;
    switch(v->type) {
        case typeCon:       result = v->node.con.value;
							break;
        case typeStr:       result = atoi(v->node.str.value);
							break;
        case typeId:        if ( strcmp(v->node.sym.name, "random") == 0)
                                result = random();
                            else
                                result = get_integer_value(variables, v->node.sym.name);
							break;
		case typeOpr:
							fprintf(stderr, "an operator cannot be used where a value is required\n"); 
							break;
	}
	freeNode(v);
	return result;
}

nodeType * ex(nodeType *p) {
	nodeType *a;
	nodeType *b;
    if (!p) return 0;
    switch(p->type) {
        case typeCon:       return duplicate(p);
        case typeStr:       return duplicate(p);
        case typeId:        
            {
                const char *val;
                if ( strcmp(p->node.sym.name, "random") == 0)
                    return constant(random());
                val = get_string_value(variables, p->node.sym.name);
                if (!val)
                    val = "";
                if (is_integer(val))
                    a = constant(get_integer_value(variables, p->node.sym.name));
                else
                    a = string_constant(strdup(val));
                return a;
            }
        case typeOpr:
        {
            int op = p->node.opr.oper;
            if (op == WHILE) {
                while(iVal(ex(p->node.opr.op[0]))) freeNode(ex(p->node.opr.op[1])); return 0;
            }
            else if (op == IF) {
 					if (iVal(ex(p->node.opr.op[0])))
                        freeNode(ex(p->node.opr.op[1]));
                    else if (p->node.opr.nops > 2)
                        freeNode(ex(p->node.opr.op[2]));
                    return 0;
            }
            else if (op == PRINT || op == PRINTLN) {
                    char *out = NULL;
                    char *eol = "";
                    if (p->node.opr.oper == PRINTLN) eol = "\n";
                    a = ex(p->node.opr.op[0]);
                    if (a->type == typeCon) 
                    {	
                        asprintf(&out, "%d%s", a->node.con.value, eol);
                        freeNode(a);
                    } 
                    else if (a->type == typeStr)
                    {	
                        asprintf(&out, "%s%s", a->node.str.value, eol);
                        freeNode(a);
                    } 
                    else
                        asprintf(&out, "%d%s", iVal(a), eol);
                    output = append_buffer(output, out); 
                    return 0;
            }
            else if (op == ';') {
                    freeNode(ex(p->node.opr.op[0])); 
                    return ex(p->node.opr.op[1]);
            }
            else if (op == '=') {
                    a = ex(p->node.opr.op[1]);
                    /* assigning to the special variable 'random' seeds the generator */
                    if ( strcmp(p->node.opr.op[0]->node.sym.name, "random") == 0 )
                    {
                        if (a->type == typeCon) srandom(a->node.con.value);
                            return a;
                    }
                    if (a->type == typeCon)
                        set_integer_value(variables, p->node.opr.op[0]->node.sym.name, a->node.con.value); 
                    else
                        set_string_value(variables, p->node.opr.op[0]->node.sym.name, a->node.str.value); 
                    return a; 
            }
            else if (op == UMINUS) {
                        a = constant(iVal(ex(p->node.opr.op[0])));
                        a->node.con.value = -a->node.con.value;
                        return a; 
            }
            else if (op == OR) {
                return constant(iVal(ex(p->node.opr.op[0])) || iVal(ex(p->node.opr.op[1])));
            }
            else if (op == AND) {
                return constant(iVal(ex(p->node.opr.op[0])) && iVal(ex(p->node.opr.op[1])));
            }
            else {
                a = ex(p->node.opr.op[0]);
                b = ex(p->node.opr.op[1]);
                if (a->type == typeCon)
                    switch(p->node.opr.oper) {
                        case '+':       return constant(iVal(a) + iVal(b));
                        case '-':       return constant(iVal(a) - iVal(b));
                        case '*':       return constant(iVal(a) * iVal(b));
                        case '/':       return constant(iVal(a) / iVal(b));
                        case '%':       return constant(iVal(a) % iVal(b));
                        case '<':       return constant(iVal(a) < iVal(b));
                        case '>':       return constant(iVal(a) > iVal(b));
                        case GE:        return constant(iVal(a) >= iVal(b));
                        case LE:        return constant(iVal(a) <= iVal(b));
                        case NE:        return constant(iVal(a) != iVal(b));
                        case EQ:        return constant(iVal(a) == iVal(b));
                    }
                else if (a->type == typeStr) {
                    nodeType *result = 0;
                    char *bStr = 0;
                    if (b->type == typeStr)
                        bStr = strdup(b->node.str.value);
                    else if (b->type == typeCon)
                        asprintf(&bStr, "%d", b->node.con.value);
                    else
                        fprintf(stderr, "Unexpected node type in expression (%d)\n", b->type);

                    if (op == '+')
                    {
                        size_t bstr_len = (bStr) ? strlen(bStr) : 0;
                        char *s = malloc(strlen(a->node.str.value) + bstr_len + 1);
                        strcpy(s, a->node.str.value);
                        if (bStr) strcat(s, bStr);
                        result = string_constant(s);
                    }
                    else {
                        int cmp = strcmp(a->node.str.value, bStr);
                        if (op == '<') result = constant(cmp < 0);
                        else if (op == '>') result = constant(cmp > 0);
                        else if (op == GE) result = constant(cmp >= 0);
                        else if (op == LE) result = constant(cmp <= 0);
                        else if (op == NE) result = constant(cmp != 0);
                        else if (op == EQ) result = constant(cmp == 0);
                    }
                    freeNode(a);
                    freeNode(b);
                    free(bStr);
                    return result;
                }
                freeNode(a);
                freeNode(b);
                return 0;
            }
        }
    }
    return 0;
}

static char *random_string(int length)
{
    char *valid = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    int max_i = strlen(valid);
    char *result = malloc(length+1);
    char *p = result;
    int i;
    
    for (i=0; i<length; i++)
        *p++ = valid[random()%max_i];
    *p = 0;
    return result;
}

#ifdef BUILD_PLUGIN
EXPORT
#else
static
#endif
char *plugin_func(symbol_table symbols, char *user_buf, int user_buflen, int argc, const char *argv[])
{
    int arg = 1;
	int direct_execution = 1;
	variables = symbols;
	output = user_buf;
	if (!output)
		output = strdup("");
	if (verbose())
	{
		fprintf(stderr,"expr plugin: ");
	    while ( argc > arg )
		{
			fprintf(stderr, "%s ", argv[arg]);
			arg++;
		}
		fprintf(stderr,"\n");
		arg = 1;
	}
    while ( argc > arg )
    {
		if (strcmp(argv[arg], "-e") == 0)
			direct_execution = 1;
		if (strcmp(argv[arg], "-f") == 0)
			direct_execution = 0;
		else {
            char *template = "/tmp/expr_tmp%s.dat";
            int random_len = 6;
            char *tempfile = malloc(strlen(template) + random_len + 1);
            { 
              char *s = random_string(random_len);
              sprintf(tempfile, template, s);
              free(s);
            }
			/*fprintf(stderr, "expression: %s\n", argv[arg]);*/
			yyin = NULL;
			if (!direct_execution)
	        	yyin = fopen(argv[arg], "r");
			else
			{
				FILE *x = fopen(tempfile, "w");
				if (x) {
					fprintf(x, "%s", argv[arg]);
					fclose(x);
					yyin = fopen( tempfile, "r");
				}
				else
                {
					perror("expr_plugin file:");
                }
			}
	        if (yyin)
			{
				process_expression();
	        	fclose(yyin);
                unlink(tempfile);
			}
            free(tempfile);
		}
        arg++;
    }
	return output;
}

#ifdef TESTING_PLUGIN
int main (int argc, const char * argv[]) {
	char *buf = NULL;
    variables = init_symbol_table();
	buf = plugin_func(variables, buf, 0, argc, argv);
	if (buf) printf("result: %s\n", buf);
    printf("symbols:\n");
    dump_symbol_table(variables);
    free_symbol_table(variables);
    fflush(stdout);
    return 0;
}
#endif


