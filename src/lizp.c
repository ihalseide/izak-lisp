#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define STRNDUP_IMPL
#include "strndup.h"
#include "lizp.h"

// Allocate value
Val *AllocVal(void)
{
    return malloc(sizeof(Val));
}

// Free value 
void FreeVal(Val *p)
{
    free(p);
}

// Free value recursively
void FreeValRec(Val *v)
{
    if (IsSeq(v))
    {
        // Sequence or NULL
        Val *p = v;
        Val *n;
        while (p && IsSeq(p))
        {
            FreeValRec(p->first);
            p->first = NULL;
            n = p->rest;
            FreeVal(p);
            p = n;
        }
    }
    else
    {
        // Symbol
        if (v->symbol)
        {
            free(v->symbol);
            v->symbol = NULL;
        }
        FreeVal(v);
    }
}

int IsEqual(Val *x, Val *y)
{
    if (x == NULL || y == NULL)
    {
        return x == y;
    }
    if (IsSym(x))
    {
        char *a = x->symbol;
        char *b = y->symbol;
        while (*a && *b && *a == *b)
        {
            a++;
            b++;
        }
        return *a == *b;
    }
    if (IsSeq(x))
    {
        Val *px = x, *py = y;
        while (px && IsSeq(px) && py && IsSeq(py))
        {
            if (!IsEqual(px->first, py->first))
            {
                break;
            }
            px = px->rest;
            py = py->rest;
        }
        return px == NULL && py == NULL;
    }
    return 0;
}

// Check if a value is a sequence
// NULL is also considered an empty sequence
int IsSeq(Val *p)
{
    return !p || (p && !(p->flag & F_SYM));
}

// Check if a value is a symbol
int IsSym(Val *p)
{
    return p && (p->flag & F_SYM);
}

// Make symbol
// - empty string -> null []
Val *MakeSym(char *s)
{
    if (!s || *s == 0)
    {
        return NULL;
    }
    Val *p = AllocVal();
    if (p)
    {
        p->flag = F_SYM;
        p->symbol = s;
    }
    return p;
}

// Make symbol
// - copies buf to take as a name
// - empty string -> null []
Val *MakeSymCopy(const char *buf, int len)
{
    if (!buf || len <= 0)
    {
        return NULL;
    }
    return MakeSym(strndup(buf, len));
}

// Make a symbol for an integer
Val *MakeSymInt(long n)
{
    const char *fmt = "%ld";
    const int sz = snprintf(NULL, 0, fmt, n);
    char buf[sz + 1];
    snprintf(buf, sizeof(buf), fmt, n);
    return MakeSymCopy(buf, sz);
}

// Make sequence
// - first: sym or seq (null included)
// - rest: seq (null included)
Val *MakeSeq(Val *first, Val *rest)
{
    if (rest && !IsSeq(rest))
    {
        return NULL;
    }
    Val *p = AllocVal();
    if (p)
    {
        p->flag = 0;
        p->first = first;
        p->rest = rest;
    }
    return p;
}

// New copy, with no structure-sharing
Val *CopyVal(Val *p)
{
    if (!p)
    {
        return p;
    }
    if (IsSym(p))
    {
        return MakeSymCopy(p->symbol, strlen(p->symbol));
    }
    // Seq
    Val *copy = MakeSeq(CopyVal(p->first), NULL);
    Val *pcopy = copy;
    p = p->rest;
    while (IsSeq(p) && p)
    {
        pcopy->rest = MakeSeq(CopyVal(p->first), NULL);
        pcopy = pcopy->rest;
        p = p->rest;
    }
    return copy;
}

// String needs quotes?
// Check if a string for a symbol name needs to be quoted
// in order to be printed "readably".
int StrNeedsQuotes(const char *s)
{
    while (*s)
    {
        switch (*s)
        {
            case '[':
            case ']':
            case '\n':
            case '\t':
            case '"':
            case '\\':
                return 1;
            default:
                if (isspace(*s))
                {
                    return 1;
                }
        }
        s++;
    }
    return 0;
}

// Escape a string.
// Modifies the string in-place
int EscapeStr(char *str, int len)
{
    if (!str || len <= 0)
    {
        return 0;
    }
    int i = 0;
    int j = 0;
    while (i < len)
    {
        char c = str[i];
        if (c == '\\')
        {
            i++;
            c = str[i];
            switch (c)
            {
                case 'n':
                    c = '\n';
                    break;
                case 't':
                    c = '\t';
                    break;
            }
        }
        str[j] = c;
        i++;
        j++;
    }
    str[j] = '\0';
    return j;
}

// Read symbol from input stream
static int ReadSym(const char *str, int len, Val **out)
{
    int i = 0;
    // leading space
    while (i < len && isspace(str[i]))
    {
        i++;
    }
    switch (str[i])
    {
        case '\0':
            // No symbol
            return 0;
        case '"':
            // Quoted symbol
            {
                i++;
                const int j = i;
                int done = 0, good = 0; 
                while (!done && i < len)
                {
                    switch (str[i])
                    {
                        case '\n':
                        case '\0':
                            done = 1;
                            break;
                        case '\\':
                            i++;
                            if (str[i] == '"')
                            {
                                i++;
                            }
                            break;
                        case '"':
                            done = 1;
                            good = 1;
                            i++;
                            break;
                        default:
                            i++;
                            break;
                    }
                }
                if (good && out)
                {
                    // Make escaped symbol string 
                    int len = i - j - 1;
                    if (len == 0)
                    {
                        *out = NULL;
                        return i;
                    }
                    assert(len > 0);
                    char *str1 = strndup(str + j, len);
                    int len2 = EscapeStr(str1, len);
                    *out = MakeSymCopy(str1, len2);
                    free(str1);
                    return i;
                }
                // invalid
                if (out)
                {
                    *out = NULL;
                }
                return i;
            }
        default:
            // Symbol
            {
                const int j = i;
                int done = 0; 
                while (!done && i < len)
                {
                    switch (str[i])
                    {
                        case '\0':
                        case '"':
                        case '[':
                        case ']':
                            done = 1;
                            break;
                        default:
                            if (isspace(str[i]))
                            {
                                done = 1;
                                break;
                            }
                            i++;
                            break;
                    }
                }
                if (out)
                {
                    *out = MakeSymCopy(str + j, i - j);
                }
                return i;
            }
    }
}

// Read value from input stream
// str = string characters
// len = string length
// out = value to return
// returns the number of chars read
int ReadVal(const char *str, int len, Val **out)
{
    if (!out || !str || !len)
    {
        return 0;
    }

    int i = 0;
    // Space
    while (i < len && isspace(str[i]))
    {
        i++;
    }
    switch (str[i])
    {
        case '\0':
            // end of string
            *out = NULL;
            return i;
        case '[':
            // begin list
            {
                i++;
                // space
                while (i < len && isspace(str[i]))
                {
                    i++;
                }
                // elements
                Val *list = NULL;
                if (str[i] != ']')
                {
                    // first item
                    Val *e;
                    int l = ReadVal(str + i, len - i, &e);
                    if (l <= 0)
                    {
                        *out = NULL;
                        return l;
                    }
                    i += l;
                    // Space
                    while (i < len && isspace(str[i]))
                    {
                        i++;
                    }
                    list = MakeSeq(e, NULL);
                    Val *p = list;
                    // rest of items
                    while (i < len && str[i] != ']')
                    {
                        Val *e;
                        int l = ReadVal(str + i, len - i, &e);
                        i += l;
                        if (l <= 0)
                        {
                            *out = list;
                            return l;
                        }
                        // Space
                        while (i < len && isspace(str[i]))
                        {
                            i++;
                        }
                        p->rest = MakeSeq(e, NULL);
                        p = p->rest;
                    }
                }
                *out = list;
                if (str[i] == ']')
                {
                    i++;
                    // Space
                    while (i < len && isspace(str[i]))
                    {
                        i++;
                    }
                }
                return i;
            }
        case ']':
            // end list
            return i;
        default:
            // Symbol
            {
                Val *sym = NULL;
                int slen = ReadSym(str + i, len - i, &sym);
                i += slen;
                *out = sym;
                // Space
                while (i < len && isspace(str[i]))
                {
                    i++;
                }
                return i;
            }
    }
}

// Prints p to the given `out` buffer.
// Does not do null termination.
// If out is NULL, it just calculates the print length
// Returns: number of chars written
int PrintValBuf(Val *v, char *out, int length, int readable)
{
    // String output count / index
    int i = 0;
    if (IsSeq(v))
    {
        if (out && i < length)
        {
            out[i] = '[';
        }
        i++;
        if (v)
        {
            // first item
            if (out)
            {
                i += PrintValBuf(v->first, out + i, length - i, readable);
            }
            else
            {
                i += PrintValBuf(v->first, NULL, 0, readable);
            }
            v = v->rest;
            while (v)
            {
                // space
                if (out && i < length)
                {
                    out[i] = ' ';
                }
                i++;
                // item
                if (out)
                {
                    i += PrintValBuf(v->first, out + i, length - i, readable);
                }
                else
                {
                    i += PrintValBuf(v->first, NULL, 0, readable);
                }
                v = v->rest;
            }
        }
        if (out && i < length)
        {
            out[i] = ']';
        }
        i++;
    }
    else if (IsSym(v))
    {
        // Symbol
        char *s = v->symbol;
        int quoted = readable && StrNeedsQuotes(s);
        if (quoted)
        {
            // Opening quote
            if (out && i < length)
            {
                out[i] = '"';
            }
            i++;
        }
        // Contents
        while (*s)
        {
            char c = *s;
            if (quoted)
            {
                // escaping
                int esc = 0;
                switch (c)
                {
                    case '\n':
                        c = 'n';
                        esc = 1;
                        break;
                    case '\t':
                        c = 't';
                        esc = 1;
                        break;
                    case '"':
                        c = '"';
                        esc = 1;
                        break;
                    case '\\':
                        c = '\\';
                        esc = 1;
                        break;
                }
                if (esc)
                {
                    if (out && i < length)
                    {
                        out[i] = '\\';
                    }
                    i++;
                }
            }
            if (out && i < length)
            {
                out[i] = c;
            }
            i++;
            s++;
        }
        if (quoted)
        {
            // Closing quote
            if (out && i < length)
            {
                out[i] = '"';
            }
            i++;
        }
    }
    else
    {
        assert(0 && "invalid Val type");
    }
    return i;
}

// Print value to a new string
char *PrintValStr(Val *v, int readable)
{
    int len1 = PrintValBuf(v, NULL, 0, readable);
    if (len1 <= 0)
    {
        return NULL;
    }
    char *new = malloc(len1 + 1);
    if (new)
    {
        int len2 = PrintValBuf(v, new, len1, readable);
        assert(len1 == len2);
        new[len1] = '\0';
    }
    return new;
}

// Print value to a file
void PrintValFile(FILE *f, Val *v)
{
    char *s = PrintValStr(v, 1);
    fprintf(f, "%s", s);
    free(s);
}

// Check whether a value is considered true or "truthy"
int IsTrue(Val *v)
{
    if (!v)
    {
        return 0;
    }
    if (IsSym(v) && !strcmp(v->symbol, "false"))
    {
        return 0;
    }
    return 1;
}

// Set value in environment
// Returns non-zero upon success
int EnvSet(Val *env, Val *key, Val *val)
{
    if (!env || !IsSeq(env))
    {
        return 0;
    }
    Val *pair = MakeSeq(key, MakeSeq(val, NULL));
    // push key-value pair onto the front of the list
    env->first = MakeSeq(pair, env->first);
    return 1;
}

// Get value in environment
Val *EnvGet(Val *env, Val *key)
{
    if (!env)
    {
        return NULL;
    }
    Val *scope = env;
    while (scope && IsSeq(scope))
    {
        Val *p = scope->first;
        while (p && IsSeq(p))
        {
            Val *pair = p->first;
            if (pair && IsSeq(pair) && IsEqual(pair->first, key))
            {
                if (!pair->rest)
                {
                    // env is improperly set up
                    return NULL;
                }
                return pair->rest->first;
            }
            p = p->rest;
        }
        // outer scope
        scope = scope->rest;
    }
    return NULL;
}

// Eval macro
// Return values must not share structure with first, args, or env
static int Macro(Val *first, Val *args, Val *env, Val **out)
{
    char *s = first->symbol;
    if (!strcmp("get", s))
    {
        // [get key] for getting value with a default of null
        if (!args || args->rest)
        {
            *out = NULL;
            return 1;
        }
        Val *key = args->first;
        *out = CopyVal(EnvGet(env, key));
        return 1;
    }
    if (!strcmp("set", s))
    {
        // TODO: remove this once "let" is implemented
        // [set key val]
        if (!args || !args->rest)
        {
            *out = NULL;
            return 1;
        }
        Val *key = args->first;
        if (!IsSym(key))
        {
            *out = NULL;
            return 1;
        }
        Val *val = Eval(args->rest->first, env);
        EnvSet(env, CopyVal(key), CopyVal(val));
        *out = val;
        return 1;
    }
    if (!strcmp("if", s))
    {
        // [if condition consequent alternative]
        if (!args || !args->rest)
        {
            *out = NULL;
            return 1;
        }
        Val *f = Eval(args->first, env);
        int t = IsTrue(f);
        FreeValRec(f);
        if (t)
        {
            *out =  Eval(args->rest->first, env);
            return 1;
        }
        if (args->rest->rest)
        {
            *out = Eval(args->rest->rest->first, env);
            return 1;
        }
        *out = NULL;
        return 1;
    }
    if (!strcmp("quote", s))
    {
        // [quote expr]
        if (!args || args->rest)
        {
            *out = NULL;
            return 1;
        }
        *out = CopyVal(args->first);
        return 1;
    }
    if (!strcmp("do", s))
    {
        // [do (expr)...]
        Val *p = args;
        Val *e = NULL;
        while (p && IsSeq(p))
        {
            e = Eval(p->first, env);
            p = p->rest;
            if (p)
            {
                FreeValRec(e);
            }
        }
        *out = e;
        return 1;
    }
    if (!strcmp("and", s))
    {
        // [and expr1 (expr)...]
        if (!args)
        {
            *out = NULL;
            return 1;
        }
        Val *p = args;
        while (p && IsSeq(p))
        {
            Val *e = Eval(p->first, env);
            if (!IsTrue(e))
            {
                // item is false
                *out = CopyVal(e);
                return 1;
            }
            p = p->rest;
            if (!p)
            {
                // last item is true
                *out = CopyVal(e);
                return 1;
            }
            FreeValRec(e);
        }
    }
    if (!strcmp("or", s))
    {
        // [or expr1 (expr)...]
        if (!args)
        {
            *out = NULL;
            return 1;
        }
        Val *p = args;
        while (p && IsSeq(p))
        {
            Val *e = Eval(p->first, env);
            if (IsTrue(e))
            {
                // item is true
                *out = CopyVal(e);
                return 1;
            }
            p = p->rest;
            if (!p)
            {
                // last item is false
                *out = CopyVal(e);
                return 1;
            }
            FreeValRec(e);
        }
    }
    if (!strcmp("cond", s))
    {
        // [cond (condition result)...] (no nested lists)
        if (!args)
        {
            *out = NULL;
            return 1;
        }
        Val *p = args;
        while (p && IsSeq(p))
        {
            Val *e = Eval(p->first, env);
            if (IsTrue(e))
            {
                FreeValRec(e);
                if (!p->rest)
                {
                    // Uneven amount of items
                    *out = NULL;
                    return 1;
                }
                *out = Eval(p->rest->first, env);
                return 1;
            }
            FreeValRec(e);
            p = p->rest;
            if (!p)
            {
                // Uneven amount of items
                *out = NULL;
                return 1;
            }
            p = p->rest;
        }
        return 1;
    }
    // TODO:
    // [lambda [(symbol)...] (expr)]
    // [let [(key val)...] (expr)] (remember to remove `set` afterwards)

    return 0;
}

// Apply functions
// Return values must not share structure with first, args, or env
// TODO: handle when first is a lambda
Val *Apply(Val *first, Val *args, Val *env)
{
    if (!first || !IsSym(first))
    {
        return NULL;
    }

    char *s = first->symbol;
    if (!strcmp("print", s))
    {
        Val *p = args;
        while (p)
        {
            PrintValFile(stdout, p->first);
            p = p->rest;
        }
        return NULL;
    }
    if(!strcmp("+", s))
    {
        // [+ (e:integer)...] sum
        long sum = 0;
        Val *p = args;
        while (p)
        {
            Val *e = p->first;
            if (!IsSym(e))
            {
                return NULL;
            }
            long x = atol(e->symbol);
            sum += x;
            p = p->rest;
        }
        return MakeSymInt(sum);
    }
    if(!strcmp("*", s))
    {
        // [+ (e:integer)...] product
        long product = 1;
        Val *p = args;
        while (p)
        {
            Val *e = p->first;
            if (!IsSym(e))
            {
                return NULL;
            }
            long x = atol(e->symbol);
            product *= x;
            p = p->rest;
        }
        return MakeSymInt(product);
    }
    if(!strcmp("-", s))
    {
        // [- x:int (y:int)] subtraction
        if (!args)
        {
            return NULL;
        }
        Val *vx = args->first;
        if (!IsSym(vx))
        {
            return NULL;
        }
        long x = atol(vx->symbol);
        if (args->rest)
        {
            Val *vy = args->rest->first;
            if (!IsSym(vy))
            {
                return NULL;
            }
            long y = atol(vy->symbol);
            return MakeSymInt(x - y);
        }
        return MakeSymInt(-x);
    }
    if(!strcmp("/", s))
    {
        // [/ x:int y:int] division
        if (!args || !args->rest)
        {
            return NULL;
        }
        Val *vx = args->first;
        if (!IsSym(vx))
        {
            return NULL;
        }
        long x = atol(vx->symbol);
        Val *vy = args->rest->first;
        if (!IsSym(vy))
        {
            return NULL;
        }
        long y = atol(vy->symbol);
        if (y == 0)
        {
            // division by zero
            return NULL;
        }
        return MakeSymInt(x / y);
    }
    if(!strcmp("%", s))
    {
        // [% x:int y:int] modulo
        if (!args || !args->rest)
        {
            return NULL;
        }
        Val *vx = args->first;
        if (!IsSym(vx))
        {
            return NULL;
        }
        long x = atol(vx->symbol);
        Val *vy = args->rest->first;
        if (!IsSym(vy))
        {
            return NULL;
        }
        long y = atol(vy->symbol);
        if (y == 0)
        {
            // division by zero
            return NULL;
        }
        return MakeSymInt(x % y);
    }
    if (!strcmp("=", s))
    {
        // [= x y (expr)...] check equality
        if (!args || !args->rest)
        {
            return NULL;
        }
        Val *f = args->first;
        Val *p = args->rest;
        while (p && IsSeq(p))
        {
            if (!IsEqual(f, p->first))
            {
                return NULL;
            }
            p = p->rest;
        }
        return MakeSymCopy("true", 4);
    }
    if (!strcmp("not", s))
    {
        // [not expr] boolean not
        if (!args)
        {
            return NULL;
        }
        if (IsTrue(args->first))
        {
            return NULL;
        }
        return MakeSymCopy("true", 4);
    }
    if (!strcmp("symbol?", s))
    {
        // [symbol? val] check if value is a symbol
        if (!args)
        {
            return NULL;
        }
        if (!IsSym(args->first))
        {
            return NULL;
        }
        return MakeSymCopy("true", 4);
    }
    if (!strcmp("list?", s))
    {
        // [list? val] check if value is a list
        if (!args)
        {
            return NULL;
        }
        if (!IsSeq(args->first))
        {
            return NULL;
        }
        return MakeSymCopy("true", 4);
    }
    if (!strcmp("empty?", s))
    {
        // [empty? val] check if value is a the empty list
        if (!args)
        {
            return NULL;
        }
        if (args->first)
        {
            return NULL;
        }
        return MakeSymCopy("true", 4);
    }
    if (!strcmp("nth", s))
    {
        // [nth index list] get the nth item in a list
        if (!args || !args->rest)
        {
            return NULL;
        }
        Val *i = args->first;
        if (!IsSym(i))
        {
            // 1st arg not a symbol
            return NULL;
        }
        Val *list = args->rest->first;
        if (!IsSeq(list))
        {
            // 2nd arg not a list
            return NULL;
        }
        long n = atol(i->symbol);
        if (n < 0)
        {
            // index negative
            return NULL;
        }
        Val *p = list;
        while (n > 0 && p && IsSeq(p))
        {
            p = p->rest;
            n--;
        }
        if (p)
        {
            return CopyVal(p->first);
        }
        // index too big
        return NULL;
    }
    if (!strcmp("list", s))
    {
        // [list (val)...] create list from arguments (variadic)
        return CopyVal(args);
    }
    if (!strcmp("length", s))
    {
        // [length list]
        if (!args)
        {
            return NULL;
        }
        if (!IsSeq(args->first))
        {
            return NULL;
        }
        long len = 0;
        Val *p = args->first;
        while (p && IsSeq(p))
        {
            len++;
            p = p->rest;
        }
        return MakeSymInt(len);
    }
    // TODO:
    // [defined? v]
    // [lambda? v]
    // [nil? v]
    // [< v1 v2 (v)...]
    // [<= v1 v2 (v)...]
    // [> v1 v2 (v)...]
    // [>= v1 v2 (v)...]
    // [chars sym] -> list
    // [symbol list] -> symbol
    // [reverse list]
    // [member item list] -> bool
    // [count item list] -> int
    // [concat list (list)...]
    // [append list val]
    // [prepend val list]
    // [join separator (list)...] -> list
    // [position item list] -> list
    // [without item list] -> list
    // [replace item1 item2 list] -> list
    // [replaceI index item list] -> list
    // [slice list start (end)]
    // [zip list (list)...]

    return NULL;
}

// Evaluate a Val
// - ast = Abstract Syntax Tree to evaluate
// - env = environment of symbol-value pairs
// Returns evaluated value
// - must only return new values that do not share structure with ast or env
Val *Eval(Val *ast, Val *env)
{
    if (!ast)
    {
        // empty list
        return ast;
    }
    if (IsSym(ast))
    {
        // lookup symbol value
        Val *val = EnvGet(env, ast);
        if (val)
        {
            return val;
        }
        // a symbol evaluates to itself if not found
        return CopyVal(ast);
    }
    assert(ast);
    assert(IsSeq(ast));
    // eval first element
    Val *first = Eval(ast->first, env);
    // macro?
    if (IsSym(first))
    {
        Val *result;
        if (Macro(first, ast->rest, env, &result))
        {
            return result;
        }
    }
    // not a macro
    // eval rest of elements for apply
    Val *list = MakeSeq(first, NULL);
    Val *p_list = list;
    Val *p_ast = ast->rest;
    while (p_ast && IsSeq(p_ast))
    {
        p_list->rest = MakeSeq(Eval(p_ast->first, env), NULL);
        p_list = p_list->rest;
        p_ast = p_ast->rest;
    }
    Val *result = Apply(first, list->rest, env);
    FreeValRec(list);
    return result;
}
