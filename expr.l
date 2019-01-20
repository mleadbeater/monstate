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
#include <stdlib.h>
#include "expr.h"
#include "y.tab.h"
#include "buffers.h"
    void yyerror(char *);
	static char *buf;
	static int buflen = 0;
	static char *bufp;

	void append_char(int ch);
%}
%x STRING
%x COMMENT

%%

\n              yylineno++;
"while"         return WHILE;
"if"            return IF;
"else"          return ELSE;
"println"         return PRINTLN;
"print"         return PRINT;

[A-Za-z][A-Za-z0-9_]*       { 
    yylval.symbol = strdup(yytext);
    return VARIABLE;
}

0           {
    yylval.iValue = atoi(yytext);
    return INTEGER;
}

[1-9][0-9]* {
    yylval.iValue = atoi(yytext);
    return INTEGER;
}

"/*"			BEGIN COMMENT;
<COMMENT>"*/"	BEGIN 0;
<COMMENT>\n		yylineno++; 
<COMMENT>.		;

\"			    {  /*"*/	
                    BEGIN STRING; 
					if (!buf) 
					{ 
					     buf = malloc(20); 
					     /*fprintf(stderr, "malloced %lx\n", (long)buf);*/ 
					     buflen = 20; 
					}
					bufp = buf;
					*bufp = 0;
 				}
<STRING>\\n		{ append_char('\n'); }
<STRING>\\r		{ append_char('\r'); }
<STRING>\\t		{ append_char('\t'); }
<STRING>\\\"	{ /*"*/ append_char('\"'); }
<STRING>\"		{ /*"*/ BEGIN 0; yylval.sValue = buf; buf = NULL; return STRINGVAL; }
<STRING>\n		{ append_char('\n'); yylineno++; }
<STRING>.		{ append_char(*yytext); }

[-()<>=+*/;{}.%] {
    return *yytext;
}

">="            return GE;
"<="            return LE;
"=="            return EQ;
"!="            return NE;
"&&"            return AND;
"||"            return OR;

[ \t]+        ;       /* ignore whitespace */

\'              /*' ignore single quotes */
.               { printf("found char %d (%c)\n", *yytext, *yytext); yyerror("Unknown character"); } 
%%
int yywrap(void) {
    return 1;
}

void append_char(int ch)
{
	if (bufp - buf == buflen-1) {
		buf = realloc(buf, buflen + 20);
		bufp = buf + buflen-1;
		buflen += 20;
	}
	*bufp++ = ch;
	*bufp = 0;
}
