/* Host name resolution and matching.
   Copyright (C) 1995, 1996, 1997, 2000, 2001 Free Software Foundation, Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

GNU Wget is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

In addition, as a special exception, the Free Software Foundation
gives permission to link the code of its release of Wget with the
OpenSSL project's "OpenSSL" library (or with modified versions of it
that use the same license as the "OpenSSL" library), and distribute
the linked executables.  You must obey the GNU General Public License
in all respects for all of the code used other than "OpenSSL".  If you
modify this file, you may extend this exception to your version of the
file, but you are not obligated to do so.  If you do not wish to do
so, delete this exception statement from your version.  */

#include <config.h>

#ifndef WINDOWS
#include <netdb.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif
#include <assert.h>
#include <sys/types.h>

#ifdef WINDOWS
# include <winsock.h>
# define SET_H_ERRNO(err) WSASetLastError (err)
#else
# include <sys/socket.h>
# include <netinet/in.h>
# ifndef __BEOS__
#  include <arpa/inet.h>
# endif
# include <netdb.h>
# define SET_H_ERRNO(err) ((void)(h_errno = (err)))
#endif /* WINDOWS */

#ifndef NO_ADDRESS
# define NO_ADDRESS NO_DATA
#endif

#include <errno.h>

#include "wget.h"
#include "utils.h"
#include "host.h"
#include "url.h"
#include "hash.h"

#ifndef errno
extern int errno;
#endif

#ifndef h_errno
# ifndef __CYGWIN__
extern int h_errno;
# endif
#endif

/* Mapping between known hosts and to lists of their addresses. */

static struct hash_table *host_name_addresses_map;

/* Lists of addresses.  This should eventually be extended to handle
   IPv6.  */

struct address_list {
  int count;			/* number of adrresses */
  ip_address *addresses;	/* pointer to the string of addresses */

  int faulty;			/* number of addresses known not to work. */
  int connected;		/* whether we were able to connect to
				   one of the addresses in the list,
				   at least once. */

  int refcount;			/* reference count; when it drops to
				   0, the entry is freed. */
};

/* Get the bounds of the address list.  */

void
address_list_get_bounds (const struct address_list *al, int *start, int *end)
{
  *start = al->faulty;
  *end   = al->count;
}

/* Return a pointer to the address at position POS.  */

const ip_address *
address_list_address_at (const struct address_list *al, int pos)
{
  assert (pos >= al->faulty && pos < al->count);
  return al->addresses + pos;
}

/* Return 1 if IP is one of the addresses in AL. */

int
address_list_find (const struct address_list *al, const ip_address *ip)
{
  int i;
  switch (ip->type)
    {
    case IPV4_ADDRESS:
      for (i = 0; i < al->count; i++)
	{
	  ip_address *cur = al->addresses + i;
	  if (cur->type == IPV4_ADDRESS
	      && (ADDRESS_IPV4_IN_ADDR (cur).s_addr
		  ==
		  ADDRESS_IPV4_IN_ADDR (ip).s_addr))
	    return 1;
	}
      return 0;
#ifdef ENABLE_IPV6
    case IPV6_ADDRESS:
      for (i = 0; i < al->count; i++)
	{
	  ip_address *cur = al->addresses + i;
	  if (cur->type == IPV6_ADDRESS
#ifdef HAVE_SOCKADDR_IN6_SCOPE_ID
	      && ADDRESS_IPV6_SCOPE (cur) == ADDRESS_IPV6_SCOPE (ip)
#endif
	      && IN6_ARE_ADDR_EQUAL (&ADDRESS_IPV6_IN6_ADDR (cur),
				     &ADDRESS_IPV6_IN6_ADDR (ip)))
	    return 1;
	}
      return 0;
#endif /* ENABLE_IPV6 */
    default:
      abort ();
      return 1;
    }
}

/* Mark the INDEXth element of AL as faulty, so that the next time
   this address list is used, the faulty element will be skipped.  */

void
address_list_set_faulty (struct address_list *al, int index)
{
  /* We assume that the address list is traversed in order, so that a
     "faulty" attempt is always preceded with all-faulty addresses,
     and this is how Wget uses it.  */
  assert (index == al->faulty);

  ++al->faulty;
  if (al->faulty >= al->count)
    /* All addresses have been proven faulty.  Since there's not much
       sense in returning the user an empty address list the next
       time, we'll rather make them all clean, so that they can be
       retried anew.  */
    al->faulty = 0;
}

/* Set the "connected" flag to true.  This flag used by connect.c to
   see if the host perhaps needs to be resolved again.  */

void
address_list_set_connected (struct address_list *al)
{
  al->connected = 1;
}

/* Return the value of the "connected" flag. */

int
address_list_connected_p (const struct address_list *al)
{
  return al->connected;
}

#ifdef ENABLE_IPV6

/* Create an address_list from the addresses in the given struct
   addrinfo.  */

static struct address_list *
address_list_from_addrinfo (const struct addrinfo *ai)
{
  struct address_list *al;
  const struct addrinfo *ptr;
  int cnt;
  ip_address *ip;

  cnt = 0;
  for (ptr = ai; ptr != NULL ; ptr = ptr->ai_next)
    if (ptr->ai_family == AF_INET || ptr->ai_family == AF_INET6)
      ++cnt;
  if (cnt == 0)
    return NULL;

  al = xnew0 (struct address_list);
  al->addresses  = xnew_array (ip_address, cnt);
  al->count      = cnt;
  al->refcount   = 1;

  ip = al->addresses;
  for (ptr = ai; ptr != NULL; ptr = ptr->ai_next)
    if (ptr->ai_family == AF_INET6) 
      {
	const struct sockaddr_in6 *sin6 =
	  (const struct sockaddr_in6 *)ptr->ai_addr;
	ip->type = IPV6_ADDRESS;
	ADDRESS_IPV6_IN6_ADDR (ip) = sin6->sin6_addr;
#ifdef HAVE_SOCKADDR_IN6_SCOPE_ID
	ADDRESS_IPV6_SCOPE (ip) = sin6->sin6_scope_id;
#endif
	++ip;
      } 
    else if (ptr->ai_family == AF_INET)
      {
	const struct sockaddr_in *sin =
	  (const struct sockaddr_in *)ptr->ai_addr;
	ip->type = IPV4_ADDRESS;
	ADDRESS_IPV4_IN_ADDR (ip) = sin->sin_addr;
	++ip;
      }
  assert (ip - al->addresses == cnt);
  return al;
}

#else  /* not ENABLE_IPV6 */

/* Create an address_list from a NULL-terminated vector of IPv4
   addresses.  This kind of vector is returned by gethostbyname.  */

static struct address_list *
address_list_from_ipv4_addresses (char **vec)
{
  int count, i;
  struct address_list *al = xnew0 (struct address_list);

  count = 0;
  while (vec[count])
    ++count;
  assert (count > 0);

  al->addresses  = xnew_array (ip_address, count);
  al->count      = count;
  al->refcount   = 1;

  for (i = 0; i < count; i++)
    {
      ip_address *ip = &al->addresses[i];
      ip->type = IPV4_ADDRESS;
      memcpy (ADDRESS_IPV4_DATA (ip), vec[i], 4);
    }

  return al;
}

#endif /* not ENABLE_IPV6 */

static void
address_list_delete (struct address_list *al)
{
  xfree (al->addresses);
  xfree (al);
}

/* Mark the address list as being no longer in use.  This will reduce
   its reference count which will cause the list to be freed when the
   count reaches 0.  */

void
address_list_release (struct address_list *al)
{
  --al->refcount;
  DEBUGP (("Releasing %p (new refcount %d).\n", al, al->refcount));
  if (al->refcount <= 0)
    {
      DEBUGP (("Deleting unused %p.\n", al));
      address_list_delete (al);
    }
}

/* Versions of gethostbyname and getaddrinfo that support timeout. */

#ifndef ENABLE_IPV6

struct ghbnwt_context {
  const char *host_name;
  struct hostent *hptr;
};

static void
gethostbyname_with_timeout_callback (void *arg)
{
  struct ghbnwt_context *ctx = (struct ghbnwt_context *)arg;
  ctx->hptr = gethostbyname (ctx->host_name);
}

/* Just like gethostbyname, except it times out after TIMEOUT seconds.
   In case of timeout, NULL is returned and errno is set to ETIMEDOUT.
   The function makes sure that when NULL is returned for reasons
   other than timeout, errno is reset.  */

static struct hostent *
gethostbyname_with_timeout (const char *host_name, double timeout)
{
  struct ghbnwt_context ctx;
  ctx.host_name = host_name;
  if (run_with_timeout (timeout, gethostbyname_with_timeout_callback, &ctx))
    {
      SET_H_ERRNO (HOST_NOT_FOUND);
      errno = ETIMEDOUT;
      return NULL;
    }
  if (!ctx.hptr)
    errno = 0;
  return ctx.hptr;
}

/* Print error messages for host errors.  */
static char *
host_errstr (int error)
{
  /* Can't use switch since some of these constants can be equal,
     which makes the compiler complain about duplicate case
     values.  */
  if (error == HOST_NOT_FOUND
      || error == NO_RECOVERY
      || error == NO_DATA
      || error == NO_ADDRESS)
    return _("Unknown host");
  else if (error == TRY_AGAIN)
    /* Message modeled after what gai_strerror returns in similar
       circumstances.  */
    return _("Temporary failure in name resolution");
  else
    return _("Unknown error");
}

#else  /* ENABLE_IPV6 */

struct gaiwt_context {
  const char *node;
  const char *service;
  const struct addrinfo *hints;
  struct addrinfo **res;
  int exit_code;
};

static void
getaddrinfo_with_timeout_callback (void *arg)
{
  struct gaiwt_context *ctx = (struct gaiwt_context *)arg;
  ctx->exit_code = getaddrinfo (ctx->node, ctx->service, ctx->hints, ctx->res);
}

/* Just like getaddrinfo, except it times out after TIMEOUT seconds.
   In case of timeout, the EAI_SYSTEM error code is returned and errno
   is set to ETIMEDOUT.  */

static int
getaddrinfo_with_timeout (const char *node, const char *service,
			  const struct addrinfo *hints, struct addrinfo **res,
			  double timeout)
{
  struct gaiwt_context ctx;
  ctx.node = node;
  ctx.service = service;
  ctx.hints = hints;
  ctx.res = res;

  if (run_with_timeout (timeout, getaddrinfo_with_timeout_callback, &ctx))
    {
      errno = ETIMEDOUT;
      return EAI_SYSTEM;
    }
  return ctx.exit_code;
}

#endif /* ENABLE_IPV6 */

/* Pretty-print ADDR.  When compiled without IPv6, this is the same as
   inet_ntoa.  With IPv6, it either prints an IPv6 address or an IPv4
   address.  */

const char *
pretty_print_address (const ip_address *addr)
{
  switch (addr->type) 
    {
    case IPV4_ADDRESS:
      return inet_ntoa (ADDRESS_IPV4_IN_ADDR (addr));
#ifdef ENABLE_IPV6
    case IPV6_ADDRESS:
      {
        static char buf[128];
	inet_ntop (AF_INET6, &ADDRESS_IPV6_IN6_ADDR (addr), buf, sizeof (buf));
#if 0
#ifdef HAVE_SOCKADDR_IN6_SCOPE_ID
	{
	  /* append "%SCOPE_ID" for all ?non-global? addresses */
	  char *p = buf + strlen (buf);
	  *p++ = '%';
	  number_to_string (p, ADDRESS_IPV6_SCOPE (addr));
	}
#endif
#endif
        buf[sizeof (buf) - 1] = '\0';
        return buf;
      }
#endif
    }
  abort ();
  return NULL;
}

/* Add host name HOST with the address ADDR_TEXT to the cache.
   ADDR_LIST is a NULL-terminated list of addresses, as in struct
   hostent.  */

static void
cache_host_lookup (const char *host, struct address_list *al)
{
  if (!host_name_addresses_map)
    host_name_addresses_map = make_nocase_string_hash_table (0);

  ++al->refcount;
  hash_table_put (host_name_addresses_map, xstrdup_lower (host), al);

#ifdef ENABLE_DEBUG
  if (opt.debug)
    {
      int i;
      debug_logprintf ("Caching %s =>", host);
      for (i = 0; i < al->count; i++)
	debug_logprintf (" %s", pretty_print_address (al->addresses + i));
      debug_logprintf ("\n");
    }
#endif
}

/* Remove HOST from Wget's DNS cache.  Does nothing is HOST is not in
   the cache.  */

void
forget_host_lookup (const char *host)
{
  struct address_list *al = hash_table_get (host_name_addresses_map, host);
  if (al)
    {
      address_list_release (al);
      hash_table_remove (host_name_addresses_map, host);
    }
}

/* Look up HOST in DNS and return a list of IP addresses.

   This function caches its result so that, if the same host is passed
   the second time, the addresses are returned without the DNS lookup.
   If you want to force lookup, call forget_host_lookup() prior to
   this function, or set opt.dns_cache to 0 to globally disable
   caching.

   If SILENT is non-zero, progress messages are not printed.  */

struct address_list *
lookup_host (const char *host, int silent)
{
  struct address_list *al = NULL;

#ifndef ENABLE_IPV6
  /* If we're not using getaddrinfo, first check if HOST names a
     numeric IPv4 address.  gethostbyname is not required to accept
     dotted-decimal IPv4 addresses, and some older implementations
     (e.g. the Ultrix one) indeed didn't.  */
  {
    uint32_t addr_ipv4 = (uint32_t)inet_addr (host);
    if (addr_ipv4 != (uint32_t) -1)
      {
	/* No need to cache host->addr relation, just return the
	   address.  */
	char *vec[2];
	vec[0] = (char *)&addr_ipv4;
	vec[1] = NULL;
	return address_list_from_ipv4_addresses (vec);
      }
  }
#endif

  /* Try to find the host in the cache. */

  if (host_name_addresses_map)
    {
      al = hash_table_get (host_name_addresses_map, host);
      if (al)
	{
	  DEBUGP (("Found %s in host_name_addresses_map (%p)\n", host, al));
	  ++al->refcount;
	  return al;
	}
    }

  /* No luck with the cache; resolve the host name. */

  if (!silent)
    logprintf (LOG_VERBOSE, _("Resolving %s... "), host);

#ifdef ENABLE_IPV6
  {
    int err;
    struct addrinfo hints, *res;

    xzero (hints);
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC; /* #### should look at opt.ipv4_only
				    and opt.ipv6_only */
    hints.ai_flags = 0;

    err = getaddrinfo_with_timeout (host, NULL, &hints, &res, opt.dns_timeout);
    if (err != 0 || res == NULL)
      {
	if (!silent)
	  logprintf (LOG_VERBOSE, _("failed: %s.\n"),
		     err != EAI_SYSTEM ? gai_strerror (err) : strerror (errno));
	return NULL;
      }
    al = address_list_from_addrinfo (res);
    freeaddrinfo (res);
    if (!al)
      {
	logprintf (LOG_VERBOSE, _("failed: No IPv4/IPv6 addresses.\n"));
	return NULL;
      }
  }
#else
  {
    struct hostent *hptr = gethostbyname_with_timeout (host, opt.dns_timeout);
    if (!hptr)
      {
	if (!silent)
	  {
	    if (errno != ETIMEDOUT)
	      logprintf (LOG_VERBOSE, _("failed: %s.\n"),
			 host_errstr (h_errno));
	    else
	      logputs (LOG_VERBOSE, _("failed: timed out.\n"));
	  }
	return NULL;
      }
    /* Do older systems have h_addr_list?  */
    al = address_list_from_ipv4_addresses (hptr->h_addr_list);
  }
#endif

  /* Print the addresses determined by DNS lookup, but no more than
     three.  */
  if (!silent)
    {
      int i;
      int printmax = al->count <= 3 ? al->count : 3;
      for (i = 0; i < printmax; i++)
	{
	  logprintf (LOG_VERBOSE, "%s",
		     pretty_print_address (al->addresses + i));
	  if (i < printmax - 1)
	    logputs (LOG_VERBOSE, ", ");
	}
      if (printmax != al->count)
	logputs (LOG_VERBOSE, ", ...");
      logputs (LOG_VERBOSE, "\n");
    }

  /* Cache the lookup information. */
  if (opt.dns_cache)
    cache_host_lookup (host, al);

  return al;
}

/* Resolve HOST to get an address for use with bind(2).  Do *not* use
   this for sockets to be used with connect(2).

   This is a function separate from lookup_host because the results it
   returns are different -- it uses the AI_PASSIVE flag to
   getaddrinfo.  Because of this distinction, it doesn't store the
   results in the cache.  It prints nothing and implements no timeouts
   because it should normally only be used with local addresses
   (typically "localhost" or numeric addresses of different local
   interfaces.)

   Without IPv6, this function just calls lookup_host.  */

struct address_list *
lookup_host_passive (const char *host)
{
#ifdef ENABLE_IPV6
  struct address_list *al = NULL;
  int err;
  struct addrinfo hints, *res;

  xzero (hints);
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;	/* #### should look at opt.ipv4_only
				   and opt.ipv6_only */
  hints.ai_flags = AI_PASSIVE;

  err = getaddrinfo (host, NULL, &hints, &res);
  if (err != 0 || res == NULL)
    return NULL;
  al = address_list_from_addrinfo (res);
  freeaddrinfo (res);
  return al;
#else
  return lookup_host (host, 1);
#endif
}

/* Determine whether a URL is acceptable to be followed, according to
   a list of domains to accept.  */
int
accept_domain (struct url *u)
{
  assert (u->host != NULL);
  if (opt.domains)
    {
      if (!sufmatch ((const char **)opt.domains, u->host))
	return 0;
    }
  if (opt.exclude_domains)
    {
      if (sufmatch ((const char **)opt.exclude_domains, u->host))
	return 0;
    }
  return 1;
}

/* Check whether WHAT is matched in LIST, each element of LIST being a
   pattern to match WHAT against, using backward matching (see
   match_backwards() in utils.c).

   If an element of LIST matched, 1 is returned, 0 otherwise.  */
int
sufmatch (const char **list, const char *what)
{
  int i, j, k, lw;

  lw = strlen (what);
  for (i = 0; list[i]; i++)
    {
      for (j = strlen (list[i]), k = lw; j >= 0 && k >= 0; j--, k--)
	if (TOLOWER (list[i][j]) != TOLOWER (what[k]))
	  break;
      /* The domain must be first to reach to beginning.  */
      if (j == -1)
	return 1;
    }
  return 0;
}

static int
host_cleanup_mapper (void *key, void *value, void *arg_ignored)
{
  struct address_list *al;

  xfree (key);			/* host */

  al = (struct address_list *)value;
  assert (al->refcount == 1);
  address_list_delete (al);

  return 0;
}

void
host_cleanup (void)
{
  if (host_name_addresses_map)
    {
      hash_table_map (host_name_addresses_map, host_cleanup_mapper, NULL);
      hash_table_destroy (host_name_addresses_map);
      host_name_addresses_map = NULL;
    }
}
