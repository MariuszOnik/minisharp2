/* MiniSharp - AST printer: turns syntax trees back into C# text in one fixed style.
   Users: the graph generator (graph -> AST -> code) and "Format code" in the editor (ms_format).
   Comments are not in the AST, so printing drops them. */
#ifndef MS_PRINTER_H
#define MS_PRINTER_H

#include "ast.h"

typedef struct { char* p; int n, cap; int oom; } MsBuf;    /* growable text; p is always 0-terminated (or NULL) */

void ms_buf_free(MsBuf* b);
void ms_buf_add(MsBuf* b, const char* s, int len);        /* len < 0: strlen */
const char* ms_op_text(int tk);                            /* "+=" for TK_PLUSEQ, "int" for TK_KINT, ... */
void ms_print_type(MsBuf* b, TypeNode* tn);
void ms_print_expr(MsBuf* b, Expr* e);
void ms_print_stmt(MsBuf* b, Stmt* s, int indent);        /* indent = level (4 spaces each); no trailing newline */
void ms_print_program(MsBuf* b, ProgramAST* prog);

/* Parses src into A (lexer + parser only, no compiling); NULL on a syntax error (message in err). */
ProgramAST* ms_parse_text(Arena* A, const char* src, char* err, int errlen);
/* Parses src and prints it again (malloc'd text, the caller frees it). On a syntax error returns NULL
   with the message in err ("line N: ..."). */
char* ms_format(const char* src, char* err, int errlen);

#endif
