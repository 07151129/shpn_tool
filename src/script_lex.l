%{
    #include "script_gram.tab.h"

    void script_error();
    static void comment(void* llocp, void* ctx, yyscan_t scanner);

    #undef YY_DECL
    #define YY_DECL \
        int yylex(YYSTYPE* yylval_param, YYLTYPE* yylloc_param, void* ctx, yyscan_t yyscanner)
    YY_DECL;
%}

%option 8bit noyywrap nounput batch yylineno
%option reentrant bison-bridge warn bison-locations
%option prefix="script_"
%option header-file="src/script_lex.yy.h"
%option extra-type="void *"

blank [ \t]
dquote "\""
unesc_char [ -!#-\[\]-~]
esc_char \\[n\\]
hexd [0-9a-f]
hex_char \\x{hexd}{hexd}
char {unesc_char}|{esc_char}|{hex_char}
chars {char}+

uint [[:digit:]]+|0[xX][[:xdigit::]]+
%%

[():.,;] {return yytext[0];}

"/*" {comment(yylloc, yyextra, yyscanner);}
"//".* { }

[ \t\n]+ { }
"byte" return SCRIPT_BYTE;


{uint} {yylval->uval = strtoumax(yytext, NULL, 0); return SCRIPT_NUM;}
{dquote}{dquote}|{dquote}{chars}{dquote} {
    assert(yyleng > 2);

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

static void comment(void* llocp, void* ctx, yyscan_t scanner) {
    int c;

    while ((c = input(scanner)))
        if (c == '*') {
            while ((c = input(scanner)) == '*')
                ;

            if (c == '/')
                return;

            if (c == 0)
                break;
        }
    script_error(llocp, ctx, "Unterminated comment");
}
