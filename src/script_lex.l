%{
    #include <stdbool.h>

    #include "defs.h"
    #include "script_gram.tab.h"

    void script_error();
    static bool comment(void* llocp, void* ctx, yyscan_t scanner);

    #undef YY_DECL
    #define YY_DECL \
        int yylex(YYSTYPE* yylval_param, YYLTYPE* yylloc_param, UNUSED void* ctx, \
            UNUSED yyscan_t yyscanner)
    YY_DECL;
%}

%option 8bit noyywrap nounput batch yylineno
%option reentrant bison-bridge warn bison-locations
%option prefix="script_"
%option header-file="src/script_lex.yy.h"
%option extra-type="void *"

blank [ \t]
dquote \"
chars (\\.|[^\\"])*|(¥.|[^¥"])*

uint 0[xX][[:xdigit:]]+|[[:digit:]]+
%%

[():.,;\"] {return yytext[0];}

"/*" {if (!comment(yylloc, yyextra, yyscanner)) return yytext[0];}
"//".* { };

[ \t\n]+ { }
"byte" return SCRIPT_BYTE;
"begin" return SCRIPT_BEGIN;
"end" return SCRIPT_END;

{uint} {yylval->uval = strtoumax(yytext, NULL, 0); return SCRIPT_NUM;}
{dquote}{chars}{dquote} {
    assert(yyleng >= 2);

    /* Drop the quotes */
    char* s = strdup(&yytext[1]);

    s[yyleng - 2] = '\0';
    yylval->sval = s;
    return SCRIPT_STR;
}

[a-zA-Z_][a-zA-Z_0-9]* {
    yylval->sval = strdup(yytext);
    return SCRIPT_ID;
}

. /* script_error(yylloc, yyextra, scanner, "Unrecognised character"); */

%%

static bool comment(void* llocp, void* ctx, yyscan_t scanner) {
    int c;

    while ((c = input(scanner)))
        if (c == '*') {
            while ((c = input(scanner)) == '*')
                ;

            if (c == '/')
                return true;

            if (c == 0)
                break;
        }
    script_error(llocp, ctx, scanner, "Unterminated comment");
    return false;
}
