/*
 * $Id$
 *
 * This is a helper for the external ACL interface for Squid Cache
 * Copyright (C) 2002 Rodrigo Albani de Campos (rodrigo@geekbunker.org)
 * 
 * It reads STDIN looking for a username that matches a specified group
 * Returns `OK' if the user belongs to the group or `ERR' otherwise, as 
 * described on http://devel.squid-cache.org/external_acl/config.html
 * To compile this program, use:
 *
 * gcc -o check_group check_group-1.0.c
 *
 * Author: Rodrigo Albani de Campos
 * E-Mail: rodrigo@geekbunker.org
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *
 * Change Log:
 *
 * Revision 1.4  2002/04/17 01:58:48  camposr
 * minor corrections in the getopt
 *
 * Revision 1.3  2002/04/17 01:43:17  camposr
 * ready for action
 *
 * Revision 1.2  2002/04/17 01:32:16  camposr
 * all main routines ready
 *
 * Revision 1.1  2002/04/16 05:02:32  camposr
 * Initial revision
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <grp.h>
#include <unistd.h>
#include <pwd.h>


#define BUFSIZE 8192		/* the stdin buffer size */
#define MAX_GROUP 10		/* maximum number of groups
				   specified on the command line */

static int
validate_user_pw (char *username, char *groupname)
{
  /* 
     Verify if user�s primary group matches groupname
     Returns 0 if user is not on the group
     Returns 1 otherwise 
   */
  struct passwd *p;
  struct group *g;

  if ((p = getpwnam (username)) == NULL)
    {
      /*
         Returns an error if user does not exist
         in the /etc/passwd
       */
      fprintf (stderr, "helper: User does not exist '%s'\n", username);
      return 0;
    }
  else
    {
      /* Verify if the this is the primary user group */
      if ((g = getgrgid (p->pw_gid)) != NULL)
	{
	  if ((strcmp (groupname, g->gr_name)) == 0)
	    return 1;
	}
    }



  return 0;
}

static int
validate_user_gr (char *username, char *groupname)
{
  /*
     Verify if the user belongs to groupname as listed
     in the /etc/group file
   */
  struct group *g;

  if ((g = getgrnam (groupname)) == NULL)
    {
      fprintf (stderr, "helper: Group does not exist '%s'\n", groupname);
      return 0;
    }
  else
    {
      while (*(g->gr_mem) != NULL)
	{
	  if (strcmp (*((g->gr_mem)++), username) == 0)
	    {
	      return 1;
	    }
	}
    }
  return 0;
}

static void
usage (char *program)
{
  fprintf (stderr, "Usage: %s -g group1 [-g group2 ...] [-p]\n\n", program);
  fprintf (stderr, "-g group\n");
  fprintf (stderr,
	   "			The group name or id that the user must belong in order to\n");
  fprintf (stderr, "			be allowed to authenticate.\n");
  fprintf (stderr, "-p			Verify primary user group as well\n");
}


int
main (int argc, char *argv[])
{
  char *user, *p;
  char buf[BUFSIZE];
  char *grents[MAX_GROUP];
  int check_pw = 0, ch, i = 0, j = 0;


  /* make standard output line buffered */
  setvbuf (stdout, NULL, _IOLBF, 0);

  /* get user options */
  while ((ch = getopt (argc, argv, "pg:")) != -1)
    {
      switch (ch)
	{
	case 'p':
	  check_pw = 1;
	  break;
	case 'g':
	  grents[i] = calloc (strlen (optarg) + 1, sizeof (char));
	  strcpy (grents[i], optarg);
	  if (i < MAX_GROUP)
	    {
	      i++;
	    }
	  else
	    {
	      fprintf (stderr,
		       "Exceeded maximum number of allowed groups (%i)\n", i);
	      exit (1);
	    }
	  break;
	case '?':
	  if (isprint (optopt))
	    {

	      fprintf (stderr, "Unknown option '-%c'.\n", optopt);
	    }
	  else
	    {
	      fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
	    }

	default:
	  usage (argv[0]);
	  exit (1);
	}
    }
  if (optind < argc)
    {
      fprintf (stderr, "Unknown option '%s'\n", argv[optind]);
      usage (argv[0]);
      exit (1);
    }
  if (i == 0)
    fprintf (stderr,
	     "helper: Warning. No groups specified, authentication will fail for all users.\n");

  while (fgets (buf, BUFSIZE, stdin))
    {
      user = buf;
      j = 0;
      if ((p = strchr (buf, '\n')) != NULL)
	{
	  *p = '\0';
	}
      for (i = 0; grents[i] != NULL; i++)
	{
	  if (check_pw == 1)
	    {
	      j += validate_user_pw (user, grents[i]);
	    }
	  j += validate_user_gr (user, grents[i]);
	}

      if (j > 0)
	{
	  printf ("OK\n");
	}
      else
	{
	  printf ("ERR\n");
	}




    }
  return 0;
}
