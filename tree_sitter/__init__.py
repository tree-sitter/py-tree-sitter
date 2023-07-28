"""Python bindings for tree-sitter."""

from ctypes import cdll, c_void_p
from distutils.ccompiler import new_compiler
from distutils.unixccompiler import UnixCCompiler
from os import path
from platform import system
from tempfile import TemporaryDirectory
from glob import glob
import re
import json
from tree_sitter.binding import _language_field_id_for_name, _language_query
from tree_sitter.binding import Node, Parser, Tree, TreeCursor  # noqa: F401


class Language:
    """A tree-sitter language"""

    @staticmethod
    def build_library(output_path, repo_paths, index=dict()):
        """
        Build a dynamic library at the given path, based on the parser
        repositories at the given paths.

        Returns `True` if the dynamic library was compiled and `False` if
        the library already existed and was modified more recently than
        any of the source files.
        """
        output_mtime = path.getmtime(output_path) if path.exists(output_path) else 0

        if not repo_paths:
            raise ValueError("Must provide at least one language folder")

        cpp = False
        source_paths = []
        for repo_path in repo_paths:
            src_path = path.join(repo_path, "src")
            source_paths.append(path.join(src_path, "parser.c"))
            if path.exists(path.join(src_path, "scanner.cc")):
                cpp = True
                source_paths.append(path.join(src_path, "scanner.cc"))
            elif path.exists(path.join(src_path, "scanner.c")):
                source_paths.append(path.join(src_path, "scanner.c"))
        source_mtimes = [path.getmtime(__file__)] + [
            path.getmtime(path_) for path_ in source_paths
        ]

        if index is not None:
            for repo_path in repo_paths:
                # find the symbol name of the parser to use for dlopen later.
                # doesn't always match the scope, repo name, or file types.
                parser = None
                with open(path.join(repo_path, "src", "parser.c"), 'r') as file:
                    for line in file:
                        if line.startswith("extern const TSLanguage *tree_sitter_"):
                            parser = re.search(r"tree_sitter_(.+?)\(", line).group(1)
                            break
                if parser is None:
                    print("ERROR: failed to find parser name in", repo_path)
                    continue

                # package.json is required, but may be missing.
                # find the json file, and parse out the tree-sitter section.
                package_json_path = path.join(repo_path, 'package.json')
                if not path.isfile(package_json_path):
                    print("NOTE: missing package.json in", repo_path)
                    index[parser] = {}
                    continue
                with open(package_json_path, 'r') as file:
                    package_json = json.load(file)

                # we may also be nested in a repo with multiple parsers (typescript, ocaml).
                nested = False
                if 'main' in package_json and package_json['main'].startswith('../'):
                    nested = True
                    package_json_path = path.join(repo_path, '..', 'package.json')
                    with open(package_json_path, 'r') as file:
                        package_json = json.load(file)

                if 'tree-sitter' not in package_json:
                    print("NOTE: missing tree-sitter section in package.json from", repo_path)
                    index[parser] = {}
                    continue

                # tree-sitter section can contain multiple entries.
                # if nested, attempt to find the one that matches this parser.
                entries = package_json['tree-sitter']
                if not nested:
                    index[parser] = entries
                    continue
                for entry in entries:
                    if entry['scope'].endswith(parser) or ('path' in entry and entry['path'] == parser):
                        index[parser] = [entry]
                        break
                if parser not in index:
                    index[parser] = entries

        compiler = new_compiler()
        if isinstance(compiler, UnixCCompiler):
            compiler.compiler_cxx[0] = "c++"

        if max(source_mtimes) <= output_mtime:
            return False

        with TemporaryDirectory(suffix="tree_sitter_language") as out_dir:
            object_paths = []
            for source_path in source_paths:
                if system() == "Windows":
                    flags = None
                else:
                    flags = ["-fPIC"]
                    if source_path.endswith(".c"):
                        flags.append("-std=c99")
                object_paths.append(
                    compiler.compile(
                        [source_path],
                        output_dir=out_dir,
                        include_dirs=[path.dirname(source_path)],
                        extra_preargs=flags,
                    )[0]
                )
            compiler.link_shared_object(
                object_paths,
                output_path,
                target_lang="c++" if cpp else "c",
            )
        return True

    @staticmethod
    def lookup_language_name_for_file(index, file_name, file_contents=None):
        matching_keys = []
        for key, entries in index.items():
            for entry in entries:
                if 'file-types' not in entry:
                    continue
                for ft in entry['file-types']:
                    if file_name == ft or file_name.endswith(ft):
                        matching_keys.append(key)

        if file_contents is None or len(matching_keys) <= 1:
            return matching_keys[0] if matching_keys else None

        best_score = -1
        best_key = None
        for key in matching_keys:
            for entry in index[key]:
                if 'content-regex' in entry and file_contents is not None:
                    match = re.search(entry['content-regex'], file_contents)
                    if match:
                        score = match.end() - match.start()
                        if score > best_score:
                            best_score = score
                            best_key = key

        return best_key if best_key else matching_keys[0]

    def __init__(self, library_path, name):
        """
        Load the language with the given name from the dynamic library
        at the given path.
        """
        self.name = name
        self.lib = cdll.LoadLibrary(library_path)
        language_function = getattr(self.lib, "tree_sitter_%s" % name)
        language_function.restype = c_void_p
        self.language_id = language_function()

    def field_id_for_name(self, name):
        """Return the field id for a field name."""
        return _language_field_id_for_name(self.language_id, name)

    def query(self, source):
        """Create a Query with the given source code."""
        return _language_query(self.language_id, source)
