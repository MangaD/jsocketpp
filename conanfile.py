from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout
import os

class jsocketppConan(ConanFile):
    name = "jsocketpp"
    version = "0.1.0"
    license = "MIT"
    author = "Your Name <your@email.com>"
    url = "https://github.com/yourusername/jsocketpp"
    description = "A modern C++ socket library."
    topics = ("socket", "network", "tcp", "udp", "c++")
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"
    exports_sources = "CMakeLists.txt", "src/*", "include/*"

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["socket"]
