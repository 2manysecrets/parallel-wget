#!/usr/bin/env python3
from sys import exit
from WgetTest import HTTPTest, WgetFile

"""
    This test ensures that Wget returns the correct return code when sent
    a 403 Forbidden by the Server.
"""

TEST_NAME = "Forbidden Retcode"

############# File Definitions ###############################################
File1 = "Apples and Oranges? Really?"

File1_rules = {
    "Response"          : 403
}

A_File = WgetFile ("File1", File1, rules=File1_rules)

WGET_OPTIONS = "-d"
WGET_URLS = [["File1"]]

Files = [[A_File]]

ExpectedReturnCode = 8
ExpectedDownloadedFiles = []

################ Pre and Post Test Hooks #####################################
pre_test = {
    "ServerFiles"       : Files
}
test_options = {
    "WgetCommands"      : WGET_OPTIONS,
    "Urls"              : WGET_URLS
}
post_test = {
    "ExpectedFiles"     : ExpectedDownloadedFiles,
    "ExpectedRetcode"   : ExpectedReturnCode
}

err = HTTPTest (
                name=TEST_NAME,
                pre_hook=pre_test,
                test_params=test_options,
                post_hook=post_test
).begin ()

exit (err)
