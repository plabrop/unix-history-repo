/* Give this program DOCSTR.mm.nn as standard input
   and it outputs to standard output
   a file of nroff output containing the doc strings.  */

# include <stdio.h>
main()
{
  register int ch;
  register int notfirst = 0;

  printf (".TL\n");
  printf ("Command Summary for GNU Emacs\n");
  printf (".AU\nRichard M. Stallman\n");
  while ((ch = getchar()) != EOF)
    {
      if (ch == '\037')
	{
	  if (notfirst)
	    printf("\n.DE");
	  else
	    notfirst = 1;

	  printf("\n.SH\n");

	  while ((ch = getchar()) != '\n')  /* Changed this line */
	    {
	      if (ch != EOF)
		  putchar(ch);
	      else
		{
		  ungetc(ch);
		  break;
		}
	    }
	  printf("\n.DS L\n");
	}
      else
	putchar(ch);
    }
  exit(0);
}
