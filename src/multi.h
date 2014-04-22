/* Declarations for HTTP.
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Free Software
   Foundation, Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

GNU Wget is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget.  If not, see <http://www.gnu.org/licenses/>.

Additional permission under GNU GPL version 3 section 7

If you modify this program, or any covered work, by linking or
combining it with the OpenSSL project's OpenSSL library (or a
modified version of that library), containing parts covered by the
terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
grants you additional permission to convey the resulting work.
Corresponding Source for a non-source form of such a combination
shall include the source code for the parts of OpenSSL used as well
as that of the covered work.  */

#ifndef MULTI_H
#define MULTI_H

#include <semaphore.h>

#include "wget.h"

#include "iri.h"
#include "url.h"

#define MIN_CHUNK_SIZE 2048

struct s_thread_ctx
{
  pthread_t thread;
  int used;
  int terminated;
  int dt, url_err;
  char *redirected;
  char *referer;
  struct url *url_parsed;
  struct iri *i;
  struct range *range;
  char *file;
  char *url;
#ifdef ENABLE_THREADS
  sem_t *retr_sem;
#else
  /* Not used.  */
  void *retr_sem;
#endif
  uerr_t status;
};

void init_temp_files();

void name_temp_files();

void merge_temp_files(char *);

void delete_temp_files();

void clean_temp_files();

void init_ranges();

int fill_ranges_data(int, long long int, long int);

void clean_range_res_data();

void clean_ranges();

int spawn_thread (struct s_thread_ctx*, int, int);

int collect_thread (sem_t *, struct s_thread_ctx *);

#endif /* MULTI_H */
