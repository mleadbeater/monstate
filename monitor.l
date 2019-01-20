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
%{
#include <stdio.h>
#include "monitor.h"
#include "buffers.h"
#include "monstate.tab.h"
extern YYSTYPE yylval;
static char *buf;
static int buflen = 0;
static char *bufp;
void append_char(int ch);
%}
%x STRING
%x DATA

%%
#.*  			; /*printf("COMMENT\n");*/
[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+ yylval.sVal = strdup(yytext); return IPADDR;
[0-9]+	  				yylval.sVal = strdup(yytext); return NUMBER;
\/[^\/]+\/				yylval.sVal = strdup(yytext + 1); yylval.sVal[strlen(yylval.sVal)-1] = 0; return PATTERN;
ENTER	  				return ENTRY;
STATE	  				return STATE;
POLL	  				return POLL;
EXIT					return TOK_EXIT;
PROPERTY		  		return PROPERTY;
DEFINE					return DEFINE;
EXECUTE					return EXECUTE;
RUN					    return RUN;
SPAWN					return SPAWN;
RESTART					return RESTART;
COLLECT					return COLLECT;
CALL					return CALL;
FROM					return FROM;
TEST					return TEST;
NOT						return NOT;
LOG						return LOG;
SET						return SET;
TRIM					return TRIM;
LINE                    return LINE;
OF                      return OF;
USING                   return USING;
MATCH                   return MATCH;
IN                      return IN;
REPLACE                 return REPLACE;
WITH                    return WITH;
INTERPRET               return INTERPRET;
\+                       return JOINED;
EACH                    return EACH;
DO                      return DO;
FUNCTION                return FUNCTION;
MATCHING                return MATCHING;
SYSTEM\_VERSION          return VERSION;

[\^\$\.a-zA-Z0-9_]+ 		yylval.sVal = strdup(yytext); return WORD;

\"				{  BEGIN STRING; 
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
<STRING>\\\"		{ append_char('"'); }
<STRING>\"		{ BEGIN 0; yylval.sVal = buf; buf = NULL; return WORD; }
<STRING>\n		{ append_char('\n'); yylineno++; }
<STRING>.		{ append_char(*yytext); }

\[              { BEGIN DATA;
					if (!buf) 
					{ 
					     buf = malloc(20); 
					     /*fprintf(stderr, "malloced %lx\n", (long)buf);*/ 
					     buflen = 20; 
					}
					bufp = buf;
					*bufp = 0;
 				}
<DATA>\\n		{ append_char('\n'); }
<DATA>\\r		{ append_char('\r'); }
<DATA>\\t		{ append_char('\t'); }
<DATA>\\\[		{ append_char('['); }
<DATA>\\\]		{ append_char(']'); }
<DATA>\]		{ BEGIN 0; yylval.sVal = buf; buf = NULL; return WORD; }
<DATA>\n		{ append_char('\n'); yylineno++; }
<DATA>\\\"      { append_char('"'); } /*"*/
                  /* at present we ignore this use of backslash for 
                    backward compatibility with scripts 
                     which used to need to quote double quotes.
                 */
                    
<DATA>.		    { append_char(*yytext); }

 /* \"[^\"]*\"			yylval.sVal = strdup(yytext+1); yylval.sVal[strlen(yylval.sVal)-1] = 0; return WORD; */
\\n					return LF;
\;					return SEPARATOR;
\(					return OEXPR;
\)	  				return EEXPR;
\&\&					return AND;
\|\|					return OR;
\n	  				yylineno++;
[ \t]+	  			;
\{	  				return OBRACE;
\}	  				return EBRACE;
  /*\"	  				return QUOTE; */
\'	  				return SQUOTE;
\<=       			return LE;
\<        			return LT;
\>=	  				return GE;
\>	  				return GT;
!=	  				return NE;
\~					return MATCHES;
==					return EQ;
=					return ASSIGNED;
!                   return NOT;
%%

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
