import sys

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class TreeSitterConan(ConanFile):
    name = "py-tree-sitter"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires(f"tree-sitter/{self.version}")  # assumes C library and Python bindings are released in sync

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["Python3_EXECUTABLE"] = sys.executable
        tc.cache_variables["PY_TS_VERSION"] = self.version
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
