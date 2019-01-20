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
 *  expr.y
 *  ExpressionParser
 *
 *  Created by Martin Leadbeater on 30/12/09.
 *
 * based on code in http://epaperpress.com/lexandyacc/download/lexyacc.pdf
 *
 */

%{
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
int yyparse();
#include "expr.h"
    
    /* prototypes */
    nodeType *opr(int oper, int nops, ...);
    nodeType *id(const char *s);
    nodeType *constant(int value);
	nodeType *string_constant(const char *value);    
    void freeNode(nodeType *p);
    nodeType *ex(nodeType *p);
    int yylex(void);
    void yyerror(char *s);
    /*int sym[26];                     symbol table */
    int yylineno;
%}

%union {
    int iValue;                 /* integer value */
    char sIndex;                /* symbol table index */
    const char *symbol;         /* symbol name */
    const char *sValue;         /* symbol name */
    nodeType *nPtr;             /* node pointer */
};

%token <iValue> INTEGER
%token <sValue> STRINGVAL
/*%token <sIndex> VARIABLE*/
%token <symbol> VARIABLE
%token WHILE IF PRINTLN PRINT
%nonassoc IFX
%nonassoc ELSE

%left OR
%left AND
%left GE LE EQ NE '>' '<'
%left '+' '-'
%left '*' '/' '%'
%nonassoc UMINUS

%type <nPtr> stmt expr stmt_list

%%

program:
function                { /*exit(0);*/ }
;

function:
function stmt         { freeNode(ex($2)); freeNode($2); }
| /* NULL */
;

stmt:
';'                            { $$ = opr(';', 2, NULL, NULL); }
| expr ';'                       { $$ = $1; }
| PRINTLN expr ';'                 { $$ = opr(PRINTLN, 1, $2); }
| PRINT expr ';'                 { $$ = opr(PRINT, 1, $2); }
| VARIABLE '=' expr ';'          { $$ = opr('=', 2, id($1), $3); }
| WHILE '(' expr ')' stmt        { $$ = opr(WHILE, 2, $3, $5); }
| IF '(' expr ')' stmt %prec IFX { $$ = opr(IF, 2, $3, $5); }
| IF '(' expr ')' stmt ELSE stmt { $$ = opr(IF, 3, $3, $5, $7); }
| '{' stmt_list '}'              { $$ = $2; }
;

stmt_list:
stmt                    { $$ = $1; }
| stmt_list stmt        { $$ = opr(';', 2, $1, $2); }
;

expr:
INTEGER                 { $$ = constant($1); }
| STRINGVAL             { $$ = string_constant($1); }
| VARIABLE              { $$ = id($1); }
| '-' expr %prec UMINUS { $$ = opr(UMINUS, 1, $2); }
| expr '+' expr         { $$ = opr('+', 2, $1, $3); }
| expr '-' expr         { $$ = opr('-', 2, $1, $3); }
| expr OR expr          { $$ = opr(OR, 2, $1, $3); }
| expr '*' expr         { $$ = opr('*', 2, $1, $3); }
| expr '/' expr         { $$ = opr('/', 2, $1, $3); }
| expr '%' expr         { $$ = opr('%', 2, $1, $3); }
| expr AND expr         { $$ = opr(AND, 2, $1, $3); }
| expr '<' expr         { $$ = opr('<', 2, $1, $3); }
| expr '>' expr         { $$ = opr('>', 2, $1, $3); }
| expr GE expr          { $$ = opr(GE, 2, $1, $3); }
| expr LE expr          { $$ = opr(LE, 2, $1, $3); }
| expr NE expr          { $$ = opr(NE, 2, $1, $3); }
| expr EQ expr          { $$ = opr(EQ, 2, $1, $3); }
| '(' expr ')'          { $$ = $2; }
;

%%

#define SIZEOF_NODETYPE ((char *)&p->node.con - (char *)p)

nodeType *constant(int value) {
    nodeType *p = 0;
    size_t nodeSize;
    
    /* allocate node */
    nodeSize = SIZEOF_NODETYPE + sizeof(conNodeType);
    if ((p = malloc(nodeSize)) == NULL)
        yyerror("out of memory");
    
    /* copy information */
	p->size = nodeSize;
    p->type = typeCon;
    p->node.con.value = value;
    
    return p;
}

nodeType *string_constant(const char *value) {
    nodeType *p = 0;
    size_t nodeSize;
    
    /* allocate node */
    nodeSize = SIZEOF_NODETYPE + sizeof(strNodeType);
    if ((p = malloc(nodeSize)) == NULL)
        yyerror("out of memory");
    
    /* copy information */
	p->size = nodeSize;
    p->type = typeStr;
    p->node.str.value = value;
    
    return p;
}

nodeType *id(const char *s) {
    nodeType *p = 0;
    size_t nodeSize;
    
    /* allocate node */
    nodeSize = SIZEOF_NODETYPE + sizeof(symNodeType);
    if ((p = malloc(nodeSize)) == NULL)
        yyerror("out of memory");
        
    /* copy information */
	p->size = nodeSize;
    p->type = typeId;
    p->node.sym.name = s;
    
    return p;
}

nodeType *opr(int oper, int nops, ...) {
    va_list ap;
    nodeType *p = 0;
    size_t nodeSize;
    int i;
    
    /* allocate node */
    nodeSize = SIZEOF_NODETYPE + sizeof(oprNodeType) +
    (nops - 1) * sizeof(nodeType*);
    if ((p = malloc(nodeSize)) == NULL)
        yyerror("out of memory");

    /* copy information */
	p->size = nodeSize;
    p->type = typeOpr;
    p->node.opr.oper = oper;
    p->node.opr.nops = nops;
    va_start(ap, nops);
    for (i = 0; i < nops; i++)
        p->node.opr.op[i] = va_arg(ap, nodeType*);
    va_end(ap);
    return p;
}

void freeNode(nodeType *p) {
    int i;
	/*if (p && p->type == typeStr) fprintf(stderr, "free %lx (%ld)\n", (long)p, (long)p->node.str.value);*/
    
    if (!p) return;
    if (p->type == typeOpr) {
        for (i = 0; i < p->node.opr.nops; i++)
            freeNode(p->node.opr.op[i]);
    }
    if (p->type == typeId) free( (char *)p->node.sym.name);
    if (p->type == typeStr) free( (char *)p->node.str.value);
    free (p);
}

void yyerror(char *s) {
    fprintf(stderr, "error %s on line %d\n", s, yylineno);
}

int process_expression() {
	return yyparse();
}
