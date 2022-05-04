#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lizp.h"

// Callback
extern Val *LizpInitEnv(void);

static long pool_size = 0;
static Val *pool = NULL;
static Val *freelist = NULL;

static long opcode;
static Val *global_env;
static Val *env;
static Val *code;
static Val *next;
static Val *args;

Val *Length(Val *args)
{
    if (args)
    {
        Val *p = args->first;
        long i = 0;
        while (p && IsSeq(p))
        {
            i++;
            p = p->rest;
        }
        return MakeInt(i);
    }
    return NULL;
}

// Concatenate lists into a single list
Val *ConcatLists(Val *lists)
{
    Val *cat = NULL;
    Val *p;
    while (lists && IsSeq(lists))
    {
        Val *s = lists->first;
        if (!IsSeq(s))
        {
            return NULL;
        }
        while (s && IsSeq(s))
        {
            if (cat)
            {
                p->rest = MakeSeq(s->first, NULL);
                p = p->rest;
                s = s->rest;
                continue;
            }
            cat = MakeSeq(s->first, NULL);
            p = cat;
            s = s->rest;
        }
        lists = lists->rest;
    }
    return cat;
}

// Join a list of strings with a separator string
// sep: separator string
// strs: list of strings
Val *JoinStrings(Val *sep, Val *strs)
{
    if (strs)
    {
        Val *result = MakeEmptyStr();
        Val *p = result;
        // First string
        Val *s;
        s = strs->first->rest;
        while (s && IsSeq(s))
        {
            assert(IsInt(s->first));
            p->rest = MakeSeq(s->first, NULL);
            p = p->rest;
            s = s->rest;
        }
        strs = strs->rest;
        // Rest of the strings
        while (strs && IsSeq(strs))
        {
            // Sep
            s = sep->rest;
            while (s && IsSeq(s))
            {
                assert(IsInt(s->first));
                p->rest = MakeSeq(s->first, NULL);
                p = p->rest;
                s = s->rest;
            }
            // String
            s = strs->first->rest;
            while (s && IsSeq(s))
            {
                assert(IsInt(s->first));
                p->rest = MakeSeq(s->first, NULL);
                p = p->rest;
                s = s->rest;
            }
            strs = strs->rest;
        }
        return result;
    }
    return MakeEmptyStr();
}

// Base function for lizp code calls
void DoPrint(Val *args, bool readable)
{
    Val *p = args;
    while (p && IsSeq(p))
    {
        print(p->first, readable);
        p = p->rest;
    }
}

// Sum up a list of integers
// Returns NULL if error
Val *Sum(Val *ints)
{
    Val *p = ints;
    long sum = 0;
    while (p && IsSeq(p))
    {
        Val *e = p->first;
        if (!IsInt(e))
        {
            return NULL;
        }
        sum += e->integer;
        p = p->rest;
    }
    return MakeInt(sum);
}

// Product of a list of integers
// Returns NULL if error
Val *Product(Val *ints)
{
    Val *p = ints;
    long product = 1;
    while (p && IsSeq(p))
    {
        Val *e = p->first;
        if (!IsInt(e))
        {
            return NULL;
        }
        product *= e->integer;
        p = p->rest;
    }
    return MakeInt(product);
}


void dprint_pool(void)
{
    for (int i = 0; i < pool_size; i++)
    {
        Val *p = &pool[i];
        printf("%c%d%c: ",
                (freelist==p)? '^' : ' ',
                i,
                p->is_mark? '*' : ' ');
        if (IsInt(p))
        {
            print(p, 1);
        }
        else
        {
            putchar('(');
            if (p->first)
            {
                printf("%ld", p->first - pool);
            }
            else
            {
                putchar('X');
            }
            printf(" . ");
            if (p->rest)
            {
                printf("%ld", p->rest - pool);
            }
            else
            {
                putchar('X');
            }
            putchar(')');
        }
        putchar('\n');
    }
    putchar('\n');
}

Val *GetVal(Val *save1, Val *save2)
{
    if (!freelist)
    {
        CollectGarbage(save1, save2);
        if (!freelist)
        {
            assert(0 && "not implemented yet");
        }
    }
    Val *p = freelist;
    freelist = freelist->rest;
    return p;
}

void ValFree(Val *p)
{
    if (p)
    {
        p->rest = freelist;
        freelist = p;
    }
}

void ValFreeRec(Val *v)
{
    Val *p = v;
    while (p && IsSeq(p))
    {
        ValFreeRec(p->first);
        p = p->rest;
    }
    ValFree(v);
}

void Mark(Val *v)
{
    if (v && !v->is_mark)
    {
        v->is_mark = 1;
        // Mark sub-sequences
        Val *p = v;
        while (p && IsSeq(p))
        {
            Mark(p->first);
            p = p->rest;
        }
    }
}

void CollectGarbage(Val *save1, Val *save2)
{
    Mark(save1);
    Mark(save2);
    Mark(global_env);
    Mark(env);
    Mark(code);
    Mark(next);
    Mark(args);
    for (int i = 0; i < pool_size; i++)
    {
        dprint_pool();
        Val *p = &pool[i];
        if (!p->is_mark)
        {
            ValFree(p);

            // TODO: for debugging only, remove!
            p->first = NULL;
        }
        // Un-mark
        p->is_mark = 0;
    }
}

// A NULL val is considered an empty sequence,
bool IsSeq(Val *p)
{
    return !p || p->is_seq;
}

bool IsInt(Val *p)
{
    return p && p->is_int;
}

bool IsFunc(Val *p)
{
    return p && p->is_func;
}

Val *MakeInt(long n)
{
    Val *p = GetVal(NULL, NULL);
    p->integer = n;
    p->is_int = 1;
    p->is_seq = 0;
    p->is_func = 0;
    assert(IsInt(p));
    return p;
}

Val *MakeSeq(Val *first, Val *rest)
{
    Val *p = GetVal(first, rest);
    p->first = first;
    p->rest = rest;
    p->is_int = 0;
    p->is_seq = 1;
    p->is_func = 0;
    assert(IsSeq(p));
    return p;
}

Val *MakeFunc(Val *func(Val *))
{
    Val *p = GetVal(NULL, NULL);
    p->func = func;
    p->is_int = 0;
    p->is_seq = 0;
    p->is_func = 1;
    assert(IsFunc(p));
    return p;
}

Val *MakeEmptyStr(void)
{
    return MakeSeq(MakeSeq(MakeInt(STR), NULL), NULL);
}

// String is a special type of Seq
Val *MakeStr(const char *s, int len)
{
    Val *p = MakeEmptyStr();
    Val *ps = p;
    for (int i = 0; i < len; i++)
    {
        ps->rest = MakeSeq(MakeInt(s[i]), NULL);
        ps = ps->rest;
    }
    return p;
}

// New copy, with no structure-sharing
Val *ValCopy(Val *p)
{
    if (p)
    {
        if (IsInt(p))
        {
            return MakeInt(p->integer);
        }
        // Seq
        Val *copy = MakeSeq(ValCopy(p->first), NULL);
        Val *pcopy = copy;
        p = p->rest;
        while (IsSeq(p) && p)
        {
            pcopy->rest = MakeSeq(ValCopy(p->first), NULL);
            pcopy = pcopy->rest;
            p = p->rest;
        }
        return copy;
    }
    return NULL;
}

bool IsEqual(Val *x, Val *y)
{
    if (x == NULL || y == NULL)
    {
        return x == y;
    }
    if (IsInt(x))
    {
        return IsInt(y) && x->integer == y->integer;
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
    return false;
}

bool ValIsTrue(Val *p)
{
    return IsInt(p) && p->integer;
}

// String is a special type of Seq
// form [[str] ...]
bool IsStr(Val *p)
{
    return p && p->rest != p && p->first && IsSeq(p->first) &&
        p->first->first && IsInt(p->first->first) &&
        p->first->first->integer == STR;
}

// Lambda is a special type of Seq
// form [[lambda args] expr]
bool ValIsLambda(Val *p)
{
    return p && IsSeq(p) && IsSeq(p->first) && p->first &&
        IsInt(p->first->first) && p->first->first->integer == LAMBDA;
}

bool CharIsSpace(char c)
{
    switch (c)
    {
        case '\0':
        case '"':
        case '@':
        case '[':
        case ']':
        case '#':
        case '-':
        case '_':
            // Not space:
            // * NULL char
            // * Reader macro chars: string, and get
            // * list brackets
            // * base10 integer sigil
            // * integer minus sign and underscore
            return false;
        default:
            // All other non-alphanumeric characters are space
            return !isalnum(c);
    }
}

int ReadSpace(const char *s, int len)
{
    const char *view = s;
    while ((view - s) < len && CharIsSpace(*view))
    {
        view++;
    }
    return view - s;
}

// Digit to integer value
// Returns -1 upon error
int DigitValue(char d)
{
    if ('0' <= d && d <= '9')
    {
        return d - '0';
    }
    else if ('a' <= d && d <= 'z')
    {
        return d - 'a' + 10;
    }
    else if ('A' <= d && d <= 'Z')
    {
        return d - 'A' + 10;
    }
    else
    {
        return -1;
    }
}

// Returns the number of characters read
// number read -> out
int ReadInt(const char *start, int length, long *valOut)
{
    // Validate inputs
    if (!start || length <= 0)
    {
        if (valOut)
        {
            *valOut = 0;
        }
        return 0;
    }

    const char *view = start;

    // Read prefix sigil(s)
    bool neg = false;
    int base = 0;
    if (*view == '-')
    {
        neg = true;
        view++;
    }

    base = 36;
    if (*view == '#')
    {
        base = 10;
        view++;
    }
    if (!isalnum(*view))
    {
        return 0;
    }

    // Keep a pointer to where the digits start
    const char *viewDigits = view;

    long n = 0;
    int d;
    while (*view && (view - start < length))
    {
        if (isalnum(*view))
        {
            d = DigitValue(*view);
            if (0 <= d && d < base)
            {
                n = (n * base) + d;
                if (n < 0)
                {
                    // There was an overflow
                    return 0;
                }
            }
            else
            {
                // Invalid digit for base
                return 0;
            }
        }
        else if (*view != '_')
        {
            // Allow underscore to separate digits.
            // All other characters are invalid, so
            // this must be the end of the number.
            break;
        }
        view++;
    }

    // Check if there were any valid digits
    if (view == viewDigits)
    {
        // No valid digits were read after the sigils
        if (valOut)
        {
            *valOut = 0;
        }
        return 0;
    }

    // Apply sign
    if (neg)
    {
        n = -n;
    }

    // Return results
    if (valOut)
    {
        *valOut = n;
    }
    return view - start;
}

// Read string, with escape codes enabled
// Returns number of chars read
int ReadString(const char *start, int length, Val **toList)
{
    if (start && length > 0)
    {
        const char *view = start;

        // Consume the opening quote
        assert(*view == '"');
        view++;

        // make form: [[str] ...]
        Val *s = MakeSeq(MakeSeq(MakeInt(STR), NULL), NULL);
        Val *ps = s;
        while (*view && *view != '"' && view < start + length)
        {
            char c = *view;
            if (c == '\\')
            {
                view++;
                if (!*view || view >= start + length)
                {
                    // Unexpected end of input
                    break;
                }
                switch (*view)
                {
                    case '0': c = '\0'; break;
                    case 'n': c = '\n'; break;
                    case 't': c = '\t'; break;
                    case '"': c = '"'; break;
                    case '\\': c = '\\'; break;
                    default: c = *view;
                }
            }
            ps->rest = MakeSeq(MakeInt(c), NULL);
            ps = ps->rest;
            view++;
        }

        // Consume closing quote
        if (*view != '"')
        {
            // Unexpected end of input
        }
        view++;

        *toList = s;
        return view - start;
    }
    return 0;
}

// Returns number of chars read
int ReadSeq(const char *start, int length, Val **toList)
{
    // Validate arguments
    if (!start || length <= 0)
    {
        return 0;
    }

    const char *view = start;

    // Consume the opening paren
    assert(*view == '[');
    view++;

    Val *s = NULL;

    // Skip whitespace
    view += ReadSpace(view, (start+length)-view);
    if (*view != ']')
    {
        // Non-empty list
        bool valid = true;
        if (*view && *view != ']' && view < start+length)
        {
            Val *e;
            int len = ReadVal(view, (start+length)-view, &e);
            if (!len)
            {
                // Error reading element
                valid = false;
            }
            // Create first item
            s = MakeSeq(e, NULL);
            view += len;
        }
        if (valid)
        {
            // Pointer for appending to s
            Val *ps = s;
            while (*view && *view != ']' && view < start+length)
            {
                Val *e;
                int len = ReadVal(view, (start+length)-view, &e);
                if (!len)
                {
                    // Error reading element
                    break;
                }
                // Append
                ps->rest = MakeSeq(e, NULL);
                ps = ps->rest;
                view += len;
            }
        }
    }
    *toList = s;

    if (*view == ']')
    {
        // Consume the closing paren
        view++;
    }
    else
    {
        // Unexpected end of input
    }
    return view - start;
}

int ReadVal(const char *start, int length, Val **out)
{
    // Validate arguments
    if (!out || !start || length <= 0)
        return 0;

    const char *view = start;

    // Loop is for allowing comments to restart the read
    while (1)
    {
        view += ReadSpace(view, start+length-view);
        switch (*view)
        {
            case '\0':
                // End of input
                *out = NULL;
                break;
            case '@':
                // Variable getter (reader macro)
                {
                    Val *v;
                    int len = 1 + ReadVal(view+1, start+length-view-1, &v);
                    if (len)
                    {
                        view += len;
                        *out = MakeSeq(MakeInt(GET), MakeSeq(v, NULL));
                    }
                    else
                    {
                        *out = NULL;
                    }
                }
                break;
            case '"':
                // String literal (reader macro)
                {
                    Val *s = NULL;
                    int len = ReadString(view, start+length-view, &s);
                    view += len;
                    if (len)
                    {
                        *out = s;
                    }
                    else
                    {
                        *out = NULL;
                    }
                }
                break;
            case ']':
                // Unmatched list
                *out = NULL;
                break;
            case '[':
                // Read sequence / list
                {
                    Val *s = NULL;
                    int len = ReadSeq(view, start+length-view, &s);
                    view += len;
                    if (len)
                    {
                        *out = s;
                    }
                    else
                    {
                        *out = NULL;
                    }
                }
                break;
            default:
                // Read integer
                {
                    long n;
                    int len = ReadInt(view, start+length-view, &n);
                    if (len)
                    {
                        view += len;
                        *out = MakeInt(n);
                    }
                    else
                    {
                        *out = NULL;
                    }
                }
                break;
        }
        break;
    }
    view += ReadSpace(view, start+length-view);

    return view - start;
}

// Returns: number of chars written
int PrintChar(char c, char *out, int length)
{
    // Validate arguments
    if (out && length > 0)
    {
        *out = c;
        return 1;
    }
    else
    {
        return 0;
    }
}

// Returns: number of chars written
int PrintCStr(const char *s, char *out, int len)
{
    // Validate inputs
    if (s && out)
    {
        int i;
        for (i = 0; s[i] && i < len; i++)
        {
            out[i] = s[i];
        }
        return i;
    }
    return 0;
}

char ValueToDigit(int d, bool upper)
{
    if (0 <= d)
    {
        if (d <= 9)
        {
            return '0' + d;
        }
        if (d <= 35)
        {
            return (upper? 'A' : 'a') + d - 10;
        }
    }
    return '?';
}

// Returns: number of chars written
int PrintInt(int n, char *out, int len, int readable, bool base10, bool upper)
{
    assert(out);
    int base = base10? 10 : 36;
    char buf[32];
    const int sz = sizeof(buf);
    // u = magnitude of N
    int u = (n >= 0)? n : -n;

    int i;
    if (u == 0)
    {
        buf[sz - 1] = '0';
        i = 1;
    }
    else
    {
        for (i = 0; (u > 0) && (i < len); i++)
        {
            assert(i < sz);
            buf[sz - i - 1] = ValueToDigit(u % base, upper);
            u /= base;
        }
    }

    assert(i >= 1);

    // Sigil for base
    if (readable && base10)
    {
        buf[sz - i - 1] = '#';
        i++;
    }

    // Minus sign for negative numbers
    if (n < 0)
    {
        assert(i < sz);
        buf[sz - i - 1] = '-';
        i++;
    }

    memcpy(out, buf + sz - i, i);
    return i;
}

int PrintStr(Val *seq, char *out, int length, bool readable)
{
    if (length > 0 && out && seq)
    {
        char *view = out;
        Val *p = seq->rest;
        if (readable)
        {
            *view = '"';
            view++;
        }
        while (IsSeq(p) && p && view < (out + length))
        {
            Val *e = p->first;
            if (!IsInt(e))
            {
                // Value is not really a proper string
                return PrintSeq(seq, out, length, readable);
            }
            char c = (char)e->integer;
            if (readable)
            {
                switch (c)
                {
                    case '\0':
                        view += PrintChar('\\', view, length-(view-out));
                        c = '0';
                        break;
                    case '\n':
                        view += PrintChar('\\', view, length-(view-out));
                        c = 'n';
                        break;
                    case '\t':
                        view += PrintChar('\\', view, length-(view-out));
                        c = 't';
                        break;
                    case '"':
                        view += PrintChar('\\', view, length-(view-out));
                        c = '"';
                        break;
                    case '\\':
                        view += PrintChar('\\', view, length-(view-out));
                        c = '\\';
                        break;
                }
            }
            view += PrintChar(c, view, length-(view-out));
            p = p->rest;
        }
        if (readable)
        {
            *view = '"';
            view++;
        }
        return view - out;
    }
    return 0;
}

// Print sequence to string buffer
int PrintSeq(Val *seq, char *out, int length, bool readable)
{
    if (length > 0 && out)
    {
        char *view = out;
        // Print opening '['
        view += PrintChar('[', view, length);
        if (seq)
        {
            // Print 1st without a space
            if (view < (out+length))
            {
                view += PrintVal(seq->first, view, length-(view-out), readable);
                seq = seq->rest;
            }
            // Print list contents
            while (IsSeq(seq) && seq && view < (out + length))
            {
                view += PrintChar(' ', view, length-(view-out));
                view += PrintVal(seq->first, view, length-(view-out), readable);
                seq = seq->rest;
            }
        }
        // Print closing ']'
        view += PrintChar(']', view, length-(view-out));
        return view - out;
    }
    return 0;
}

int PrintLambda(Val *p, char *out, int length, bool readable)
{
    char *view = out;
    view += PrintCStr("[lambda ", view, length-(view-out));
    view += PrintSeq(p->first->rest, view, length-(view-out), readable);
    view += PrintCStr("]", view, length-(view-out));
    return view - out;
}

// Prints p to the given output stream
// Returns: number of chars written
int PrintVal(Val *p, char *out, int length, bool readable)
{
    if (length > 0)
    {
        if (IsInt(p))
        {
            return PrintInt(p->integer, out, length, readable, true, false);
        }
        if (IsStr(p))
        {
            return PrintStr(p, out, length, readable);
        }
        if (ValIsLambda(p))
        {
            return PrintLambda(p, out, length, readable);
        }
        return PrintSeq(p, out, length, readable);
    }
    return 0;
}

static bool IsSelfEvaluating(Val *ast)
{
    return ast == NULL || IsInt(ast) || IsStr(ast);
}

// Does: Read a form from the stream
// Returns: the form, which may be NULL
Val *read(const char *start, int length)
{
    Val *x = NULL;
    if (start && length > 0)
    {
        int len = ReadVal(start, length, &x);
        if (len <= 0)
        {
            x = NULL;
        }
    }
    return x;
}

Val *eval(Val *ast)
{
    while (true)
    {
        if (IsSelfEvaluating(ast))
        {
            break;
        }
        ast = NULL;
    }
    return ast;
}

void print(Val *expr, int readable)
{
    const int n = 2 * 1024;
    char buffer[n];
    int len = PrintVal(expr, buffer, sizeof(buffer), readable);
    printf("%.*s", len, buffer);
}

// Set a key-value pair in the current environment.
void EnvSet(Val **env, long key, Val *val)
{
    // [key val]
    Val *pair = MakeSeq(MakeInt(key), MakeSeq(val, NULL));
    if (*env)
    {
        Val *pairs = MakeSeq(pair, (*env)->first);
        *env = MakeSeq(pairs, (*env)->rest);
        return;
    }
    *env = MakeSeq(MakeSeq(pair, NULL), NULL);
}

// Search current environment and then check outer scope if not found.
// Environment is of the form:
// [[[key value]...] outer...]
int EnvGet(Val *env, long key, Val **out)
{
    Val *scope = env;
    while (scope && IsSeq(scope))
    {
        Val *p = scope->first;
        while (p && IsSeq(p))
        {
            Val *e = p->first;
            Val *k = e->first;
            Val *v = e->rest->first;
            if (k->integer == key)
            {
                *out = v;
                return 1;
            }
            p = p->rest;
        }
        scope = scope->rest;
    }
    *out = NULL;
    return 0;
}

void EnvSetName(Val **env, const char *base36_name, Val *val)
{
    long symbol = 0;
    ReadInt(base36_name, strlen(base36_name), &symbol);
    if (symbol)
    {
        EnvSet(env, symbol, val);
    }
}

void InitPool(void)
{
    pool_size = 30;
    pool = malloc(sizeof(*pool) * pool_size);
    assert(pool != NULL);
    for (int i = 0; i < pool_size; i++)
    {
        pool[i].first = NULL;
        pool[i].rest = &pool[i + 1];
        pool[i].is_int = 0;
        pool[i].is_func = 0;
        pool[i].is_seq = 1;
    }
    pool[pool_size - 1].rest = NULL;
    freelist = pool;
    assert(pool != NULL);
    assert(freelist != NULL);
    assert(pool_size > 0);
}

void InitLizp(void)
{
    InitPool();
    global_env = LizpInitEnv();
}

