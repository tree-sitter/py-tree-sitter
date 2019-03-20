from ctypes import cdll, c_void_p
from tree_sitter_binding import Parser
import subprocess
import os.path as path


INCLUDE_PATH = path.join(path.dirname(__file__), "core", "lib", "include")


class Language:
    def build(repo_path, output_path):
        compiler = "clang++"
        src_path = path.join(repo_path, "src")
        parser_path = path.join(src_path, "parser.c")

        command = [
            compiler,
            "-shared",
            "-o",
            output_path,
            "-I",
            INCLUDE_PATH,
            "-xc",
            path.join(src_path, "parser.c")
        ]

        if path.exists(path.join(src_path, "scanner.cc")):
            command.append("-xc++")
            command.append(path.join(src_path, "scanner.cc"))
        elif path.exists(path.join(src_path, "scanner.c")):
            command.append(path.join(src_path, "scanner.c"))

        subprocess.run(command)

    def __init__(self, path, name):
        self.path = path
        self.name = name
        self.lib = cdll.LoadLibrary(path)
        function = getattr(self.lib, "tree_sitter_%s" % name)
        function.restype = c_void_p
        self.language_id = function()
