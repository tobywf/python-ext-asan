from setuptools import setup, Extension

asan = Extension(
    "asan",
    ["asan.c"],
    extra_compile_args=[
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
        "-O1",
        "-g",
        "-fsanitize=address",
        "-fno-omit-frame-pointer",
        "-shared-libasan",
    ],
    extra_link_args=[
        "-g",
        "-fsanitize=address",
        "-shared-libasan",
    ],
    language="c",
)

setup(name="asan-test", ext_modules=[asan])
