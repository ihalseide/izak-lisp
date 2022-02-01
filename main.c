#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

#include "types.h"

// Cell memory:
Cell *cell_pool = NULL;
int cell_pool_cap = 0;

// String memory:
char *char_pool = NULL;
char *char_free = NULL;
int char_pool_cap = 0;
Cell *string_list = NULL;

// Constant symbols
Cell * c_nil;
Cell * c_true;
Cell * c_false;

char char_end (char c)
{
	switch (c)
	{
		case '(': return ')';
		case '[': return ']';
		case '{': return '}';
		case '|': return '|';
		default: return 0;
	}
}

char char_is_paren (char c)
{
	return (c == '(') || (c == ')')
		|| (c == '[') || (c == ']')
		|| (c == '{') || (c == '}')
		|| (c == '|');
}

int char_is_symbol (char c)
{
	return !char_is_paren(c) && !isspace(c);
}

// Checks two character streams for equality.
// If len == 0, then this is equivalent to strcmp(s1,s2) == 0
// Otherwise, this only returns true if s1 and s2 are equal up to
// the `len`th character.
bool stream_eq (const char *s1, const char *s2, int len)
{
	// Validate arguments
	if ((s1 == NULL) || (s2 == NULL) || (len < 0))
	{
		return false;
	}

	if (s1 == s2)
	{
		return true;
	}

	if (len)
	{
		// Compare length strings
		for (int i = 0; i < len; i++)
		{
			if (s1[i] != s2[i])
			{
				return false;
			}
		}
		return true;
	}
	else
	{
		// Compare c-style string
		while (*s1 && *s2 && *s1 == *s2)
		{
			s1++;
			s2++;
		}
		return *s1 == *s2;
	}
}

// Removes a cell from the cell pool a.k.a. free list
Cell *cell_get ()
{
	if (!cell_pool)
	{
		return NULL;
	}

	Cell *x = cell_pool->as_pair.rest;
	if (x)
	{
		cell_pool->as_pair.rest = x->as_pair.rest;
	}
	return x;
}

Cell *cell_init (enum Cell_kind k)
{
	Cell *x = cell_get();
	if (x)
	{
		_Static_assert(_CELL_KIND_COUNT == 5, "handle all cell kinds");
		if ((T_INT <= k) && (k <= _CELL_KIND_COUNT))
		{
			x->kind = k;
		}
		else
		{
			fprintf(stderr, "cell_init: invalid cell kind\n");
			exit(1);
		}
	}
	return x;
}

Cell *make_pair (Cell *first, Cell *rest)
{
	Cell *x = cell_init(T_PAIR);
	if (x)
	{
		x->as_pair.first = first;
		x->as_pair.rest = rest;
	}
	return x;
}

Cell *make_empty_list ()
{
	return make_pair(NULL, c_nil);
}

bool is_list (Cell *x)
{
	return (x != NULL) && (x->kind == T_PAIR);
}

bool is_empty_list (Cell *x)
{
	return is_list(x) && (x->as_pair.first == NULL);
}

Cell *make_symbol (Cell *name)
{
	Cell *x = cell_init(T_SYMBOL);
	if (x)
	{
		x->as_symbol = name;
	}
	return x;
}

Cell *make_string (char *start, int length)
{
	Cell *x = cell_init(T_STRING);
	if (x)
	{
		x->as_str.start = start;
		x->as_str.length = length;
	}
	return x;
}

// only for use by string_intern
Cell *string_create (const char *start, int length)
{
	// Validate inputs
	if (start == NULL || length <= 0)
	{
		return NULL;
	}

	// Check memory space
	if ((char_free - char_pool) >= char_pool_cap)
	{
		fprintf(stderr, "string_create: out of string character memory\n");
		exit(1);
	}

	// Copy string
	char *res = char_free;
	for (int i = 0; i < length; i++)
	{
		*char_free++ = start[i];
	}

	// Add string terminator for easy use with C 
	// (Even though every string is length encoded,
	//  it's a hassle to add a null terminator if it isn't already there)
	*char_free++ = '\0';

	// Insert a new string into the string list
	Cell *x = make_pair(make_string(res, length), string_list->as_pair.rest);
	string_list->as_pair.rest = x;

	// Return the string
	return x->as_pair.first;
}

// Use this when creating new strings from short-lived char pointers
// returns a string cell
Cell *string_intern (const char *start, int length)
{
	if ((start == NULL) || (length < 0))
	{
		return NULL;
	}

	// Linear search through the (circular) string list
	Cell *p = string_list;
	do
	{
		Cell *s = p->as_pair.first;
		if ((s->as_str.length == length)
				&& stream_eq(s->as_str.start, start, length))
		{
			// Found an internal string, so return that
			return p->as_pair.first;
		}

		// Next item in string list
		p = p->as_pair.rest;
	}
	while (p != string_list);

	// Did not find an internal string, so make new one
	return string_create(start, length);
}

Cell *string_intern_cstring (char *str)
{
	return string_intern(str, strlen(str));
}

// Inserts a cell back into the free list
void cell_free (Cell *x)
{
	if (!x || !cell_pool)
	{
		return;
	}

	x->as_pair.rest = cell_pool->as_pair.rest;
	cell_pool->as_pair.rest = x;
}

Cell *make_int (int n)
{
	Cell *x = cell_init(T_INT);
	if (x)
	{
		x->as_int = n;
	}
	return x;
}

Cell *make_cfunc (Cell *(*c_func)(Cell*))
{
	Cell *x = cell_init(T_C_FUNCTION);
	if (x)
	{
		x ->as_func = c_func;
	}
	return x;
}

bool symbol_eq (const Cell *s1, const Cell *s2)
{
	// Validate inputs
	if ((s1 == NULL) || (s2 == NULL)
			|| (s1->kind != T_SYMBOL) || (s2->kind != T_SYMBOL)
			|| (s1->as_symbol == NULL) || (s2->as_symbol == NULL))
	{
		return false;
	}

	return ((s1 == s2) || (s1->as_symbol == s2->as_symbol));
}

int read_symbol (const char *start, int length, Cell **out)
{
	// Get how much of `s` is alphabetical chars
	int i;
	for (i = 0; (i < length) && char_is_symbol(start[i]); i++)
	{
		continue;
	}
	Cell *name = string_intern(start, i);
	*out = make_symbol(name);
	return i;
}

int read_int (const char *start, int length, Cell **out)
{
	int start_length = length;

	int sign = 1;
	if (*start == '-')
	{
		sign = -sign;
		start++;
		length--;
	}

	int n = 0;
	while (isdigit(*start) && (length > 0))
	{
		n = (n * 10) + ((*start) - '0');
		start++;
		length--;
	}

	int num_len = start_length - length;
	*out = make_int(n * sign);
	return num_len;
}

void string_skip_white(const char **stream, int *length)
{
	const char *view = *stream;
	int rem = *length;
	while (isspace(*view) && (rem > 0))
	{
		view++;
		rem--;
	}
	*stream = view;
	*length = rem;
}

void string_step (const char **stream, int *length, int n)
{
	*stream += n;
	*length -= n;
}

int read_form (const char *start, int length, Cell **out);

int read_list (const char *start, int length, Cell **out)
{
	const char *view = start;
	int rem = length;

	// Consume the opening character
	char opener = *view;
	char closer = char_end(opener);
	string_step(&view, &rem, 1);

	// Check if there are no elements
	string_skip_white(&view, &rem);
	if (*view == closer)
	{
		// empty list
		// (empty list = pair where first = NULL)
		// consume the final character
		string_step(&view, &rem, 1);

		*out = make_empty_list();
		(*out)->as_pair.variant = opener;

		int len = view - start;
		return len;
	}

	// Read the first element
	Cell *e;
	string_step(&view, &rem, read_form(view, rem, &e));
	string_skip_white(&view, &rem);

	// Read the rest of the normal elements (don't handle the dot)
	Cell *list, *p;
	p = list = make_pair(e, c_nil);
	while ((rem > 0) && (*view != closer) && (*view != '.'))
	{
		string_step(&view, &rem, read_form(view, rem, &e));
		p->as_pair.rest = make_pair(e, c_nil);
		p = p->as_pair.rest;
		string_skip_white(&view, &rem);
	}

	// Handle either the optionally dotted end of the list
	bool has_dot = false;

	if (*view == '.')
	{
		// Dotted end of the list:
		has_dot = true;
		// consume the '.' dot
		string_step(&view, &rem, 1);
		// read what should be the final element
		string_step(&view, &rem, read_form(view, rem, &e));
		p->as_pair.rest = e;
		string_skip_white(&view, &rem);
	}

	if (*view == closer)
	{
		// The actual end of list:
		// consume the final character
		string_step(&view, &rem, 1);
		*out = list;
		(*out)->as_pair.variant = opener;
		int len = length - rem;
		return len;
	}
	else
	{
		// Unexpected end of list, or multiple items after the '.' dot
		if (has_dot)
		{
			fprintf(stderr, "expected the form directly after the '.' (dot) to be the final form of the enclosing list\n");
		}
		else
		{
			fprintf(stderr, "unexpected end of list\n");
		}
		exit(1);
	}
}

// Env = (outer . list:slot)
// Slot/pair = (symbol . value)
Cell *env_create (Cell *env_outer)
{
	if (env_outer == NULL)
	{
		return NULL;
	}

	return make_pair(env_outer, make_empty_list());
}

// Search only the current env for the symbol
// Returns:
//   when found -> the (symbol . value) "slot"
//   not found -> NULL!
Cell *env_get_self (Cell *env, const Cell *sym)
{
	// Search through the (symbol . value) list in the environment
	Cell *slots = env->as_pair.rest;
	while (slots->as_pair.first != NULL)
	{
		if (symbol_eq(slots->as_pair.first->as_pair.first, sym))
		{
			return slots->as_pair.first;
		}

		slots = slots->as_pair.rest;
	}
	return NULL;
}

// Find the innermost env which contains symbol
// Returns: environment Cell or NULL!
Cell *env_find (Cell *env, const Cell *sym)
{
	// Validate inputs
	if ((env == NULL) || (sym == NULL))
	{
		return NULL;
	}

	// Search up the environment hierarchy
	while (is_list(env) && !is_empty_list(env))
	{
		Cell *slot = env_get_self(env, sym);
		if (slot != NULL)
		{
			return env;
		}

		// Move on to the outer environment
		env = env->as_pair.first;
	}

	// Not found
	return NULL;
}

// Get the innermost definition for the symbol
// Returns:
//   when found -> the value of the symbol
//   when not found -> NULL!
Cell *env_get (Cell *env, Cell *sym)
{
	// Validate inputs
	if ((env == NULL) || (sym == NULL))
	{
		return NULL;
	}

	// Find the environment which contains the symbol
	Cell *containing_env = env_find(env, sym);
	if (containing_env == NULL)
	{
		// Symbol not found
		return NULL;
	}
	else
	{
		// Fetch the symbol from the environment it's in
		Cell *slot = env_get_self(containing_env, sym);
		return slot->as_pair.rest;
	}
}

void env_set (Cell *env, Cell *sym, Cell *val)
{
	// Validate inputs. Val is allowed to be null to "undefine" things.
	if ((env == NULL) || (sym == NULL))
	{
		return;
	}

	// Debug
	if (val == NULL)
	{
		printf("  note: undefining a symbol\n");
	}


	// If there is already a symbol defined, change the value,
	// otherwise add the new symbol with the value.
	Cell *slot = env_get_self(env, sym);
	if (slot == NULL)
	{
		// Push the new (symbol . value) pair to the env
		slot = make_pair(sym, val);
		env->as_pair.rest = make_pair(slot, env->as_pair.rest);
	}
	else
	{
		// Change the value of the already present slot
		slot->as_pair.rest = val;
	}
}

void env_set_c (Cell *env, char *cstr, Cell *val)
{
	if ((env == NULL) || (cstr == NULL) || (val == NULL))
	{
		return;
	}

	Cell *name = string_intern_cstring(cstr);
	Cell *sym = make_symbol(name);
	env_set(env, sym, val);
}


int read_form (const char *start, int length, Cell **out)
{
	const char *view = start;
	int rem = length;

	string_skip_white(&view, &rem);

	switch (*view)
	{
		case '(':
		case '{':
		case '[':
		case '|':
			// Opening paren, for lists
			string_step(&view, &rem, read_list(view, rem, out));
			break;
		case ')':
		case '}':
		case ']':
			// Closing paren, shouldn't appear in valid text with matched parens
			fprintf(stderr, "read_form: unmatched closing '%c' character\n", *view);
			exit(1);
		case '.':
			// Should only be inside when reading a list
			fprintf(stderr, "read_form: unexpected '.' (dot) character\n");
			exit(1);
		case '-':
			// Number
			string_step(&view, &rem, read_int(view, rem, out));
			break;
		case '\0':
			// Null terminator for strings
			fprintf(stderr, "read_form: unexpected end of string");
			exit(1);
		default:
			if (isdigit(*view))
			{
				string_step(&view, &rem, read_int(view, rem, out));
			}
			else
			{
				string_step(&view, &rem, read_symbol(view, rem, out));
			}
	}
	return view - start;
}

int print_char (char c, char *out, int length)
{
	if (out && (length > 0))
	{
		*out = c;
		return 1;
	}
	return 0;
}

// returns number of chars written
int print_cstr (char *s, char *out, int length)
{
	// Validate inputs
	if ((s == NULL) || (out == NULL) || (length <= 0))
	{
		return 0;
	}

	int i;
	for (i = 0; s[i] && i < length; i++)
	{
		out[i] = s[i];
	}
	return i;
}

// Print out a string cell
// returns number of chars written
int print_string (const char *start, int ilength, char *out, int length)
{
	// Validate inputs
	if ((start == NULL) || (out == NULL) || (ilength <= 0) || (length <= 0))
	{
		return 0;
	}

	int i;
	for (i = 0; (i < ilength) && (i < length); i++)
	{
		out[i] = start[i];
	}

	return i;
}

// returns number of chars written
int print_int (int n, char *out, int length)
{
	char buf[20];
	int u = (n >= 0)? n : -n;

	// Remaining length
	int rem = length;

	int i = sizeof(buf) - 1;
	while ((rem > 0) && (i >= 0))
	{
		buf[i] = '0' + (u % 10);
		u /= 10;
		if (u <= 0) { break; }
		rem--;
		i--;
	}

	// Add minus sign
	if (n < 0)
	{
		buf[--i] = '-';
	}

	int len = sizeof(buf) - i;
	memcpy(out, buf + i, len);
	return len;
}

int print_form (Cell *x, char *out, int length);

int print_pair (Cell *x, char *out, int length)
{
	char *view = out;
	int rem = length;

	// Remember the type of parens used to create the list (paren is default)
	char opener = (x->as_pair.variant)? x->as_pair.variant : '(';
	char closer = char_end(opener);

	string_step((const char**)&view, &rem, print_char(opener, view, rem));
	if (x->as_pair.first != NULL)
	{
		while (x != c_nil)
		{
			string_step((const char**)&view, &rem, print_form(x->as_pair.first, view, rem));

			if (x->as_pair.rest == c_nil)
			{
				break;
			}

			// See if the list continues with more pairs...
			if (x->as_pair.rest->kind == T_PAIR)
			{
				// Step into the 'rest' pair
				if (x->as_pair.rest->as_pair.first != NULL)
				{
					string_step((const char**)&view, &rem, print_char(' ', view, rem));
				}
				x = x->as_pair.rest;
			}
			else
			{
				// Dotted list because the rest of this pair is not a pair
				string_step((const char**)&view, &rem, print_cstr(" . ", view, rem));
				string_step((const char**)&view, &rem, print_form(x->as_pair.rest, view, rem));
				break;
			}
		}
	}
	string_step((const char**)&view, &rem, print_char(closer, view, rem));

	int len = length - rem;
	return len;
}

// returns number of chars written
int print_form (Cell *x, char *out, int length)
{
	if ((x == NULL) || (length == 0))
	{
		return 0;
	}

	_Static_assert(_CELL_KIND_COUNT == 5, "exhaustive handling of all cell kinds");
	switch (x->kind)
	{
		case T_INT:
			return print_int(x->as_int, out, length);
		case T_STRING:
			return print_string(x->as_str.start, x->as_str.length, out, length);
		case T_SYMBOL:
			// Recurse by printing the symbol's string name
			return print_form(x->as_symbol, out, length);
		case T_C_FUNCTION:
			return print_cstr("#<code>", out, length);
		case T_PAIR:
			return print_pair(x, out, length);
		default:
			// error
			fprintf(stderr, "cell_print: invalid cell kind\n");
			exit(1);
			break;
	}
}

void PRINT (Cell *x);

Cell *READ ()
{
	char buffer[1000];
	if(!fgets(buffer, sizeof(buffer), stdin))
	{
		fprintf(stderr, "READ: fgets failed\n");
		exit(1);
	}

	Cell *x;
	read_form(buffer, strlen(buffer), &x);

	return x;
}

Cell *EVAL (Cell *, Cell *env);

// Evaluate each item of list x
// does not modify x
Cell *eval_list (Cell *env, Cell *x)
{
	Cell *y = make_pair(NULL, c_nil);
	Cell *p_y = y;
	Cell *p_x = x;

	// Turn x into y by evaluating each item of x:

	// eval the first element
	p_y->as_pair.first = EVAL(p_x->as_pair.first, env);
	p_x = p_x->as_pair.rest;

	// eval the rest of the elements
	while (p_x != c_nil && p_x->as_pair.first)
	{
		p_y->as_pair.rest = make_pair(EVAL(p_x->as_pair.first, env), c_nil);

		// next
		p_x = p_x->as_pair.rest;
		p_y = p_y->as_pair.rest;
	}

	// Copy the variant kind
	y->as_pair.variant = x->as_pair.variant;

	return y;
}


Cell *symbol_lookup (Cell *env, Cell *x)
{
	Cell *val = env_get(env, x);
	if (val == NULL)
	{
		val = string_intern_cstring("error: undefined symbol");
	}

	return val;
}

Cell *eval_apply (Cell *env, Cell *list)
{
	if (env == NULL || list == NULL)
	{
		return NULL;
	}

	// Eval each item in list
	Cell *y = eval_list(env, list);

	// List application
	Cell *fn = y->as_pair.first;
	Cell *args = y->as_pair.rest;

	// Validate
	assert(fn != NULL);
	assert(args != NULL);

	if (fn->kind == T_C_FUNCTION)
	{
		// Run C function with args
		Cell *(*c_func)(Cell*) = fn->as_func;
		Cell *result = c_func(args);
		return result;
	}
	else if (fn->kind == T_PAIR)
	{
		// Error
		return string_intern_cstring("error: unknown function");
	}
	else
	{
		// Error
		return string_intern_cstring("error: not a function");
	}
}

bool is_self_evaluating (Cell* x)
{
	// Validate inputs
	if (x == NULL)
	{
		return false;
	}

	return ((x == c_nil)
			|| (x == c_true)
			|| (x == c_false)
			|| (x->kind == T_INT)
			|| (x->kind == T_STRING)
			|| is_empty_list(x));
}

Cell *EVAL (Cell *x, Cell *env)
{
	if (is_self_evaluating(x))
	{
		return x;
	}

	switch (x->kind)
	{
		case T_INT:
		case T_STRING:
			assert(0 && "should have been caught by is_self_evaluating");
			break;
		case T_SYMBOL:
			return symbol_lookup(env, x);
		case T_PAIR:
			return eval_apply(env, x);
		default:
			// error
			fprintf(stderr, "cell_init: invalid cell kind\n");
			exit(1);
	}
}

void PRINT (Cell *expr)
{
	char buffer[1000];

	int p_len = print_form(expr, buffer, sizeof(buffer));

	printf("%.*s\n", p_len, buffer);
}

void rep (Cell *env)
{
	Cell * form = READ();
	Cell * value = EVAL(form, env);
	PRINT(value);
}

// Built-in functions:

Cell *bi_plus (Cell *args)
{
	int a = args->as_pair.first->as_int;
	int b = args->as_pair.rest->as_pair.first->as_int;
	return make_int(a + b);
}

Cell *bi_minus (Cell *args)
{
	int a = args->as_pair.first->as_int;
	int b = args->as_pair.rest->as_pair.first->as_int;
	return make_int(a - b);
}

Cell *bi_star (Cell *args)
{
	int a = args->as_pair.first->as_int;
	int b = args->as_pair.rest->as_pair.first->as_int;
	return make_int(a * b);
}

Cell *bi_slash (Cell *args)
{
	int a = args->as_pair.first->as_int;
	int b = args->as_pair.rest->as_pair.first->as_int;
	return make_int(a / b);
}

// How to set up the cell memory
// SHOULD ONLY BE CALLED ONCE
void pools_init (int ncells, int nchars)
{
	// Allocate the arrays
	cell_pool = malloc(ncells * sizeof(*cell_pool));
	char_pool = malloc(nchars);
	char_free = char_pool;

	// Check the malloc'd pointers
	if (!cell_pool || !char_pool)
	{
		fprintf(stderr, "pools_init: malloc failed\n");
		exit(1);
	}

	// Set the capacities
	cell_pool_cap = ncells;
	char_pool_cap = nchars;

	// Link the free cells together in a circular list
	for (int i = 0; i < (cell_pool_cap - 1); i++)
	{
		cell_pool[i].as_pair.rest = &cell_pool[i + 1];
	}
	cell_pool[cell_pool_cap - 1].as_pair.rest = cell_pool;

	// Set up the internal string circular list with one item
	Cell *empty_s = make_string(NULL, 0);
	string_list = make_pair(empty_s, NULL);
	string_list->as_pair.rest = string_list;

	// Create the constant symbols
	c_nil = make_symbol(string_intern_cstring("nil"));
	c_true = make_symbol(string_intern_cstring("true"));
	c_false = make_symbol(string_intern_cstring("false"));
}

int main (int argc, char **argv)
{
	// Initialize memories
	pools_init(1024, 2048);

	// Initialize the REPL environment
	Cell *repl_env = env_create(c_nil);
	env_set_c(repl_env, "nil", c_nil);
	env_set_c(repl_env, "true", c_true);
	env_set_c(repl_env, "false", c_false);
	env_set_c(repl_env, "+", make_cfunc(bi_plus));
	env_set_c(repl_env, "-", make_cfunc(bi_minus));
	env_set_c(repl_env, "*", make_cfunc(bi_star));
	env_set_c(repl_env, "/", make_cfunc(bi_slash));

	// Print out the environment for debugging
	printf("env:\n");
	PRINT(repl_env);
	printf("----------\n");

	while(1)
	{
		printf("user> ");
		rep(repl_env);
	}
	return 0;
}
