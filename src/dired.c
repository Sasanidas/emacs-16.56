/* Lisp functions for making directory listings.
   Copyright (C) 1985 Richard M. Stallman.

This file is part of GNU Emacs.

GNU Emacs is distributed in the hope that it will be useful,
but without any warranty.  No author or distributor
accepts responsibility to anyone for the consequences of using it
or for whether it serves any particular purpose or works at all,
unless he says so in writing.

Everyone is granted permission to copy, modify and redistribute
GNU Emacs, but only under the conditions described in the
document "GNU Emacs copying permission notice".   An exact copy
of the document is supposed to have been given to you along with
GNU Emacs so that you can know how you may redistribute it all.
It should be in a file named COPYING.  Among other things, the
copyright notice and this notice must be preserved on all copies.  */


#include "config.h"
#include <sys/types.h>
#ifndef USG
#include <sys/time.h>
#endif
#include <sys/stat.h>
#include <sys/dir.h>
#include <stdio.h>
#undef NULL
#include "lisp.h"
#include "buffer.h"
#include "commands.h"

/* This entire file is not handled for system V yet.  */

#ifndef USG

#define min(a, b) ((a) < (b) ? (a) : (b))

extern Lisp_Object concat2 ();

Lisp_Object Vcompletion_ignored_extensions;

DEFUN ("directory-files", Fdirectory_files, Sdirectory_files, 1, 2, 0,
  "Return a list of names of files in DIRECTORY.\n\
If FULL is non-NIL, absolute pathnames of the files are returned.")
  (dirname, full)
     Lisp_Object dirname, full;
{
  DIR *d;
  struct direct *dp;
  char slashfilename[MAXNAMLEN+2];
  char *filename = slashfilename;
  int length;
  Lisp_Object list, name;

  dirname = Fexpand_file_name (dirname, Qnil);
  if (!(d = opendir (XSTRING (dirname)->data)))
    report_file_error ("Opening directory", Fcons (dirname, Qnil));

  list = Qnil;
  length = XSTRING (dirname)->size;
  if (length == 0   ||  XSTRING (dirname)->data[length - 1] != '/')
    *filename++ = '/';

  /* Loop reading blocks */
  while (1)
    {
      dp = readdir (d);
      if (!dp) break;
      if (dp->d_ino)
	{
	  int len = strlen(dp->d_name);
	  strncpy (filename, dp->d_name, len);
	  filename[len] = 0;
	  if (!NULL (full))
            name = concat2 (dirname, build_string (slashfilename));
	  else
	    name = build_string (filename);
	  list = Fcons (name, list);
	}
    }
  closedir (d);
  return Fsort (Fnreverse (list), Qstring_lessp);
}

Lisp_Object file_name_completion ();

DEFUN ("file-name-completion", Ffile_name_completion, Sfile_name_completion,
  2, 2, 0,
  "Complete file name FILE in directory DIR.\n\
Returns the longest string common to all filenames in DIR\n\
that start with FILE.\n\
If there is only one and FILE matches it exactly, returns t.\n\
Returns nil if DIR contains no name starting with FILE.")
  (file, dirname)
     Lisp_Object file, dirname;
{
  return file_name_completion (file, dirname, 0);
}

DEFUN ("file-name-all-completions", Ffile_name_all_completions,
  Sfile_name_all_completions, 2, 2, 0,
  "Return a list of all completions of file name FILE in directory DIR.")
  (file, dirname)
     Lisp_Object file, dirname;
{
  return file_name_completion (file, dirname, 1);
}

Lisp_Object
file_name_completion (file, dirname, all_flag)
     Lisp_Object file, dirname;
     int all_flag;
{
  DIR *d;
  struct direct *dp;
  int bestmatchsize, skip;
  register int compare, matchsize;
  unsigned char *p1, *p2;
  int matchcount = 0;
  Lisp_Object bestmatch, tem, elt, name;
  struct stat st;
  int directoryp;
  int passcount;

  dirname = Fexpand_file_name (dirname, Qnil);
  bestmatch = Qnil;

  /* passcount = 0, ignore files that end in an ignored extension.
     If nothing found then try again with passcount = 1, don't ignore them */
  for (passcount = 0; NULL (bestmatch) && passcount < 2; passcount++)
    {
      if (!(d = opendir (XSTRING (dirname)->data)))
	report_file_error ("Opening directory", Fcons (dirname, Qnil));

      /* Loop reading blocks */
      while (dp = readdir (d))
	{
	  if (!NULL (Vquit_flag) && NULL (Vinhibit_quit))
	    goto quit;
	  if (!dp->d_ino ||
	      strlen(dp->d_name) < XSTRING (file)->size ||
	      bcmp (dp->d_name, XSTRING (file)->data, XSTRING (file)->size))
	    continue;

	  if (!passcount)
	    /* Compare each extension-to-be-ignored against end of this file name */
	    /* and exit this for loop if a match is found */
	    for (tem = Vcompletion_ignored_extensions;
		 LISTP (tem); tem = XCONS (tem)->cdr)
	      {
		elt = XCONS (tem)->car;
		if (XTYPE (elt) != Lisp_String) continue;
		skip = strlen(dp->d_name) - XSTRING (elt)->size;
		if (skip < 0) continue;

		if (bcmp (dp->d_name + skip,
			  XSTRING (elt)->data,
			  XSTRING (elt)->size))
		  continue;
		break;
	      }

	  /* Unless a match was found, process this name as a completion */
	  if ((passcount || NULL (tem))
	      && file_name_completion_stat (dirname, dp, &st) >= 0)
	    {
	      /* Update computation of how much all possible completions match */

	      matchcount++;
	      directoryp = ((st.st_mode & S_IFMT) == S_IFDIR);
	      if (all_flag || NULL (bestmatch))
		{
		  /* This is a possible completion */
		  if (directoryp)
		    {
		      /* This completion is a directory; make it end with '/' */
		      name = make_string (dp->d_name, strlen(dp->d_name) + 1);
		      XSTRING (name)->data[strlen(dp->d_name)] = '/';
		    }
		  else
		    name = make_string (dp->d_name, strlen(dp->d_name));
		  if (all_flag)
		    {
		      bestmatch = Fcons (name, bestmatch);
		    }
		  else
		    {
		      bestmatch = name;
		      bestmatchsize = XSTRING (name)->size;
		    }
		}
	      else
		{
		  compare = min (bestmatchsize, strlen(dp->d_name));
		  p1 = XSTRING (bestmatch)->data;
		  p2 = (unsigned char *) dp->d_name;
		  for (matchsize = 0; matchsize < compare; matchsize++)
		    if (p1[matchsize] != p2[matchsize]) break;
		  if (directoryp  &&
		      (bestmatchsize > matchsize) &&
		      (p1[matchsize + 1] == '/'))
		    matchsize++;
		  bestmatchsize = min (matchsize, bestmatchsize);
		}
	    }
	}
      closedir (d);
    }

  if (all_flag || NULL (bestmatch))
    return bestmatch;
  if (matchcount == 1 && bestmatchsize == XSTRING (file)->size)
    return Qt;
  return Fsubstring (bestmatch, make_number (0), make_number (bestmatchsize));
 quit:
  if (d) closedir (d);
  Vquit_flag = Qnil;
  Fsignal (Qquit, Qnil);
}

file_name_completion_stat (dirname, dp, st_addr)
     Lisp_Object dirname;
     struct direct *dp;
     struct stat *st_addr;
{
  char *fullname = (char *) alloca (strlen(dp->d_name) + XSTRING (dirname)->size + 2);
  int pos = XSTRING (dirname)->size;

  bcopy (XSTRING (dirname)->data, fullname, pos);
  if (fullname[pos - 1] != '/')
    fullname[pos++] = '/';

  bcopy (dp->d_name, fullname + pos, strlen(dp->d_name));
  fullname[pos + strlen(dp->d_name)] = 0;

  return stat (fullname, st_addr);
}

Lisp_Object
make_time (time)
     int time;
{
  return Fcons (make_number (time >> 16),
		Fcons (make_number (time & 0177777), Qnil));
}

DEFUN ("file-attributes", Ffile_attributes, Sfile_attributes, 1, 1, 0,
  "Return a list of attributes of file FILENAME.\n\
Elements are:\n\
 0. t for directory, string (name linked to) for symbolic link, or nil.\n\
 1. Number of links to file.\n\
 2. File uid.\n\
 3. File gid.\n\
 4. Last access time, as a list of two integers.\n\
  First integer has high-order 16 bits of time, second has low 16 bits.\n\
 5. Last modification time, likewise.\n\
 6. Creation time, likewise.\n\
 7. Size in bytes.\n\
 8. File modes, as a string of nine letters or dashes as in ls -l.")
  (filename)
     Lisp_Object filename;
{
  Lisp_Object values[9];
  struct stat s;
  char modes[10];

  filename = Fexpand_file_name (filename, Qnil);
  if (lstat(XSTRING (filename)->data, &s)<0)
    return Qnil;

  values[0] = ((s.st_mode & S_IFMT)==S_IFDIR) ? Qt : Qnil;
  if ((s.st_mode & S_IFMT) == S_IFLNK)
    values[0] = Ffile_symlink_p (filename);
  XFASTINT (values[1]) = s.st_nlink;
  XFASTINT (values[2]) = s.st_uid;
  XFASTINT (values[3]) = s.st_gid;
  values[4] = make_time(s.st_atime);
  values[5] = make_time(s.st_mtime);
  values[6] = make_time(s.st_ctime);
  XFASTINT (values[7]) = s.st_size;
  filemodestring (&s, modes);
  values[8] = make_string (modes, 10);
  return Flist (9, values);
}

syms_of_dired ()
{
  defsubr (&Sdirectory_files);
  defsubr (&Sfile_name_completion);
  defsubr (&Sfile_name_all_completions);
  defsubr (&Sfile_attributes);

  DefLispVar ("completion-ignored-extensions", &Vcompletion_ignored_extensions,
    "*Completion ignores filenames ending in any string in this list.");
  Vcompletion_ignored_extensions = Qnil;
}

#endif /* ndef USG */
