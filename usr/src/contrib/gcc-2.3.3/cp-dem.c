/* Demangler for GNU C++ 
   Copyright (C) 1989, 1992 Free Software Foundation, Inc.
   written by James Clark (jjc@jclark.uucp)
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* This is for g++ 1.36.1 (November 6 version). It will probably
   require changes for any other version.

   Modified for g++ 1.36.2 (November 18 version).

   Modified for g++ 1.90.06 (December 31 version).

   Modified for g++ 1.95.03 (November 13 version).  */

/* This file exports one function

   char *cplus_demangle (const char *name)

   If NAME is a mangled function name produced by GNU C++, then
   a pointer to a malloced string giving a C++ representation
   of the name will be returned; otherwise NULL will be returned.
   It is the caller's responsibility to free the string which
   is returned.

   For example,
   
   cplus_demangle ("_foo__1Ai")
   
   returns

   "A::foo(int)"

   This file imports xmalloc and xrealloc, which are like malloc and
   realloc except that they generate a fatal error if there is no
   available memory. */

/* #define nounderscore 1 /* define this is names don't start with _ */

#include <stdio.h>
#include <ctype.h>

#ifdef USG
#include <memory.h>
#else
#define memcpy(s1, s2, n) bcopy ((s2), (s1), (n))
#define memcmp(s1, s2, n) bcmp ((s2), (s1), (n))
#endif

/* This is '$' on systems where the assembler can deal with that.
   Where the assembler can't, it's '.' (but on many systems '.' is
   used for other things).  */
#if !defined (CPLUS_MARKER)
#define CPLUS_MARKER '$'
#endif

#ifndef __STDC__
#define const
#endif

#ifdef __STDC__
extern char *cplus_demangle (const char *type);
#else
extern char *cplus_demangle ();
#endif

#ifdef __STDC__
extern char *xmalloc (int);
extern char *xrealloc (char *, int);
extern void free (char *);
#else
extern char *xmalloc ();
extern char *xrealloc ();
extern void free ();
#endif

static char **typevec = 0;
static int ntypes = 0;
static int typevec_size = 0;

static struct {
  const char *in;
  const char *out;
} optable[] = {
  "nw", " new",			/* new (1.92, ansi) */
  "dl", " delete",		/* new (1.92, ansi) */
  "new", " new",		/* old (1.91) */
  "delete", " delete",		/* old (1.91) */
  "ne", "!=",			/* old, ansi */
  "eq", "==",			/* old, ansi */
  "ge", ">=",			/* old, ansi */
  "gt", ">",			/* old, ansi */
  "le", "<=",			/* old, ansi */
  "lt", "<",			/* old, ansi */
  "plus", "+",			/* old */
  "pl", "+",			/* ansi */
  "apl", "+=",			/* ansi */
  "minus", "-",			/* old */
  "mi", "-",			/* ansi */
  "ami", "-=",			/* ansi */
  "mult", "*",			/* old */
  "ml", "*",			/* ansi */
  "aml", "*=",			/* ansi */
  "convert", "+",		/* old (unary +) */
  "negate", "-",		/* old (unary -) */
  "trunc_mod", "%",		/* old */
  "md", "%",			/* ansi */
  "amd", "%=",			/* ansi */
  "trunc_div", "/",		/* old */
  "dv", "/",			/* ansi */
  "adv", "/=",			/* ansi */
  "truth_andif", "&&",		/* old */
  "aa", "&&",			/* ansi */
  "truth_orif", "||",		/* old */
  "oo", "||",			/* ansi */
  "truth_not", "!",		/* old */
  "nt", "!",			/* ansi */
  "postincrement", "++",	/* old */
  "pp", "++",			/* ansi */
  "postdecrement", "--",	/* old */
  "mm", "--",			/* ansi */
  "bit_ior", "|",		/* old */
  "or", "|",			/* ansi */
  "aor", "|=",			/* ansi */
  "bit_xor", "^",		/* old */
  "er", "^",			/* ansi */
  "aer", "^=",			/* ansi */
  "bit_and", "&",		/* old */
  "ad", "&",			/* ansi */
  "aad", "&=",			/* ansi */
  "bit_not", "~",		/* old */
  "co", "~",			/* ansi */
  "call", "()",			/* old */
  "cl", "()",			/* ansi */
  "cond", "?:",			/* old */
  "alshift", "<<",		/* old */
  "ls", "<<",			/* ansi */
  "als", "<<=",			/* ansi */
  "arshift", ">>",		/* old */
  "rs", ">>",			/* ansi */
  "ars", ">>=",			/* ansi */
  "component", "->",		/* old */
  "rf", "->",			/* ansi */
  "indirect", "*",		/* old */
  "method_call", "->()",	/* old */
  "addr", "&",			/* old (unary &) */
  "array", "[]",		/* old */
  "vc", "[]",			/* ansi */
  "compound", ",",		/* old */
  "cm", ",",			/* ansi */
  "nop", "",			/* old (for operator=) */
  "as", "=",			/* ansi */
};

/* Beware: these aren't '\0' terminated. */

typedef struct {
  char *b;			/* pointer to start of string */
  char *p;			/* pointer after last character */
  char *e;			/* pointer after end of allocated space */
} string;

#ifdef __STDC__
static void string_need (string *s, int n);
static void string_delete (string *s);
static void string_init (string *s);
static void string_clear (string *s);
static int string_empty (string *s);
static void string_append (string *p, const char *s);
static void string_appends (string *p, string *s);
static void string_appendn (string *p, const char *s, int n);
static void string_prepend (string *p, const char *s);
#if 0
static void string_prepends (string *p, string *s);
#endif
static void string_prependn (string *p, const char *s, int n);
static int get_count (const char **type, int *count);
static int do_args (const char **type, string *decl);
static int do_type (const char **type, string *result);
static int do_arg (const char **type, string *result);
static int do_args (const char **type, string *decl);
static void munge_function_name (string *name);
static void remember_type (const char *type, int len);
#else
static void string_need ();
static void string_delete ();
static void string_init ();
static void string_clear ();
static int string_empty ();
static void string_append ();
static void string_appends ();
static void string_appendn ();
static void string_prepend ();
static void string_prepends ();
static void string_prependn ();
static int get_count ();
static int do_args ();
static int do_type ();
static int do_arg ();
static int do_args ();
static void munge_function_name ();
static void remember_type ();
#endif

int
get_simple_count (type, res)
     char **type;
     int *res;
{
  int n = 0, success = 1;;
  
  do
    {
      n *= 10;
      n += **type - '0';
      *type += 1;
    } 
  while (isdigit (**type));
  if (strlen (*type) < n)
    {
      success = 0;
    }

  *res = n;
  return success;
}

char *
cplus_demangle (type)
     const char *type;
{
  string decl;
  int n;
  int success = 0;
  int constructor = 0;
  int destructor = 0;
  int static_type = 0;
  int const_flag = 0;
  int i;
  const char *p;
#ifndef LONGERNAMES
  const char *premangle;
#endif

  if (type == NULL || *type == '\0')
    return NULL;
#ifndef nounderscore
  if (*type++ != '_')
    return NULL;
#endif
  p = type;
  while (*p != '\0' && !(*p == '_' && p[1] == '_'))
    p++;
  if (*p == '\0')
    {
      /* destructor */
      if (type[0] == '_' && type[1] == CPLUS_MARKER && type[2] == '_')
	{
	  destructor = 1;
	  p = type;
	}
      /* static data member */
      else if (*type != '_' && (index (type, CPLUS_MARKER) != NULL))
	{
	  static_type = 1;
	  p = type;
	}
      /* virtual table "_vt$"  */
      else if (type[0] == '_' && type[1] == 'v' && type[2] == 't' && type[3] == CPLUS_MARKER)
	{
	  int n = strlen (type + 4) + 14 + 1;
	  char *tem = (char *) xmalloc (n);
	  strcpy (tem, type + 4);
	  strcat (tem, " virtual table");
	  return tem;
	}
      else return NULL;
    }

  string_init (&decl);

  if (static_type)
    {
      if (!isdigit (p[0]) && ('t' != p[0]))
	{
	  string_delete (&decl);
	  return NULL;
	}
    }
  else if (destructor)
    {
      if (!isdigit (p[3])&& ('t' != p[3]))
	{
	  string_delete (&decl);
	  return NULL;
	}
      p += 3;
    }
  else if (p == type)
    {
      if (!isdigit (p[2]) && ('t' != p[2]))
	{
	  p += 1;
	  while (*p != '\0' && !(*p == '_' && p[1] == '_'))
	    p++;
	  string_appendn (&decl, type, p - type);	  
	  string_appendn (&decl, "", 1);
	  munge_function_name (&decl);
	  if (decl.b[0] == '_')
	    {
	      string_delete (&decl);
	      return NULL;
	    }
	  else
	    p += 2;
	}
      else
	{
	  constructor = 1;
	  p += 2;
	}
    }
  else
    {
      string_appendn (&decl, type, p - type);
      decl.p[0] = '0';
      munge_function_name (&decl);
      p += 2;
    }

#ifndef LONGERNAMES
  premangle = p;
#endif
  switch (*p)
    {
    case 'C':
      /* a const member function */
      if (!isdigit (p[1]))
	{
	  string_delete (&decl);
	  return NULL;
	}
      p += 1;
      const_flag = 1;
      /* fall through */
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      n = 0;
      do
	{
	  n *= 10;
	  n += *p - '0';
	  p += 1;
	}
      while (isdigit (*p));
      if (strlen (p) < n)
	{
	  string_delete (&decl);
	  return NULL;
	}
      if (constructor || destructor)
	{
	  string_appendn (&decl, p, n);
	  string_append (&decl, "::");
	  if (destructor)
	    string_append(&decl, "~");
	  string_appendn (&decl, p, n);
	}
      else
	{
	  string_prepend (&decl, "::");
	  string_prependn (&decl, p, n);
	}
      p += n;
#ifndef LONGERNAMES
      remember_type (premangle, p - premangle);
#endif
      if (static_type)
	{
	  string_append(&decl, p+1);
	  p += strlen(p);
	  success = 1;
	}
      else
	success = do_args (&p, &decl);
      if (const_flag)
	string_append (&decl, " const");
      break;
    case 'F':
      p += 1;
      success = do_args (&p, &decl);
      break;
    /* template additions */
    case 't':
      p += 1;
      {
	int r, i;
	string tname;
	string trawname;
	
	string temp;
	int need_comma = 0;
	
	string_init(&tname);
	string_init(&trawname);
	
	/* get template name */
	if (!get_simple_count (&p, &r))
	  {
	    string_delete (&decl);
	    return 0;
	  }
	string_appendn (&tname, p, r);
	string_appendn (&trawname, p, r);
	string_appendn (&trawname, "", 1);
	p += r;
	string_append (&tname, "<");
	/* get size of template parameter list */
	if (!get_count (&p, &r))
	  return 0;
	for (i = 0; i < r; i++)
	  {
	    if (need_comma)
	      string_append (&tname, ", ");
	    /* Z for type parameters */
	    if (*p == 'Z')
	      {
		p += 1;
		
		success = do_type (&p, &temp);
		string_appendn (&temp, "", 1);
		if (success)
		  string_append (&tname, temp.b);
		string_delete(&temp);
		if (!success)
		  break;
	      }
	    /* otherwise, value parameter */
	    else
	      {
		const char *old_p  = p;
		int is_pointer = 0;
		int is_real = 0;
		int is_integral = 0;
		int done = 0;

		success = do_type (&p, &temp);
		string_appendn (&temp, "", 1);
		if (success)
		  string_append (&tname, temp.b);
		string_delete(&temp);
		if (!success)
		  break;
		string_append (&tname, "=");
		while (*old_p && !done)
		  {	
		    switch (*old_p)
		      {
		      case 'P':
		      case 'R':
			done = is_pointer = 1;
			break;
		      case 'C':	/* const */
		      case 'U':	/* unsigned */
		      case 'V':	/* volatile */
		      case 'F':	/* function */
		      case 'M':	/* member function */
		      case 'O':	/* ??? */
			old_p++;
			continue;
		      case 'Q':	/* repetition of following */
		      case 'T':	/* remembered type */
			abort();
			break;
		      case 'v':	/* void */
			abort();
			break;
		      case 'x':	/* long long */
		      case 'l':	/* long */
		      case 'i':	/* int */
		      case 's':	/* short */
		      case 'c':	/* char */
		      case 'w': /* wchar_t */
			done = is_integral = 1;
			break;
		      case 'r':	/* long double */
		      case 'd':	/* double */
		      case 'f':	/* float */
			done = is_real = 1;
			break;
		      default:
			abort();
		      }
		  }
		if (is_integral)
		  {
		    if (*p == 'm')
		      {
			string_appendn (&tname, "-", 1);
			p++;
		      }
		    while (isdigit (*p))	
		      {
			string_appendn (&tname, p, 1);
			p++;
		      }
		  }
		else if (is_real)
		  {
		    if (*p == 'm')
		      {
			string_appendn (&tname, "-", 1);
			p++;
		      }
		    while (isdigit (*p))	
		      {
			string_appendn (&tname, p, 1);
			p++;
		      }
		    if (*p == '.') /* fraction */
		      {
			string_appendn (&tname, ".", 1);
			p++;
			while (isdigit (*p))	
			  {
			    string_appendn (&tname, p, 1);
			    p++;
			  }
		      }
		    if (*p == 'e') /* exponent */
		      {
			string_appendn (&tname, "e", 1);
			p++;
			while (isdigit (*p))	
			  {
			    string_appendn (&tname, p, 1);
			    p++;
			  }
		      }
		  }
		else if (is_pointer)
		  {
		    int symbol_len;
		    
		    if (!get_count (&p, &symbol_len))
		      {
			success = 0;
			break;
		      }
		    string_appendn (&tname, p, symbol_len);
		    p += symbol_len;
		  }
	      }
	    need_comma = 1;
	  }
	string_append (&tname, ">::");
	if (destructor)
	  string_append(&tname, "~");
	if (constructor || destructor) {
	  string_appendn (&trawname, "", 1);
	  string_append (&tname, trawname.b);
	}
	string_delete(&trawname);
	
	if (!success) {
	  string_delete(&tname);
	  return 0;
	}
	string_appendn (&tname, "", 1);
	string_prepend (&decl, tname.b);
	string_delete (&tname);

	if (static_type)
	  {
	    string_append (&decl, p+1);
	    p += strlen (p);
	    success = 1;
	  }
	else
	  success = do_args (&p, &decl);
	break;
      }
    }

  for (i = 0; i < ntypes; i++)
    if (typevec[i] != NULL)
      free (typevec[i]);
  ntypes = 0;
  if (typevec != NULL)
    {
      free ((char *)typevec);
      typevec = NULL;
      typevec_size = 0;
    }

  if (success)
    {
      string_appendn (&decl, "", 1);
      return decl.b;
    }
  else
    {
      string_delete (&decl);
      return NULL;
    }
}

static int
get_count (type, count)
     const char **type;
     int *count;
{
  if (!isdigit (**type))
    return 0;
  *count = **type - '0';
  *type += 1;
  /* see flush_repeats in cp-method.c */
  if (isdigit (**type))
    {
      const char *p = *type;
      int n = *count;
      do 
	{
	  n *= 10;
	  n += *p - '0';
	  p += 1;
	} 
      while (isdigit (*p));
      if (*p == '_')
	{
	  *type = p + 1;
	  *count = n;
	}
    }
  return 1;
}

/* result will be initialised here; it will be freed on failure */

static int
do_type (type, result)
     const char **type;
     string *result;
{
  int n;
  int done;
  int non_empty = 0;
  int success;
  string decl;
  const char *remembered_type;

  string_init (&decl);
  string_init (result);

  done = 0;
  success = 1;
  while (success && !done)
    {
      int member;
      switch (**type)
	{
	case 'Q':
	  n = (*type)[1] - '0';
	  if (n < 0 || n > 9)
	    success = 0;
	  *type += 2;
	  while (n-- > 0)
	    do_type (type, result);
	  break;

	case 'P':
	  *type += 1;
	  string_prepend (&decl, "*");
	  break;

	case 'R':
	  *type += 1;
	  string_prepend (&decl, "&");
	  break;

	case 'T':
	  *type += 1;
	  if (!get_count (type, &n) || n >= ntypes)
	    success = 0;
	  else
	    {
	      remembered_type = typevec[n];
	      type = &remembered_type;
	    }
	  break;

	case 'F':
	  *type += 1;
	  if (!string_empty (&decl) && decl.b[0] == '*')
	    {
	      string_prepend (&decl, "(");
	      string_append (&decl, ")");
	    }
	  if (!do_args (type, &decl) || **type != '_')
	    success = 0;
	  else
	    *type += 1;
	  break;

	case 'M':
	case 'O':
	  {
	    int constp = 0;
	    int volatilep = 0;

	    member = **type == 'M';
	    *type += 1;
	    if (!isdigit (**type))
	      {
		success = 0;
		break;
	      }
	    n = 0;
	    do
	      {
		n *= 10;
		n += **type - '0';
		*type += 1;
	      } 
	    while (isdigit (**type));
	    if (strlen (*type) < n)
	      {
		success = 0;
		break;
	      }
	    string_append (&decl, ")");
	    string_prepend (&decl, "::");
	    string_prependn (&decl, *type, n);
	    string_prepend (&decl, "(");
	    *type += n;
	    if (member)
	      {
		if (**type == 'C')
		  {
		    *type += 1;
		    constp = 1;
		  }
		if (**type == 'V')
		  {
		    *type += 1;
		    volatilep = 1;
		  }
		if (*(*type)++ != 'F')
		  {
		    success = 0;
		    break;
		  }
	      }
	    if ((member && !do_args (type, &decl)) || **type != '_')
	      {
		success = 0;
		break;
	      }
	    *type += 1;
	    if (constp)
	      {
		if (non_empty)
		  string_append (&decl, " ");
		else
		  non_empty = 1;
		string_append (&decl, "const");
	      }
	    if (volatilep)
	      {
		if (non_empty)
		  string_append (&decl, " ");
		else
		  non_empty = 1;
		string_append (&decl, "volatile");
	      }
	    break;
	  }

	case 'C':
	  if ((*type)[1] == 'P')
	    {
	      *type += 1;
	      if (!string_empty (&decl))
		string_prepend (&decl, " ");
	      string_prepend (&decl, "const");
	      break;
	    }

	  /* fall through */
	default:
	  done = 1;
	  break;
	}
    }

  non_empty = 0;
  if (success)
    success = do_cuv_prefix (type, result, &non_empty);

  if (success)
    success = do_builtin_type(type, result, &non_empty);
  
  if (success)
    {
      if (!string_empty (&decl))
	{
	  string_append (result, " ");
	  string_appends (result, &decl);
	}
      string_delete (&decl);
      return 1;
    }
  else
    {
      string_delete (&decl);
      string_delete (result);
      return 0;
    }
}

int
do_cuv_prefix (type, result, non_empty)
     char **type;
     string* result;
     int* non_empty;
{
  int success = 1;
  int done = 0;
  
  while (success && !done)
    {
      switch (**type)
	{
	case 'C':
	  *type += 1;
	  if (*non_empty)
	    string_append (result, " ");
	  else
	    *non_empty = 1;
	  string_append (result, "const");
	  break;
	case 'U':
	  *type += 1;
	  if (*non_empty)
	    string_append (result, " ");
	  else
	    *non_empty = 1;
	  string_append (result, "unsigned");
	  break;
	case 'V':
	  *type += 1;
	  if (*non_empty)
	    string_append (result, " ");
	  else
	    *non_empty = 1;
	  string_append (result, "volatile");
	  break;
	default:
	  done = 1;
	  break;
	}
    }
  return success;
}

int
do_builtin_type (type, result, non_empty)
     char **type;
     string* result;
     int *non_empty;
{
  int success = 1;
  int n;
  
  switch (**type)
    {
    case '\0':
    case '_':
      break;
    case 'v':
      *type += 1;
      if (*non_empty)
	string_append (result, " ");
      string_append (result, "void");
      break;
    case 'x':
      *type += 1;
      if (*non_empty)
	string_append (result, " ");
      string_append (result, "long long");
      break;
    case 'l':
      *type += 1;
      if (*non_empty)
	string_append (result, " ");
      string_append (result, "long");
      break;
    case 'i':
      *type += 1;
      if (*non_empty)
	string_append (result, " ");
      string_append (result, "int");
      break;
    case 's':
      *type += 1;
      if (*non_empty)
	string_append (result, " ");
      string_append (result, "short");
      break;
    case 'c':
      *type += 1;
      if (*non_empty)
	string_append (result, " ");
      string_append (result, "char");
      break;
    case 'w':
      *type += 1;
      if (*non_empty)
	string_append (result, " ");
      string_append (result, "wchar_t");
      break;
    case 'r':
      *type += 1;
      if (*non_empty)
	string_append (result, " ");
      string_append (result, "long double");
      break;
    case 'd':
      *type += 1;
      if (*non_empty)
	string_append (result, " ");
      string_append (result, "double");
      break;
    case 'f':
      *type += 1;
      if (*non_empty)
	string_append (result, " ");
      string_append (result, "float");
      break;
    case 'G':
      *type += 1;
      if (!isdigit (**type))
	{
	  success = 0;
	  break;
	}
      /* fall through */
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      n = 0;
      do
	{
	  n *= 10;
	  n += **type - '0';
	  *type += 1;
	}
      while (isdigit (**type));
      if (strlen (*type) < n)
	{
	  success = 0;
	  break;
	}
      if (*non_empty)
	string_append (result, " ");
      string_appendn (result, *type, n);
      *type += n;
      break;
    default:
      success = 0;
      break;
    }
  return success;
}

/* `result' will be initialised in do_type; it will be freed on failure */

static int
do_arg (type, result)
     const char **type;
     string *result;
{
  const char *start = *type;

  if (!do_type (type, result))
    return 0;
  remember_type (start, *type - start);
  return 1;
}

static void
remember_type (start, len)
     const char *start;
     int len;
{
  char *tem;

  if (ntypes >= typevec_size)
    {
      if (typevec_size == 0)
	{
	  typevec_size = 3;
	  typevec = (char **) xmalloc (sizeof (char*)*typevec_size);
	}
      else
	{
	  typevec_size *= 2;
	  typevec = (char **) xrealloc ((char *)typevec, sizeof (char*)*typevec_size);
	}
    }
  tem = (char *) xmalloc (len + 1);
  memcpy (tem, start, len);
  tem[len] = '\0';
  typevec[ntypes++] = tem;
}

/* `decl' must be already initialised, usually non-empty;
   it won't be freed on failure */

static int
do_args (type, decl)
     const char **type;
     string *decl;
{
  string arg;
  int need_comma = 0;

  string_append (decl, "(");

  while (**type != '_' && **type != '\0' && **type != 'e' && **type != 'v')
    {
      if (**type == 'N')
	{
	  int r;
	  int t;
	  *type += 1;
	  if (!get_count (type, &r) || !get_count (type, &t) || t >= ntypes)
	    return 0;
	  while (--r >= 0)
	    {
	      const char *tem = typevec[t];
	      if (need_comma)
		string_append (decl, ", ");
	      if (!do_arg (&tem, &arg))
		return 0;
	      string_appends (decl, &arg);
	      string_delete (&arg);
	      need_comma = 1;
	    }
	}
      else
	{
	  if (need_comma)
	    string_append (decl, ", ");
	  if (!do_arg (type, &arg))
	    return 0;
	  string_appends (decl, &arg);
	  string_delete (&arg);
	  need_comma = 1;
	}
    }

  if (**type == 'v')
    *type += 1;
  else if (**type == 'e')
    {
      *type += 1;
      if (need_comma)
	string_append (decl, ",");
      string_append (decl, "...");
    }

  string_append (decl, ")");
  return 1;
}

static void
munge_function_name (name)
     string *name;
{
  if (string_empty (name))
    return;

  if (name->p - name->b >= 3 
      && name->b[0] == 'o' && name->b[1] == 'p' && name->b[2] == CPLUS_MARKER)
    {
      int i;
      /* see if it's an assignment expression */
      if (name->p - name->b >= 10 /* op$assign_ */
	  && memcmp (name->b + 3, "assign_", 7) == 0)
	{
	  for (i = 0; i < sizeof (optable)/sizeof (optable[0]); i++)
	    {
	      int len = name->p - name->b - 10;
	      if (strlen (optable[i].in) == len
		  && memcmp (optable[i].in, name->b + 10, len) == 0)
		{
		  string_clear (name);
		  string_append (name, "operator");
		  string_append (name, optable[i].out);
		  string_append (name, "=");
		  return;
		}
	    }
	}
      else
	{
	  for (i = 0; i < sizeof (optable)/sizeof (optable[0]); i++)
	    {
	      int len = name->p - name->b - 3;
	      if (strlen (optable[i].in) == len 
		  && memcmp (optable[i].in, name->b + 3, len) == 0)
		{
		  string_clear (name);
		  string_append (name, "operator");
		  string_append (name, optable[i].out);
		  return;
		}
	    }
	}
      return;
    }
  else if (name->p - name->b >= 5 && memcmp (name->b, "type$", 5) == 0)
    {
      /* type conversion operator */
      string type;
      const char *tem = name->b + 5;
      if (do_type (&tem, &type))
	{
	  string_clear (name);
	  string_append (name, "operator ");
	  string_appends (name, &type);
	  string_delete (&type);
	  return;
	}
    }
  /* ANSI.  */
  else if (name->b[2] == 'o' && name->b[3] == 'p')
    {
      /* type conversion operator.  */
      string type;
      const char *tem = name->b + 4;
      if (do_type (&tem, &type))
	{
	  string_clear (name);
	  string_append (name, "operator ");
	  string_appends (name, &type);
	  string_delete (&type);
	  return;
	}
    }
  else if (name->b[2] >= 'a' && name->b[2] <= 'z'
	   && name->b[3] >= 'a' && name->b[3] <= 'z')
    {
      int i;

      if (name->b[4] == '\0')
	{
	  /* Operator.  */
	  for (i = 0; i < sizeof (optable)/sizeof (optable[0]); i++)
	    {
	      if (strlen (optable[i].in) == 2
		  && memcmp (optable[i].in, name->b + 2, 2) == 0)
		{
		  string_clear (name);
		  string_append (name, "operator");
		  string_append (name, optable[i].out);
		  return;
		}
	    }
	}
      else
	{
	  if (name->b[2] != 'a' || name->b[5] != '\0')
	    return;
	  /* Assignment.  */
	  for (i = 0; i < sizeof (optable)/sizeof (optable[0]); i++)
	    {
	      if (strlen (optable[i].in) == 3
		  && memcmp (optable[i].in, name->b + 2, 3) == 0)
		{
		  string_clear (name);
		  string_append (name, "operator");
		  string_append (name, optable[i].out);
		  return;
		}
	    }
	}
    }
}

/* a mini string-handling package */

static void
string_need (s, n)
     string *s;
     int n;
{
  if (s->b == NULL)
    {
      if (n < 32)
	n = 32;
      s->p = s->b = (char *) xmalloc (n);
      s->e = s->b + n;
    }
  else if (s->e - s->p < n)
    {
      int tem = s->p - s->b;
      n += tem;
      n *= 2;
      s->b = (char *) xrealloc (s->b, n);
      s->p = s->b + tem;
      s->e = s->b + n;
    }
}

static void
string_delete (s)
     string *s;
{
  if (s->b != NULL)
    {
      free (s->b);
      s->b = s->e = s->p = NULL;
    }
}

static void
string_init (s)
     string *s;
{
  s->b = s->p = s->e = NULL;
}

static void 
string_clear (s)
     string *s;
{
  s->p = s->b;
}

static int
string_empty (s)
     string *s;
{
  return s->b == s->p;
}

static void
string_append (p, s)
     string *p;
     const char *s;
{
  int n;
  if (s == NULL || *s == '\0')
    return;
  n = strlen (s);
  string_need (p, n);
  memcpy (p->p, s, n);
  p->p += n;
}

static void
string_appends (p, s)
     string *p, *s;
{
  int n;
  if (s->b == s->p)
    return;
  n = s->p - s->b;
  string_need (p, n);
  memcpy (p->p, s->b, n);
  p->p += n;
}

static void
string_appendn (p, s, n)
     string *p;
     const char *s;
     int n;
{
  if (n == 0)
    return;
  string_need (p, n);
  memcpy (p->p, s, n);
  p->p += n;
}

static void
string_prepend (p, s)
     string *p;
     const char *s;
{
  if (s == NULL || *s == '\0')
    return;
  string_prependn (p, s, strlen (s));
}

#if 0
static void
string_prepends (p, s)
     string *p, *s;
{
  if (s->b == s->p)
    return;
  string_prependn (p, s->b, s->p - s->b);
}
#endif

static void
string_prependn (p, s, n)
     string *p;
     const char *s;
     int n;
{
  char *q;

  if (n == 0)
    return;
  string_need (p, n);
  for (q = p->p - 1; q >= p->b; q--)
    q[n] = q[0];
  memcpy (p->b, s, n);
  p->p += n;
}
