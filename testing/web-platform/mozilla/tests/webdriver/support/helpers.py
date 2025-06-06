import argparse
import json
import os
import re
import socket
import subprocess
import sys
import tempfile
import threading
import time
from contextlib import suppress
from copy import deepcopy
from urllib.parse import urlparse

import webdriver
from mozprofile import Preferences, Profile
from mozrunner import FirefoxRunner

from .context import using_context


class Browser:
    def __init__(
        self,
        binary,
        profile,
        env=None,
        extra_args=None,
        extra_prefs=None,
        log_level=None,
        truncate_enabled=True,
        use_bidi=False,
        use_marionette=False,
    ):
        self.profile = profile

        self.extra_args = extra_args
        self.extra_prefs = extra_prefs
        self.log_level = log_level
        self.truncate_enabled = truncate_enabled
        self.use_bidi = use_bidi
        self.use_marionette = use_marionette

        self.bidi_port_file = None
        self.remote_agent_host = None
        self.remote_agent_port = None

        cmdargs = ["-no-remote"]

        if use_bidi:
            self.webdriver_bidi_file = os.path.join(
                self.profile.profile, "WebDriverBiDiServer.json"
            )
            with suppress(FileNotFoundError):
                os.remove(self.webdriver_bidi_file)

        if use_marionette:
            cmdargs.extend(["-marionette"])

        # Avoid modifying extra_prefs to prevent side-effects with the "browser" fixture,
        # which checks session equality and would create a new session each time.
        prefs = self.extra_prefs or {}

        if log_level is not None:
            prefs.update({"remote.log.level": log_level})

        if truncate_enabled is False:
            prefs.update({"remote.log.truncate": False})

        self.profile.set_preferences(prefs)

        if self.use_bidi:
            cmdargs.extend(["--remote-debugging-port", "0"])
        if self.extra_args is not None:
            cmdargs.extend(self.extra_args)

        print(f"Run command: {binary} {cmdargs}")
        self.runner = FirefoxRunner(
            binary=binary, profile=self.profile, cmdargs=cmdargs, env=env
        )

    @property
    def websocket_url(self):
        if self.remote_agent_host is None or self.remote_agent_port is None:
            raise Exception("No WebSocket server running")

        return f"ws://{self.remote_agent_host}:{self.remote_agent_port}"

    @property
    def is_running(self):
        return self.runner.is_running()

    def start(self):
        # Start Firefox.
        self.runner.start()

        if self.use_bidi:
            # Wait until the WebDriverBiDiServer.json file is ready
            while not os.path.exists(self.webdriver_bidi_file):
                time.sleep(0.1)

            # Read the connection details from file
            data = json.loads(open(self.webdriver_bidi_file).read())
            self.remote_agent_host = data["ws_host"]
            self.remote_agent_port = int(data["ws_port"])

    def quit(self, clean_profile=True):
        if self.is_running:
            self.runner.stop()
            self.runner.cleanup()

        if clean_profile:
            self.profile.cleanup()

    def wait(self):
        if self.is_running is True:
            self.runner.wait()


class Geckodriver:
    PORT_RE = re.compile(rb".*Listening on [^ :]*:(\d+)")

    def __init__(self, configuration, hostname=None, extra_args=None, extra_env=None):
        self.config = configuration["webdriver"]
        self.requested_capabilities = configuration["capabilities"]
        self.hostname = hostname or configuration["host"]
        self.extra_args = extra_args or []
        self.env = configuration["browser"]["env"]
        self.extra_env = extra_env or {}

        self.command = None
        self.proc = None
        self.port = None
        self.reader_thread = None

        self.capabilities = {"alwaysMatch": self.requested_capabilities}
        self.session = None

    @property
    def remote_agent_port(self):
        webSocketUrl = self.session.capabilities.get("webSocketUrl")
        assert webSocketUrl is not None

        return urlparse(webSocketUrl).port

    def start(self):
        self.command = (
            [self.config["binary"], "--port", "0"]
            + self.config["args"]
            + self.extra_args
        )

        print(f"Running command: {' '.join(self.command)}")
        all_env = deepcopy(self.env)
        all_env.update(self.extra_env)

        self.proc = subprocess.Popen(self.command, env=all_env, stdout=subprocess.PIPE)

        self.reader_thread = threading.Thread(
            target=readOutputLine,
            args=(self.proc.stdout, self.processOutputLine),
            daemon=True,
        )
        self.reader_thread.start()
        # Wait for the port to become ready
        end_time = time.time() + 10
        while time.time() < end_time:
            returncode = self.proc.poll()
            if returncode is not None:
                raise ChildProcessError(
                    f"geckodriver terminated with code {returncode}"
                )
            if self.port is not None:
                with socket.socket() as sock:
                    if sock.connect_ex((self.hostname, self.port)) == 0:
                        break
            else:
                time.sleep(0.1)
        else:
            if self.port is None:
                raise OSError(
                    f"Failed to read geckodriver port started on {self.hostname}"
                )
            raise ConnectionRefusedError(
                f"Failed to connect to geckodriver on {self.hostname}:{self.port}"
            )

        self.session = webdriver.Session(
            self.hostname, self.port, capabilities=self.capabilities
        )

        return self

    def processOutputLine(self, line):
        sys.stdout.write(line)

        if self.port is None:
            m = self.PORT_RE.match(line)
            if m is not None:
                self.port = int(m.groups()[0])

    async def stop(self):
        if self.session is not None:
            await self.delete_session()
        if self.proc:
            self.proc.kill()
        self.port = None
        if self.reader_thread is not None:
            self.reader_thread.join()

    def new_session(self):
        self.session.start()

    async def delete_session(self):
        if self.session.bidi_session is not None:
            await self.session.bidi_session.end()

        self.session.end()


def clear_pref(session, pref):
    """Clear the user-defined value from the specified preference.

    :param pref: Name of the preference.
    """
    with using_context(session, "chrome"):
        session.execute_script(
            """
           const { Preferences } = ChromeUtils.importESModule(
             "resource://gre/modules/Preferences.sys.mjs"
           );
           Preferences.reset(arguments[0]);
           """,
            args=(pref,),
        )


def create_custom_profile(base_profile, default_preferences, clone=True):
    if clone:
        # Clone the current profile and remove the prefs.js file to only
        # keep default preferences as set in user.js.
        profile = Profile.clone(base_profile)
        prefs_path = os.path.join(profile.profile, "prefs.js")
        if os.path.exists(prefs_path):
            os.remove(prefs_path)
    else:
        profile = Profile(tempfile.mkdtemp(prefix="wdspec-"))
        profile.set_preferences(default_preferences)

    return profile


def get_arg_value(arg_names, args):
    """Get an argument value from a list of arguments

    This assumes that argparse argument parsing is close enough to the target
    to be compatible, at least with the set of inputs we have.

    :param arg_names: - List of names for the argument e.g. ["--foo", "-f"]
    :param args: - List of arguments to parse
    :returns: - Optional string argument value
    """
    parser = argparse.ArgumentParser()
    parser.add_argument(*arg_names, action="store", dest="value", default=None)
    parsed, _ = parser.parse_known_args(args)
    return parsed.value


def get_pref(session, pref):
    """Get the value of the specified preference.

    :param pref: Name of the preference.
    """
    with using_context(session, "chrome"):
        pref_value = session.execute_script(
            """
            const { Preferences } = ChromeUtils.importESModule(
              "resource://gre/modules/Preferences.sys.mjs"
            );

            let pref = arguments[0];

            prefs = new Preferences();
            return prefs.get(pref, null);
            """,
            args=(pref,),
        )
        return pref_value


def get_profile_folder(firefox_options):
    return get_arg_value(["--profile"], firefox_options["args"])


def readOutputLine(stream, callback):
    while True:
        line = stream.readline()
        if not line:
            break

        callback(line)


def read_user_preferences(profile_path, filename="user.js"):
    prefs_file = os.path.join(profile_path, filename)

    prefs = {}
    for pref_name, pref_value in Preferences().read_prefs(prefs_file):
        prefs[pref_name] = pref_value

    return prefs


def set_pref(session, pref, value):
    """Set the value of the specified preference.

    :param pref: Name of the preference.
    :param value: The value to set the preference to. If the value is None,
                  reset the preference to its default value. If no default
                  value exists, the preference will cease to exist.
    """
    if value is None:
        clear_pref(session, pref)
        return

    with using_context(session, "chrome"):
        session.execute_script(
            """
            const { Preferences } = ChromeUtils.importESModule(
              "resource://gre/modules/Preferences.sys.mjs"
            );

            const [pref, value] = arguments;

            prefs = new Preferences();
            prefs.set(pref, value);
            """,
            args=(pref, value),
        )
