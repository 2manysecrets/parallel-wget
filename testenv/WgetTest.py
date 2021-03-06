import os
import shutil
import shlex
import sys
import traceback
import HTTPServer
import re
import time
from subprocess import call
from ColourTerm import printer
from difflib import unified_diff

HTTP = "HTTP"
HTTPS = "HTTPS"

""" A Custom Exception raised by the Test Environment. """

class TestFailed (Exception):

    def __init__ (self, error):
        self.error = error


""" Class that defines methods common to both HTTP and FTP Tests. """

class CommonMethods:
    TestFailed = TestFailed

    def init_test_env (self, name):
        testDir = name + "-test"
        try:
            os.mkdir (testDir)
        except FileExistsError:
            shutil.rmtree (testDir)
            os.mkdir (testDir)
        os.chdir (testDir)
        self.tests_passed = True

    def get_domain_addr (self, addr):
        self.port = str (addr[1])
        return addr[0] + ":" + str(addr[1]) + "/"

    def exec_wget (self, options, urls, domain_list):
        cmd_line = self.get_cmd_line (options, urls, domain_list)
        params = shlex.split (cmd_line)
        print (params)
        if os.getenv ("SERVER_WAIT"):
            time.sleep (float (os.getenv ("SERVER_WAIT")))
        try:
            retcode = call (params)
        except FileNotFoundError as filenotfound:
            raise TestFailed (
                "The Wget Executable does not exist at the expected path")
        return retcode

    def get_cmd_line (self, options, urls, domain_list):
        TEST_PATH = os.path.abspath (".")
        WGET_PATH = os.path.join (TEST_PATH, "..", "..", "src", "wget")
        WGET_PATH = os.path.abspath (WGET_PATH)
        cmd_line = WGET_PATH + " " + options + " "
        for i in range (0, self.servers):
            for url in urls[i]:
                protocol = "http://" if self.server_types[i] is "HTTP" else "https://"
                cmd_line += protocol + domain_list[i] + url + " "
#        for url in urls:
#            cmd_line += domain_list[0] + url + " "
        print (cmd_line)
        return cmd_line

    def __test_cleanup (self):
        testDir = self.name + "-test"
        os.chdir ('..')
        try:
            if os.getenv ("NO_CLEANUP") is None:
                shutil.rmtree (testDir)
        except Exception as ae:
            print ("Unknown Exception while trying to remove Test Environment.")

    def _exit_test (self):
        self.__test_cleanup ()

    def begin (self):
        return 0 if self.tests_passed else 100

    """ Methods to check if the Test Case passes or not. """

    def __gen_local_filesys (self):
        file_sys = dict ()
        for parent, dirs, files in os.walk ('.'):
            for name in files:
                onefile = dict ()
                # Create the full path to file, removing the leading ./
                # Might not work on non-unix systems. Someone please test.
                filepath = os.path.join (parent, name)
                file_handle = open (filepath, 'r')
                file_content = file_handle.read ()
                onefile['content'] = file_content
                filepath = filepath[2:]
                file_sys[filepath] = onefile
                file_handle.close ()
        return file_sys


    def __check_downloaded_files (self, exp_filesys):
        local_filesys = self.__gen_local_filesys ()
        for files in exp_filesys:
            if files.name in local_filesys:
                local_file = local_filesys.pop (files.name)
                if files.content != local_file ['content']:
                    for line in unified_diff (local_file['content'], files.content, fromfile="Actual", tofile="Expected"):
                        sys.stderr.write (line)
                    raise TestFailed ("Contents of " + files.name + " do not match")
            else:
                raise TestFailed ("Expected file " + files.name +  " not found")
        if local_filesys:
            print (local_filesys)
            raise TestFailed ("Extra files downloaded.")

    def _replace_substring (self, string):
        pattern = re.compile ('\{\{\w+\}\}')
        match_obj = pattern.search (string)
        if match_obj is not None:
            rep = match_obj.group()
            temp = getattr (self, rep.strip ('{}'))
            string = string.replace (rep, temp)
        return string


    """ Test Rule Definitions """
    """ This should really be taken out soon. All this extra stuff to ensure
        re-use of old code is crap. Someone needs to re-write it. The new rework
        branch is much better written, but integrating it requires effort.
        All these classes should never exist. The whole server needs to modified.
    """

    class Authentication:
        def __init__ (self, auth_obj):
            self.auth_type = auth_obj['Type']
            self.auth_user = auth_obj['User']
            self.auth_pass = auth_obj['Pass']

    class ExpectHeader:
        def __init__ (self, header_obj):
            self.headers = header_obj

    class RejectHeader:
        def __init__ (self, header_obj):
            self.headers = header_obj

    class Response:
        def __init__ (self, retcode):
            self.response_code = retcode

    class SendHeader:
        def __init__ (self, header_obj):
            self.headers = header_obj

    def get_server_rules (self, file_obj):
        """ The handling of expect header could be made much better when the
            options are parsed in a true and better fashion. For an example,
            see the commented portion in Test-basic-auth.py.
        """
        server_rules = dict ()
        for rule in file_obj.rules:
            r_obj = getattr (self, rule) (file_obj.rules[rule])
            server_rules[rule] = r_obj
        return server_rules

    """ Pre-Test Hook Function Calls """

    def ServerFiles (self, server_files):
        for i in range (0, self.servers):
            file_list = dict ()
            server_rules = dict ()
            for file_obj in server_files[i]:
                content = self._replace_substring (file_obj.content)
                file_list[file_obj.name] = content
                rule_obj = self.get_server_rules (file_obj)
                server_rules[file_obj.name] = rule_obj
            self.server_list[i].server_conf (file_list, server_rules)

    def LocalFiles (self, local_files):
        for file_obj in local_files:
            file_handler = open (file_obj.name, "w")
            file_handler.write (file_obj.content)
            file_handler.close ()

    def ServerConf (self, server_settings):
        for i in range (0, self.servers):
            self.server_list[i].server_sett (server_settings)

    """ Test Option Function Calls """

    def WgetCommands (self, command_list):
        self.options = self._replace_substring (command_list)

    def Urls (self, url_list):
        self.urls = url_list

    """ Post-Test Hook Function Calls """

    def ExpectedRetcode (self, retcode):
        if self.act_retcode != retcode:
            pr = "Return codes do not match.\nExpected: " + str(retcode) + "\nActual: " + str(self.act_retcode)
            raise TestFailed (pr)

    def ExpectedFiles (self, exp_filesys):
        self.__check_downloaded_files (exp_filesys)

    def FilesCrawled (self, Request_Headers):
        for i in range (0, self.servers):
            headers = set(Request_Headers[i])
            o_headers = self.Request_remaining[i]
            header_diff = headers.symmetric_difference (o_headers)
            if len(header_diff) is not 0:
                printer ("RED", str (header_diff))
                raise TestFailed ("Not all files were crawled correctly")


""" Class for HTTP Tests. """

class HTTPTest (CommonMethods):

# Temp Notes: It is expected that when pre-hook functions are executed, only an empty test-dir exists.
# pre-hook functions are executed just prior to the call to Wget is made.
# post-hook functions will be executed immediately after the call to Wget returns.

    def __init__ (
        self,
        name="Unnamed Test",
        pre_hook=dict(),
        test_params=dict(),
        post_hook=dict(),
        servers=[HTTP]
    ):
        try:
            self.Server_setup (name, pre_hook, test_params, post_hook, servers)
        except TestFailed as tf:
            printer ("RED", "Error: " + tf.error)
            self.tests_passed = False
        except Exception as ae:
            printer ("RED", "Unhandled Exception Caught.")
            print ( ae.__str__ ())
            traceback.print_exc ()
            self.tests_passed = False
        else:
            printer ("GREEN", "Test Passed")
        finally:
            self._exit_test ()

    def Server_setup (self, name, pre_hook, test_params, post_hook, servers):
        self.name = name
        self.server_types = servers
        self.servers = len (servers)
        printer ("BLUE", "Running Test " + self.name)
        self.init_test_env (name)
        self.server_list = list()
        self.domain_list = list()
        for server_type in servers:
            server_inst = getattr (self, "init_" + server_type + "_Server") ()
            self.server_list.append (server_inst)
            domain = self.get_domain_addr (server_inst.server_address)
            self.domain_list.append (domain)
        #self.server = self.init_HTTP_Server ()
        #self.domain = self.get_domain_addr (self.server.server_address)

        self.pre_hook_call (pre_hook)
        self.call_test (test_params)
        self.post_hook_call (post_hook)

    def pre_hook_call (self, pre_hook):
        for pre_hook_func in pre_hook:
            try:
                assert hasattr (self, pre_hook_func)
            except AssertionError as ae:
                self.stop_HTTP_Server ()
                raise TestFailed ("Pre Test Function " + pre_hook_func + " not defined.")
            getattr (self, pre_hook_func) (pre_hook[pre_hook_func])

    def call_test (self, test_params):
        for test_func in test_params:
            try:
                assert hasattr (self, test_func)
            except AssertionError as ae:
                self.stop_HTTP_Server ()
                raise TestFailed ("Test Option " + test_func + " unknown.")
            getattr (self, test_func) (test_params[test_func])

        try:
            self.act_retcode = self.exec_wget (self.options, self.urls, self.domain_list)
        except TestFailed as tf:
            self.stop_HTTP_Server ()
            raise TestFailed (tf.__str__ ())
        self.stop_HTTP_Server ()

    def post_hook_call (self, post_hook):
        for post_hook_func in post_hook:
            try:
                assert hasattr (self, post_hook_func)
            except AssertionError as ae:
                raise TestFailed ("Post Test Function " + post_hook_func + " not defined.")
            getattr (self, post_hook_func) (post_hook[post_hook_func])

    def init_HTTP_Server (self):
        server = HTTPServer.HTTPd ()
        server.start ()
        return server

    def init_HTTPS_Server (self):
        server = HTTPServer.HTTPSd ()
        server.start ()
        return server

    def stop_HTTP_Server (self):
        self.Request_remaining = list ()
        for server in self.server_list:
            server_req = server.server_inst.get_req_headers ()
            self.Request_remaining.append (server_req)
            server.server_inst.shutdown ()

""" WgetFile is a File Data Container object """

class WgetFile:

    def __init__ (
        self,
        name,
        content="Test Contents",
        timestamp=None,
        rules=dict()
    ):
        self.name = name
        self.content = content
        self.timestamp = timestamp
        self.rules = rules

# vim: set ts=4 sts=4 sw=4 tw=80 et :
