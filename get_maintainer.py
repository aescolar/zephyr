#!/usr/bin/env python3

# Copyright (c) 2019 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

import argparse
import glob
import os
import re
import shlex
import subprocess

from yaml import load, YAMLError
try:
    # Use the speedier C LibYAML parser if available
    from yaml import CLoader as Loader
except ImportError:
    from yaml import Loader


def _main():
    # Entry point when run as an executable

    args = _parse_args()

    maint = Maintainers(args.maintainers)

    if args.args_are_paths:
        subsystems = {subsys for path in args.commit_ranges_or_paths
                             for subsys in maint.path2subsystems(path)}
    else:
        commit_ranges = args.commit_ranges_or_paths or ["HEAD~.."]
        subsystems = {subsys for commit_range in commit_ranges
                             for subsys in maint.commits2subsystems(commit_range)}

    for subsys in subsystems:
        print("""\
{}:
\tmaintainers: {}
\tsub-maintainers: {}
\tinform: {}""".format(subsys.name,
                       ", ".join(subsys.maintainers),
                       ", ".join(subsys.sub_maintainers),
                       ", ".join(subsys.inform)))


def _parse_args():
    parser = argparse.ArgumentParser(
        description="""
Prints maintainer information for commits or paths, derived from
MAINTAINERS.yml. By default, one or more commit ranges is expected. If run with
no arguments, the commit range HEAD~.. is used (just the tip commit). Commit
ranges are passed as-is to 'git diff --name-only' to get a list of modified
files.
""")

    parser.add_argument("-m", "--maintainers",
                        metavar="MAINTAINERS_FILE",
                        default="MAINTAINERS.yml",
                        help="Maintainers file to load (default: MAINTAINERS.yml)")

    parser.add_argument("-f", "--files",
                        dest="args_are_paths",
                        action="store_true",
                        help="Interpret arguments as paths instead of commit ranges")

    parser.add_argument("commit_ranges_or_paths",
                        nargs="*",
                        help="Commit ranges or (with -f) paths (default: HEAD~..)")

    return parser.parse_args()


class Maintainers:
    def __init__(self, fname):
        yaml = _load_maintainers(fname)

        self.subsystems = []
        for subsys_name, subsys_dict in yaml.items():
            subsys = Subsystem()
            subsys.name = subsys_name
            subsys.maintainers = subsys_dict.get("maintainers", [])
            subsys.sub_maintainers = subsys_dict.get("sub-maintainers", [])
            subsys.inform = subsys_dict.get("inform", [])
            subsys._files = subsys_dict.get("files")
            subsys._files_regex = subsys_dict.get("files-regex")
            self.subsystems.append(subsys)

    def path2subsystems(self, path):
        return [subsys for subsys in self.subsystems if subsys._contains(path)]

    def commits2subsystems(self, commits):
        subsystems = set()
        for path in _git("diff", "--name-only", commits).splitlines():
            subsystems.update(self.path2subsystems(path))
        return subsystems


class Subsystem:
    def _contains(self, path):
        if self._files is not None:
            for glob in self._files:
                if _glob_match(glob, path):
                    return True

        if self._files_regex is not None:
            for regex in self._files_regex:
                if re.match(regex, path):
                    return True

        return False

    def __repr__(self):
        return "<Subsystem {}>".format(self.name)


def _glob_match(glob, path):
    match_fn = re.match if glob.endswith("/") else re.fullmatch
    regex = glob.replace(".", "\\.").replace("*", "[^/]*").replace("?", "[^/]")
    return match_fn(regex, path)


def _load_maintainers(fname):
    with open(fname, encoding="utf-8") as f:
        try:
            yaml = load(f, Loader=Loader)
        except YAMLError as e:
            raise MaintainersError("{}: YAML error: {}".format(fname, e))

        _check_maintainers(fname, yaml)
        return yaml


def _check_maintainers(fname, yaml):
    def ferr(msg):
        _err("{}: {}".format(fname, msg))

    if not isinstance(yaml, dict):
        ferr("empty or malformed YAML (not a dict)")

    ok_keys = {"maintainers", "sub-maintainers", "inform", "files",
               "files-regex"}

    for subsys_name, subsys_dict in yaml.items():
        if not isinstance(subsys_dict, dict):
            ferr("malformed entry for subsystem '{}' (not a dict)"
                 .format(subsys_name))

        for key in subsys_dict:
            if key not in ok_keys:
                ferr("unknown key '{}' in subsystem '{}'"
                     .format(key, subsys_name))

        if not subsys_dict.keys() & {"files", "files-regex"}:
            ferr("either 'files' or 'files-regex' (or both) must be specified "
                 "for subsystem '{}'".format(subsys_name))

        for list_name in "maintainers", "sub-maintainers", "inform", "files", \
                         "files-regex":
            if list_name in subsys_dict:
                lst = subsys_dict[list_name]
                if not (isinstance(lst, list) and
                        all(isinstance(elm, str) for elm in lst)):
                    ferr("malformed '{}' value for subsystem '{}' -- should "
                         "be a list of strings".format(list_name, subsys_name))

        if "files" in subsys_dict:
            for glob_pattern in subsys_dict["files"]:
                # TODO: Change this if it turns out to be too slow
                paths = glob.glob(glob_pattern)
                if not paths:
                    ferr("glob pattern '{}' in 'files' in subsystem '{}' does "
                         "not match any files"
                         .format(glob_pattern, subsys_name))
                if not glob_pattern.endswith("/"):
                    for path in paths:
                        if os.path.isdir(path):
                            ferr("glob pattern '{}' in 'files' in subsystem "
                                 "'{}' matches a directory but does not end "
                                 "in '/'".format(glob_pattern, subsys_name))

        if "files-regex" in subsys_dict:
            for regex in subsys_dict["files-regex"]:
                try:
                    # This also caches the regex in the re module, so we don't
                    # need to worry
                    re.compile(regex)
                except re.error as e:
                    ferr("bad regular expression '{}' in 'file_regex' in "
                         "'{}': {}".format(regex, subsys_name, e.msg))


def _git(*args):
    # Helper for running a Git command. Returns the rstrip()ed stdout output.
    # Called like git("diff"). Exits with SystemError (raised by sys.exit()) on
    # errors.

    git_cmd = ("git",) + args
    git_cmd_s = " ".join(shlex.quote(word) for word in git_cmd)  # For errors

    try:
        git_process = subprocess.Popen(
            git_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except FileNotFoundError:
        _giterr("git executable not found (when running '{}'). Check that "
                "it's in listed in the PATH environment variable"
                .format(git_cmd_s))
    except OSError as e:
        _giterr("failed to run '{}': {}".format(git_cmd_s, e))

    stdout, stderr = git_process.communicate()
    if git_process.returncode:
        _giterr("failed to run '{}': stdout:\n{}\nstderr:\n{}".format(
            git_cmd_s, stdout.decode("utf-8"), stderr.decode("utf-8")))

    return stdout.decode("utf-8").rstrip()


def _err(msg):
    raise MaintainersError(msg)


def _giterr(msg):
    raise GitError(msg)


class MaintainersError(Exception):
    "Exception raised for MAINTAINERS.yml-related errors"


class GitError(Exception):
    "Exception raised for Git-related errors"


if __name__ == "__main__":
    _main()
