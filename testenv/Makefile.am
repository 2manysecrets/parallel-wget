# Makefile for `wget' utility
# Copyright (C) 2013 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with Wget.  If not, see <http://www.gnu.org/licenses/>.

# Additional permission under GNU GPL version 3 section 7

# If you modify this program, or any covered work, by linking or
# combining it with the OpenSSL project's OpenSSL library (or a
# modified version of that library), containing parts covered by the
# terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
# grants you additional permission to convey the resulting work.
# Corresponding Source for a non-source form of such a combination
# shall include the source code for the parts of OpenSSL used as well
# as that of the covered work.


AUTOMAKE_OPTIONS = parallel-tests
AM_TESTS_ENVIRONMENT = MAKE_CHECK=True; export MAKE_CHECK;

if RACE_CHECKING_IS_ENABLED
RACE_FAIL = Test--spider-r.py
RACE_TESTS =
endif

TESTS = Test-auth-basic-fail.py             \
    Test-auth-basic.py                      \
    Test-auth-both.py                       \
    Test-auth-digest.py                     \
    Test-auth-no-challenge.py               \
    Test-auth-no-challenge-url.py           \
    Test-auth-retcode.py                    \
    Test-auth-with-content-disposition.py   \
    Test-c-full.py                          \
    Test-Content-disposition-2.py           \
    Test-Content-disposition.py             \
    Test-cookie-401.py                      \
    Test-cookie-domain-mismatch.py          \
    Test-cookie-expires.py                  \
    Test-cookie.py                          \
    $(RACE_TESTS)                           \
    Test-Head.py                            \
    Test--https.py							\
    Test-O.py                               \
    Test-Post.py                            \
    Test--spider-r.py

XFAIL_TESTS = Test-auth-both.py             \
    $(RACE_FAIL)

LOG_COMPILER = python3

EXTRA_DIST = ColourTerm.py          \
    FTPServer.py                \
    HTTPServer.py               \
    README                  \
    Test--spider-r.py           \
    Test--https.py				\
    Test-Content-disposition-2.py       \
    Test-Content-disposition.py     \
    Test-Head.py                \
    Test-O.py               \
    Test-Parallel-Proto.py          \
    Test-Post.py                \
    Test-Proto.py               \
    Test-auth-basic-fail.py         \
    Test-auth-basic.py          \
    Test-auth-both.py           \
    Test-auth-digest.py         \
    Test-auth-no-challenge-url.py       \
    Test-auth-no-challenge.py       \
    Test-auth-retcode.py            \
    Test-auth-with-content-disposition.py   \
    Test-c-full.py              \
    Test-cookie-401.py          \
    Test-cookie-domain-mismatch.py      \
    Test-cookie-expires.py          \
    Test-cookie.py              \
    WgetTest.py
