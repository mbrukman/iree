#!/usr/bin/env python3
# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This script assists with converting from Bazel BUILD files to CMakeLists.txt.
#
# Bazel BUILD files should, where possible, be written to use simple features
# that can be directly evaluated and avoid more advanced features like
# variables, list comprehensions, etc.
#
# Generated CMake files will be similar in structure to their source BUILD
# files by using the functions in build_tools/cmake/ that imitate corresponding
# Bazel rules (e.g. cc_library -> iree_cc_library.cmake).
#
# For usage, see:
#   python3 build_tools/scripts/bazel_to_cmake.py --help

import argparse
import bazel_to_cmake_targets
import datetime
import os
import textwrap

repo_root = None


def parse_arguments():
  global repo_root

  parser = argparse.ArgumentParser(
      description="Bazel to CMake conversion helper.")
  parser.add_argument(
      "--preview",
      help="Prints results instead of writing files",
      action="store_true",
      default=False)

  # Specify only one of these (defaults to --root_dir=iree).
  group = parser.add_mutually_exclusive_group()
  group.add_argument(
      "--dir",
      help="Converts the BUILD file in the given directory",
      default=None)
  group.add_argument(
      "--root_dir",
      help="Converts all BUILD files under a root directory (defaults to iree/)",
      default="iree")

  # TODO(scotttodd): --check option that returns success/failure depending on
  #   if files match the converted versions

  args = parser.parse_args()

  # --dir takes precedence over --root_dir.
  # They are mutually exclusive, but the default value is still set.
  if args.dir:
    args.root_dir = None

  return args


def setup_environment():
  """Sets up some environment globals."""
  global repo_root

  # Determine the repository root (two dir-levels up).
  repo_root = os.path.dirname(
      os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


class BuildFileFunctions(object):
  """Object passed to `exec` that has handlers for BUILD file functions."""

  def __init__(self, converter):
    self.converter = converter

  # ------------------------------------------------------------------------- #
  # Conversion utilities, written to reduce boilerplate and allow for reuse   #
  # between similar rule conversions (e.g. cc_library and cc_binary).         #
  # ------------------------------------------------------------------------- #

  def _convert_name_block(self, **kwargs):
    #  NAME
    #    rule_name
    return "  NAME\n    %s\n" % (kwargs["name"])

  def _convert_namespace_block(self, **kwargs):
    #  CC_NAMESPACE
    #    "cc_namespace"
    return "  CC_NAMESPACE\n    \"%s\"\n" % (kwargs["cc_namespace"])

  def _convert_translation_block(self, **kwargs):
    return "  TRANSLATION\n    \"%s\"\n" % (kwargs["translation"])

  def _convert_option_block(self, option, option_value):
    if option_value:
      # Note: this is a truthiness check as well as an existence check, i.e.
      # Bazel `testonly = False` will be handled correctly by this condition.
      return "  %s\n" % option
    else:
      return ""

  def _convert_alwayslink_block(self, **kwargs):
    return self._convert_option_block("ALWAYSLINK", kwargs.get("alwayslink"))

  def _convert_testonly_block(self, **kwargs):
    return self._convert_option_block("TESTONLY", kwargs.get("testonly"))

  def _convert_filelist_block(self, list_name, files):
    if not files:
      return ""

    #  list_name
    #    "file_1.h"
    #    "file_2.h"
    #    "file_3.h"
    files_list = "\n".join(["    \"%s\"" % (file) for file in files])
    return "  %s\n%s\n" % (list_name, files_list)

  def _convert_hdrs_block(self, **kwargs):
    return self._convert_filelist_block("HDRS", kwargs.get("hdrs"))

  def _convert_srcs_block(self, **kwargs):
    return self._convert_filelist_block("SRCS", kwargs.get("srcs"))

  def _convert_src_block(self, **kwargs):
    return "  SRC\n    \"%s\"\n" % kwargs.get("src")

  def _convert_target(self, target):
    if target.startswith(":"):
      # Bazel package-relative `:logging` -> CMake absolute `iree::base::logging`
      package = os.path.dirname(self.converter.rel_build_file_path)
      package = package.replace(os.path.sep, "::")
      if package.endswith(target):
        target = package  # Omit target if it matches the package name
      else:
        target = package + ":" + target
    elif not target.startswith("//iree"):
      # External target, call helper method for special case handling.
      target = bazel_to_cmake_targets.convert_external_target(target)
    else:
      # Bazel `//iree/base`     -> CMake `iree::base`
      # Bazel `//iree/base:api` -> CMake `iree::base::api`
      target = target.replace("//", "")  # iree/base:api
      target = target.replace(":", "::")  # iree/base::api
      target = target.replace("/", "::")  # iree::base::api
    return target

  def _convert_deps_block(self, **kwargs):
    if not kwargs.get("deps"):
      return ""

    #  DEPS
    #    package1::target1
    #    package1::target2
    #    package2::target
    deps = kwargs.get("deps")
    deps_list = [self._convert_target(dep) for dep in deps]
    deps_list = sorted(list(set(deps_list)))  # Remove duplicates and sort.
    deps_list = "\n".join(["    %s" % (dep,) for dep in deps_list])
    return "  DEPS\n%s\n" % (deps_list,)

  def _convert_unimplemented_function(self, rule, *args, **kwargs):
    name = kwargs.get("name", "unnamed")
    self.converter.body += "# Unimplemented %(rule)s %(name)s\n" % {
        "rule": rule,
        "name": name
    }

  # ------------------------------------------------------------------------- #
  # Function handlers that convert BUILD definitions to CMake definitions.    #
  #                                                                           #
  # Names and signatures must match 1:1 with those expected in BUILD files.   #
  # Each function that may be found in a BUILD file must be listed here.      #
  # ------------------------------------------------------------------------- #

  def load(self, *args):
    pass

  def package(self, **kwargs):
    # No mapping to CMake, ignore.
    pass

  def iree_build_test(self, **kwargs):
    pass

  def filegroup(self, **kwargs):
    # Not implemented yet. Might be a no-op, or may want to evaluate the srcs
    # attribute and pass them along to any targets that depend on the filegroup.
    # Cross-package dependencies and complicated globs could be hard to handle.
    self._convert_unimplemented_function("filegroup", **kwargs)

  def exports_files(self, *args, **kwargs):
    pass

  def glob(self, *args):
    # Not supported during conversion (yet?).
    self._convert_unimplemented_function("glob", *args)

  def config_setting(self, **kwargs):
    # No mapping to CMake, ignore.
    pass

  def cc_library(self, **kwargs):
    name_block = self._convert_name_block(**kwargs)
    hdrs_block = self._convert_hdrs_block(**kwargs)
    srcs_block = self._convert_srcs_block(**kwargs)
    deps_block = self._convert_deps_block(**kwargs)
    alwayslink_block = self._convert_alwayslink_block(**kwargs)
    testonly_block = self._convert_testonly_block(**kwargs)

    self.converter.body += """iree_cc_library(
%(name_block)s%(hdrs_block)s%(srcs_block)s%(deps_block)s%(alwayslink_block)s%(testonly_block)s  PUBLIC
)\n\n""" % {
    "name_block": name_block,
    "hdrs_block": hdrs_block,
    "srcs_block": srcs_block,
    "deps_block": deps_block,
    "alwayslink_block": alwayslink_block,
    "testonly_block": testonly_block,
    }

  def cc_test(self, **kwargs):
    name_block = self._convert_name_block(**kwargs)
    hdrs_block = self._convert_hdrs_block(**kwargs)
    srcs_block = self._convert_srcs_block(**kwargs)
    deps_block = self._convert_deps_block(**kwargs)

    self.converter.body += """iree_cc_test(
%(name_block)s%(hdrs_block)s%(srcs_block)s%(deps_block)s)\n\n""" % {
    "name_block": name_block,
    "hdrs_block": hdrs_block,
    "srcs_block": srcs_block,
    "deps_block": deps_block,
    }

  def cc_binary(self, **kwargs):
    name_block = self._convert_name_block(**kwargs)
    srcs_block = self._convert_srcs_block(**kwargs)
    deps_block = self._convert_deps_block(**kwargs)

    self.converter.body += """iree_cc_binary(
%(name_block)s%(srcs_block)s%(deps_block)s)\n\n""" % {
    "name_block": name_block,
    "srcs_block": srcs_block,
    "deps_block": deps_block,
    }

  def spirv_kernel_cc_library(self, **kwargs):
    name_block = self._convert_name_block(**kwargs)
    srcs_block = self._convert_srcs_block(**kwargs)

    self.converter.body += """iree_spirv_kernel_cc_library(
%(name_block)s%(srcs_block)s)\n\n""" % {
    "name_block": name_block,
    "srcs_block": srcs_block,
    }

  def iree_bytecode_module(self, **kwargs):
    name_block = self._convert_name_block(**kwargs)
    src_block = self._convert_src_block(**kwargs)
    namespace_block = self._convert_namespace_block(**kwargs)
    translation_block = self._convert_translation_block(**kwargs)

    self.converter.body += """iree_bytecode_module(
%(name_block)s%(src_block)s%(namespace_block)s%(translation_block)s  PUBLIC\n)\n\n""" % {
    "name_block": name_block,
    "src_block": src_block,
    "namespace_block": namespace_block,
    "translation_block": translation_block,
    }

  def gentbl(self, **kwargs):
    self._convert_unimplemented_function("gentbl", **kwargs)

  def cc_embed_data(self, **kwargs):
    self._convert_unimplemented_function("cc_embed_data", **kwargs)

  def iree_setup_lit_package(self, **kwargs):
    self._convert_unimplemented_function("iree_setup_lit_package", **kwargs)

  def iree_glob_lit_tests(self, **kwargs):
    self._convert_unimplemented_function("iree_glob_lit_tests", **kwargs)


class Converter(object):
  """Conversion state tracking and full file template substitution."""

  def __init__(self, directory_path, rel_build_file_path):
    self.body = ""
    self.directory_path = directory_path
    self.rel_build_file_path = rel_build_file_path

  def convert(self):
    # One `add_subdirectory(name)` per subdirectory.
    add_subdirectories = ""
    for root, dirs, file_names in os.walk(self.directory_path):
      add_subdirectories = "\n".join(
          ["add_subdirectory(%s)" % (dir,) for dir in dirs])
      # Stop walk, only add direct subdirectories.
      break

    converted_file = self.template % {
        "date_year": datetime.date.today().year,
        "add_subdirectories": add_subdirectories,
        "body": self.body,
    }

    # Cleanup newline characters. This is more convenient than ensuring all
    # conversions are careful with where they insert newlines.
    converted_file = converted_file.replace("\n\n\n", "\n")
    converted_file = converted_file.rstrip() + "\n"

    return converted_file

  template = textwrap.dedent("""\
    # Copyright %(date_year)s Google LLC
    #
    # Licensed under the Apache License, Version 2.0 (the "License");
    # you may not use this file except in compliance with the License.
    # You may obtain a copy of the License at
    #
    #      https://www.apache.org/licenses/LICENSE-2.0
    #
    # Unless required by applicable law or agreed to in writing, software
    # distributed under the License is distributed on an "AS IS" BASIS,
    # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    # See the License for the specific language governing permissions and
    # limitations under the License.

    %(add_subdirectories)s

    %(body)s""")


def GetDict(obj):
  ret = {}
  for k in dir(obj):
    if not k.startswith("_"):
      ret[k] = getattr(obj, k)
  return ret


def convert_directory_tree(root_directory_path, write_files):
  print("convert_directory_tree: %s" % (root_directory_path,))
  for root, dirs, file_names in os.walk(root_directory_path):
    convert_directory(root, write_files)


def convert_directory(directory_path, write_files):
  if not os.path.isdir(directory_path):
    raise FileNotFoundError("Cannot find directory '%s'" % (directory_path,))

  build_file_path = os.path.join(directory_path, "BUILD")
  cmakelists_file_path = os.path.join(directory_path, "CMakeLists.txt")

  if not os.path.isfile(build_file_path):
    # No Bazel BUILD file in this directory to convert, skip.
    return

  global repo_root
  rel_build_file_path = os.path.relpath(build_file_path, repo_root)
  rel_cmakelists_file_path = os.path.relpath(cmakelists_file_path, repo_root)
  print("Converting %s to %s" % (rel_build_file_path, rel_cmakelists_file_path))

  if write_files:
    # TODO(scotttodd): Attempt to merge instead of overwrite?
    #   Existing CMakeLists.txt may have special logic that should be preserved
    if os.path.isfile(cmakelists_file_path):
      print("  %s already exists, overwritting" % (rel_cmakelists_file_path,))
    else:
      print("  %s does not exist yet, creating" % (rel_cmakelists_file_path,))
  print("")

  with open(build_file_path, "rt") as build_file:
    build_file_code = compile(build_file.read(), build_file_path, "exec")
    converter = Converter(directory_path, rel_build_file_path)
    try:
      exec(build_file_code, GetDict(BuildFileFunctions(converter)))
      converted_text = converter.convert()

      if write_files:
        with open(cmakelists_file_path, "wt") as cmakelists_file:
          cmakelists_file.write(converted_text)
      else:
        print(converted_text)
    except NameError as e:
      print(
          "Failed to convert %s. Missing a rule handler in bazel_to_cmake.py?" %
          (rel_build_file_path))
      print("  Reason: `%s: %s`" % (type(e).__name__, e))
    except KeyError as e:
      print(
          "Failed to convert %s. Missing a conversion in bazel_to_cmake_targets.py?"
          % (rel_build_file_path))
      print("  Reason: `%s: %s`" % (type(e).__name__, e))


def main(args):
  """Runs Bazel to CMake conversion."""
  global repo_root

  write_files = not args.preview

  if args.root_dir:
    convert_directory_tree(os.path.join(repo_root, args.root_dir), write_files)
  elif args.dir:
    convert_directory(os.path.join(repo_root, args.dir), write_files)


if __name__ == "__main__":
  setup_environment()
  main(parse_arguments())
