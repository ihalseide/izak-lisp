#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lizp.h"

// TODO: add BeginRead, Read1, and EndRead?
// Global, no nested reading.
//   int BeginRead(const char *str, int len);
//   int Read1(Val **out);
//   int EndRead(void);
// State:
//   int str, len;
//   int index, status;
//   Val *result;
//   Val *pstack;

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
            n = p->rest;
            free(p);
            p = n;
        }
    }
    else
    {
        // Symbol
        if (v->symbol)
        {
            free(v->symbol);
        }
        free(v);
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

// NULL is considered an empty sequence
int IsSeq(Val *p)
{
    return !p || (p && !(p->flag & F_SYM));
}

int IsSym(Val *p)
{
    return p && (p->flag & F_SYM);
}

// Make symbol (copies buf to take as a name)
Val *MakeSym(const char *buf, int len)
{
    if (!buf || len <= 0)
    {
        return NULL;
    }
    Val *p = malloc(sizeof(*p));
    p->flag = F_SYM;
    p->symbol = malloc((len + 1) * sizeof(*buf));
    memcpy(p->symbol, buf, len);
    p->symbol[len] = 0;
    return p;
}

Val *MakeSeq(Val *first, Val *rest)
{
    if (rest && !IsSeq(rest))
    {
        // only allow seq's in the rest slot
        return NULL;
    }
    Val *p = malloc(sizeof(*p));
    p->flag = 0;
    p->first = first;
    p->rest = rest;
    return p;
}

// New copy, with no structure-sharing
Val *CopyVal(Val *p)
{
    if (p)
    {
        if (IsSym(p))
        {
            return MakeSym(p->symbol, strlen(p->symbol));
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
    return NULL;
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
// In-place string escaping
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
                        case '\0':
                            done = 1;
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
                if (good)
                {
                    if (out)
                    {
                        *out = MakeSym(str + j, i - j - 1);
                    }
                    return i;
                }
                else
                {
                    assert(0 && "error");
                }
                break;
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
                    *out = MakeSym(str + j, i - j);
                }
                return i;
            }
    }
}

// str = string characters
// len = string length
// out = value to return
// returns number of chars read
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
                Val *p;
                while (i < len && str[i] != ']')
                {
                    Val *e;
                    int l = ReadVal(str + i, len - i, &e);
                    if (!l)
                    {
                        break;
                    }
                    if (p)
                    {
                        p->rest = MakeSeq(e, NULL);
                        p = p->rest;
                    }
                    else
                    {
                        p = list = MakeSeq(e, NULL);
                    }
                    i += l;
                }
                *out = list;
                return i;
            }
        case ']':
            // end list
            *out = NULL;
            return i;
        default:
            // Symbol
            {
                Val *sym = NULL;
                int slen = ReadSym(str + i, len - i, &sym);
                EscapeStr(sym->symbol, len - i);
                i += slen;
                *out = sym;
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
    Val mark1, mark2, mark3;
    // String output count / index
    int i = 0;
    // Value stack
    Val *stack = MakeSeq(v, NULL);
    Val *prev = NULL;
    while (stack)
    {
        Val *e = stack->first;
        Val *old = stack;
        stack = stack->rest;
        free(old);
        if (!e)
        {
            // NULL, empty Sequence
            if (out && i < length)
            {
                // "[]"
                out[i] = '[';
                out[i + 1] = ']';
            }
            i += 2;
        }
        else if (e == &mark1)
        {
            // List end marker
            if (out && i < length)
            {
                out[i] = ']';
            }
            i++;
        }
        else if (e == &mark2)
        {
            // List separator marker
            if (out && i < length)
            {
                out[i] = ' ';
            }
            i += 1;
        }
        else if (e == &mark3)
        {
            // List begin marker
            if (out && i < length)
            {
                out[i] = '[';
            }
            i++;
        }
        else if (IsSym(e))
        {
            // Symbol
            char *s = e->symbol;
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
            prev = e;
        }
        else
        {
            // Sequence -> split into smaller units
            int wrap = !prev || (prev->first == e);
            if (wrap)
            {
                stack = MakeSeq(&mark1, stack); 
            }
            if (e->rest) 
            {
                stack = MakeSeq(e->rest, stack);
                stack = MakeSeq(&mark2, stack);
            }
            stack = MakeSeq(e->first, stack);
            if (wrap)
            {
                stack = MakeSeq(&mark3, stack);
            }
        }
        prev = e;
    }

    return i;
}

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
        (void)len2;
        new[len1] = '\0';
    }
    return new;
}

