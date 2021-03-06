#!/usr/bin/env python3
from sys import exit
from WgetTest import HTTPTest, WgetFile

"""
    This test ensures that Wget handles Content-Disposition correctly when
    coupled with Authentication
"""
TEST_NAME = "Authentication with Content Disposition"
############# File Definitions ###############################################
File1 = "Need a cookie?"

File1_rules = {
    "Authentication"    : {
        "Type"          : "Basic",
        "User"          : "Pacman",
        "Pass"          : "Omnomnom"
    },
    "SendHeader"        : {
        "Content-Disposition" : "Attachment; filename=Arch"
    }
}
A_File = WgetFile ("File1", File1, rules=File1_rules)

WGET_OPTIONS = "-d --user=Pacman --password=Omnomnom --content-disposition"
WGET_URLS = [["File1"]]

Files = [[A_File]]

ExpectedReturnCode = 0
ExpectedDownloadedFiles = [WgetFile ("Arch", File1)]

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
