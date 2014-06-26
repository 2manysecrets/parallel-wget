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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <metalink/metalink_parser.h>
#include <metalink/metalink_types.h>

#include "wget.h"

#include "log.h"
#include "md5.h"
#include "sha1.h"
#include "sha256.h"
#include "metalink.h"
#include "utils.h"

#define HASH_TYPES 3
/* Between MD5, SHA1 and SHA256, SHA256 has the greatest hash length, which is
   32. In the line below, 64 is written to have a more readable code. */
#define MAX_DIGEST_LENGTH 32

static char supported_hashes[HASH_TYPES][7] = {"sha256", "sha1", "md5"};
static int digest_sizes[HASH_TYPES] = {SHA256_DIGEST_SIZE, SHA1_DIGEST_SIZE, MD5_DIGEST_SIZE};
static int (*hash_function[HASH_TYPES]) (FILE *, void *) = {sha256_stream, sha1_stream, md5_stream};

mlink *
parse_metalink(char *input_file)
{
  int err;
  metalink_t *metalink;
  metalink_file_t **files;
  metalink_resource_t **resources;
  metalink_checksum_t **checksums;
  metalink_chunk_checksum_t *chunk_checksum;
  metalink_piece_hash_t **piece_hashes;
  mlink *mlink;
  err = metalink_parse_file (input_file, &metalink);
  if(err != 0 || !metalink)
    {
      logprintf (LOG_VERBOSE, "Libmetalink could not parse the metalink file.\n");
      return NULL;
    }
  else if(metalink->files == NULL) {
    logprintf (LOG_VERBOSE, "PARSE METALINK: Metalink doesn't have any file data.\n");
    metalink_delete(metalink);
    return NULL;
  }

  mlink = malloc (sizeof(mlink));
  mlink->identity = (metalink->identity ? xstrdup (metalink->identity) : NULL);
  mlink->tags = (metalink->tags ? xstrdup (metalink->tags) : NULL);
  mlink->files = NULL;
  mlink->num_of_files = 0;

  for (files = metalink->files; *files; ++files)
    {
      mlink_file *file;

      if (!(*files)->name)
        {
          /* File name is missing */
          logprintf (LOG_VERBOSE, "PARSE METALINK: Skipping file"
                     " due to missing name/path.\n");
          continue;
        }
      else if (!(*files)->resources)
        {
          /* URL is missing */
          logprintf (LOG_VERBOSE, "PARSE METALINK: Skipping file(%s)"
                     " due to missing resources.\n", (*files)->name);
          continue;
        }

      file = malloc(sizeof(mlink_file));
      ++(mlink->num_of_files);
      file -> next = (mlink->files);
      (mlink->files) = file;

      file->name = xstrdup ((*files)->name);
      file->size = (*files)->size;
      file->maxconnections = (*files)->maxconnections;
      file->version = ((*files)->version ? xstrdup ((*files)->version) : NULL);
      file->language = ((*files)->language ? xstrdup ((*files)->language) : NULL);
      file->os = ((*files)->os ? xstrdup ((*files)->os) : NULL);
      file->resources = NULL;
      file->checksums = NULL;
      file->chunk_checksum = NULL;
      file->num_of_res = file->num_of_checksums = 0;

      for (resources = (*files)->resources; *resources; ++resources)
        {
          mlink_resource *resource;

          if (!(*resources)->url)
            {
              logprintf (LOG_VERBOSE, "PARSE METALINK: Skipping resource"
                         " due to missing URL.\n");
              continue;
            }

          resource = malloc (sizeof(mlink_resource));
          ++(file->num_of_res);

          resource->url = xstrdup ((*resources)->url);
          resource->type = ((*resources)->type ? xstrdup ((*resources)->type) : NULL);
          resource->location = ((*resources)->location ? xstrdup ((*resources)->location) : NULL);
          resource->preference = (*resources)->preference;
          resource->maxconnections = (*resources)->maxconnections;

          resource->next = (file->resources);
          (file->resources) = resource;
        }

      for (checksums = (*files)->checksums; *checksums; ++checksums)
        {
          mlink_checksum *checksum = malloc (sizeof(mlink_checksum));

          if (!(*checksums)->type)
            {
              logprintf (LOG_VERBOSE, "PARSE METALINK: Skipping checksum"
                         " due to missing hash type.\n");
              continue;
            }
          else if (!(*checksums)->hash)
            {
              logprintf (LOG_VERBOSE, "PARSE METALINK: Skipping resource"
                         " due to missing hash value.\n");
              continue;
            }

          checksum->type = ((*checksums)->type ? xstrdup ((*checksums)->type) : NULL);
          checksum->hash = ((*checksums)->hash ? xstrdup ((*checksums)->hash) : NULL);

          checksum->next = (file->checksums);
          (file->checksums) = checksum;
        }

      if((chunk_checksum = (*files)->chunk_checksum))
        {
          mlink_chunk_checksum *chunk_sum;
          
          if(!chunk_checksum->type)
            logprintf (LOG_VERBOSE, "PARSE METALINK: Skipping chunk checksum"
                       " due to missing type information.\n");
          else
            {
              chunk_sum = malloc (sizeof(mlink_chunk_checksum));
              chunk_sum->length = chunk_checksum->length;
              chunk_sum->type = (chunk_checksum->type ? xstrdup (chunk_checksum->type) : NULL);
              for (piece_hashes = chunk_checksum->piece_hashes; *piece_hashes; ++piece_hashes)
                {
                  mlink_piece_hash piece_hash;
                  if(!chunk_checksum->type)
                    {
                      logprintf (LOG_VERBOSE, "PARSE METALINK: Skipping chunk checksum"
                                 " due to missing hash value for piece(%d).\n",
                                 (*piece_hashes)->piece);
                      free (chunk_sum);
                      break;
                    }
                }
            }
        }
    }

  metalink_delete(metalink);
  return mlink;
}

void
elect_resources (mlink *mlink)
{
  mlink_file *file = mlink -> files;
  mlink_resource *res, *prev;

  for (; file; file = file->next)
    {
      prev = file->resources;
      res = prev->next;
      while (res)
        {
          if(strcmp(res->type, "ftp") || strcmp(res->type, "http"))
            {
              prev->next = res->next;
              free (res);
            }
          else
            {
              prev = prev->next;
              res = prev->next;
            }
        }
      res = file->resources;
      if(strcmp(res->type, "ftp") || strcmp(res->type, "http"))
        {
          file->resources = res->next;
          free(res);
        }
    }
}

void
elect_checksums (mlink *mlink)
{
  int i;
  mlink_file *file = mlink -> files;
  mlink_checksum *csum, *prev;

  for (; file; file = file->next)
    {
      prev = file->checksums;
      csum = prev->next;
      while (csum)
        {
          for(i=0; i<HASH_TYPES && strcmp(csum->type, supported_hashes[i]); ++i);
          if (i < HASH_TYPES)
            {
              prev->next = csum->next;
              free (csum);
            }
          else
            {
              prev = prev->next;
              csum = prev->next;
            }
        }
      csum = file->checksums;
      for(i=0; i<HASH_TYPES && strcmp(csum->type, supported_hashes[i]); ++i);
      if (i < HASH_TYPES)
        {
          prev->next = csum->next;
          free (csum);
        }
    }
}

void
delete_mlink(mlink *metalink)
{
  mlink_file *file, *tempfile;
  mlink_resource *res, *tempres;
  mlink_checksum *csum, *tempcsum;
  mlink_piece_hash *phash, *temphash;

  if(!metalink)
    return;

  xfree_null (metalink->tags);
  xfree_null (metalink->identity);

  file = metalink->files;
  while (file)
    {
      xfree_null(file->os);
      xfree_null(file->language);
      xfree_null(file->version);
      xfree_null(file->name);

      res = file->resources;
      while (res)
        {
          xfree_null (res->url);
          xfree_null (res->type);
          xfree_null (res->location);

          tempres = res;
          res = res->next;
          free (tempres);
        }

      csum = file->checksums;
      while (csum)
        {
          xfree_null (csum->type);
          xfree_null (csum->hash);

          tempcsum = csum;
          csum = csum->next;
          free (tempcsum);
        }

      if(file->chunk_checksum)
        {
          free (file->chunk_checksum->type);
          phash = file->chunk_checksum->piece_hashes;
          while (phash)
            {
              xfree_null (phash->hash);

              temphash = phash;
              phash = phash->next;
              free (temphash);
            }
        }

      tempfile = file;
      file = file->next;
      free (tempfile);
    }
  free (metalink);
}

/* Parses metalink into type metalink_t and returns a pointer to it.
   Returns NULL if the parsing is failed. */
metalink_t*
metalink_context (const char *url)
{
  metalink_error_t err;
  metalink_t* metalink;

  err = metalink_parse_memory (url, strlen(url), &metalink);

  if(err != 0)
      metalink = NULL;
  return metalink;
}

/* It should be taken into account that file hashes in metalink files may
   include uppercase letter. This function turns the case of the first length
   letters in the space pointed by hash into lowercase. */
static void
lower_hex_case (unsigned char *hash, int length)
{
  int i;

  /* 32 is the difference between the ascii codes of 'a' and 'A'. */
  for(i = 0; i < length; ++i)
    if('A' <= hash[i] && hash[i] <= 'Z')
      hash[i] += 32;
}

/* Verifies file hash by comparing the file hashes found by gnulib functions
   and hashes provided by metalink file. Works by comparing strongest supported
   hash type available in the metalink file.
   
   Returns;
   -1      if hashes that were compared turned out to be different.
    0      if all pairs of hashes compared turned out to be the same.
    1      if due to some error, comparisons could not be made.  */
int
verify_file_hash (const char *filename, mlink_checksum *checksums)
{
  int i, j, req_type, res = 0;

  unsigned char hash_raw[MAX_DIGEST_LENGTH];
  /* Points to a hash of supported type from the metalink file. The index dedicated
     to a type is inversely proportional to its strength. (check supported_types
     to see the supported hash types listed in decreasing order of strength)*/
  unsigned char *metalink_hashes[HASH_TYPES];
  unsigned char file_hash[2 * MAX_DIGEST_LENGTH + 1];
  FILE *file;
  mlink_checksum *checksum;

  if (!checksums)
    {
      /* Metalink file has no hashes for this file. */
      logprintf (LOG_VERBOSE, "Validating(%s) failed: digest missing in metalink file.\n",
                 filename);
      return 1;
    }

  for (i = 0; i < HASH_TYPES; ++i)
    metalink_hashes[i] = NULL;

  /* Fill metalink_hashes to contain an instance of supported types of hashes. */
  for (checksum = checksums; checksum; checksum = checksum->next)
    for (j = 0; j < HASH_TYPES; ++j)
      if (!strcmp(checksum->type, supported_hashes[j]))
        {
          if(metalink_hashes[j])
            {
              /* As of libmetalin-0.03, it is not checked during parsing the
                 information in the metalink file whether there are multiple
                 hashes of same type for one file. That case should be checked,
                 as none of those hashes can be trusted above the other. */
              logprintf (LOG_VERBOSE, "Validating(%s) failed: metalink file contains different hashes of same type.\n",
                         filename);
              return 1;
            }
          else
            metalink_hashes[j] = checksum->hash;
        }

  for (i = 0; !metalink_hashes[i]; ++i);

  if (i == HASH_TYPES)
    {
      /* no hash of supported types could be found. */
      logprintf (LOG_VERBOSE, "Validating(%s) failed: No hash of supported types could be found in metalink file.\n",
                 filename);
      return 1;
    }
  req_type = i;

  if (!(file = fopen(filename, "r")))
    {
      /* File could not be opened. */
      logprintf (LOG_VERBOSE, "Validating(%s) failed: file could not be opened.\n",
                 filename);
      return 1;
    }

  res = (*hash_function[req_type]) (file, hash_raw);
  fclose(file);

  /* Find file hash accordingly. */
  if (res)
    {
      logprintf (LOG_VERBOSE, "Validating(%s) failed: File hash could not be found.\n",
                 filename);
      return 1;
    }

  /* Turn byte-form hash to hex form. */
  for(j = 0 ; j < digest_sizes[req_type]; ++j)
    sprintf(file_hash + 2 * j, "%02x", hash_raw[j]);

  lower_hex_case(metalink_hashes[req_type], 2 * digest_sizes[req_type]);
  if (strcmp(metalink_hashes[req_type], file_hash))
    {
      logprintf (LOG_VERBOSE, "Verifying(%s) failed: %s hashes are different.\n",
                 filename, supported_hashes[i]);
      return -1;
    }

  logprintf (LOG_VERBOSE, "Verifying(%s): %s hashes are the same.\n",
             filename, supported_hashes[i]);
  return 0;
}
