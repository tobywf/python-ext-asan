# Using LLVM's address sanitizer and leak sanitizer for Python extensions

LLVM/clang have powerful capabilities for memory error detection for C/C++. This comes in the form of [AddressSanitizer (ASan)](https://github.com/google/sanitizers/wiki/AddressSanitizer) and [LeakSanitizer (LSan)](https://github.com/google/sanitizers/wiki/AddressSanitizerLeakSanitizer). However, they are somewhat tricky to set up if you don't want to rebuild Python with ASan support. But it's possible to apply them to a single Python extension.

## Walkthrough

I'm assuming you have both clang and Python 3 installed (I haven't tested with the default Python on macOS, only with the Homebrew version), and the ability yo create virtual environments (only a challenge on Debian/Ubuntu). The sanitizer wiki covers [how to use ASan for a single library](https://github.com/google/sanitizers/wiki/AddressSanitizerAsDso). It's a bit bare-bones, as "ASAN-DSO is not (yet?) officially supported. Use it at your own risk." Anyway, the relevant compile and link options are set in `setup.py`, basically `-fsanitize=address`, and `-shared-libasan`. The wiki also recommends `-fno-omit-frame-pointer`. I believe they could also be set via `CPPFLAGS` and `LDFLAGS`.

Create a virtual environment so as not to trash the system Python. I'm not going to activate the virtual environment, I find it's easier in this case.

```shell
python3 -m venv env
env/bin/python setup.py develop
```

These steps are also performed by `make` or `make install` if you prefer. Next, test the module can be imported. You want to see an ASan failure (which is good, it proves it's working), not a `ModuleNotFoundError`.

```shell
env/bin/python -c 'import asan'
```

The next steps are different on Linux and macOS; pick the relevant section for your environment. If you're on Windows, may I suggest a virtual machine or WSL?

### Linux

On Linux, this produced something like:

```console
$ env/bin/python -c 'import asan'
ImportError: libclang_rt.asan-x86_64.so: cannot open shared object file: No such file or directory
```

I preloaded the runtime, but you may want to hold off running this. First, check the path, it may be different. Also, the LSan is turned on by default on Linux, so you will get a lot of spew. Python (seems?) to leak a lot of objects.

```bash
LD_PRELOAD=/usr/local/lib/clang/11.0.0/lib/linux/libclang_rt.asan-x86_64.so \
  env/bin/python -c 'import asan'
```

Turn off the LSan (again, check the path to the runtime), you should get a clean run:

```bash
LD_PRELOAD=/usr/local/lib/clang/11.0.0/lib/linux/libclang_rt.asan-x86_64.so \
  ASAN_OPTIONS=detect_leaks=0 \
  env/bin/python -c 'import asan'
```

You could also try and suppress the spurious LSan detections. But I've found this to be difficult. An example file is provided called `lsan-suppr.txt`, this is intended for macOS (the function names seem to be slightly different).

I'm going to omit the options going forward, to reduce clutter and because they are different on macOS.

### macOS

On macOS, this produces something like:

```console
$ env/bin/python -c 'import asan'
==91213==ERROR: Interceptors are not working. This may be because AddressSanitizer is loaded too late (e.g. via dlopen). Please launch the executable with:
DYLD_INSERT_LIBRARIES=/Library/Developer/CommandLineTools/usr/lib/clang/11.0.0/lib/darwin/libclang_rt.asan_osx_dynamic.dylib
"interceptors not installed" && 0Abort trap: 6
```

You can also see that this is using the default/Apple clang. Let's do what the rather helpful error message says:

```console
$ DYLD_INSERT_LIBRARIES=/Library/Developer/CommandLineTools/usr/lib/clang/11.0.0/lib/darwin/libclang_rt.asan_osx_dynamic.dylib \
>   env/bin/python -c 'import asan'
==92278==ERROR: Interceptors are not working. This may be because AddressSanitizer is loaded too late (e.g. via dlopen). Please launch the executable with:
DYLD_INSERT_LIBRARIES=/Library/Developer/CommandLineTools/usr/lib/clang/11.0.0/lib/darwin/libclang_rt.asan_osx_dynamic.dylib
"interceptors not installed" && 0Abort trap: 6
```

Oops! This is a bug on macOS. I've found at least two other people who also hit this: [akim on StackOverflow](https://stackoverflow.com/a/47853433) and [Jonas Devlieghere](https://jonasdevlieghere.com/sanitizing-python-modules/). Basically, `DYLD_INSERT_LIBRARIES` won't work properly due to the way Python is wrapped/shimmed. I've provided a Python script (`fix-macos.py`) that:

* resolves where `env/bin/python` points to
* rewrites that to the unshimmed version
* moves `env/bin/python` to `env/bin/old`
* makes a new `env/bin/python` that points to the unshimmed version

Run it with `python3 fix-macos.py`. Now, when the command is run:

```bash
DYLD_INSERT_LIBRARIES=/Library/Developer/CommandLineTools/usr/lib/clang/11.0.0/lib/darwin/libclang_rt.asan_osx_dynamic.dylib \
  env/bin/python -c 'import asan'
```

You should get a clean execution. Unlike on Linux, leak detection/LSan is not enabled by default. You can try and turn it on:

```console
$ DYLD_INSERT_LIBRARIES=/Library/Developer/CommandLineTools/usr/lib/clang/11.0.0/lib/darwin/libclang_rt.asan_osx_dynamic.dylib \
>   ASAN_OPTIONS=detect_leaks=1 \
>   env/bin/python -c 'import asan'
==92353==AddressSanitizer: detect_leaks is not supported on this platform.
Abort trap: 6
```

This isn't supported for me with Apple's LLVM/clang, but may be fixed in macOS newer than Catalina (10.15). Alternatively, you may install clang via Homebrew (`brew install llvm`). It's then somewhat involved switching to use that LLVM/clang version, but possible. Note that Homebrew will tell you what paths to use. For me, this was the following (warning: the output is for Fish, not Bash):

```plain
To use the bundled libc++ please add the following LDFLAGS:
  LDFLAGS="-L/usr/local/opt/llvm/lib -Wl,-rpath,/usr/local/opt/llvm/lib"

llvm is keg-only, which means it was not symlinked into /usr/local,
because macOS already provides this software and installing another version in
parallel can cause all kinds of trouble.

If you need to have llvm first in your PATH, run:
  echo 'set -g fish_user_paths "/usr/local/opt/llvm/bin" $fish_user_paths' >> ~/.config/fish/config.fish

For compilers to find llvm you may need to set:
  set -gx LDFLAGS "-L/usr/local/opt/llvm/lib"
  set -gx CPPFLAGS "-I/usr/local/opt/llvm/include"
```

Therefore, the command becomes:

```bash
make clean
PATH="/usr/local/opt/llvm/bin:$PATH" \
  CC=/usr/local/opt/llvm/bin/clang \
  CXX=/usr/local/opt/llvm/bin/clang++ \
  LDFLAGS="-L/usr/local/opt/llvm/lib -Wl,-rpath,/usr/local/opt/llvm/lib" \
  CPPFLAGS="-I/usr/local/opt/llvm/include" \
  make install
```

Importantly, you must also use that runtime. Test that it works:

```bash
DYLD_INSERT_LIBRARIES=/usr/local/Cellar/llvm/11.0.1/lib/clang/11.0.1/lib/darwin/libclang_rt.asan_osx_dynamic.dylib \
  ASAN_OPTIONS=detect_leaks=1 \
  env/bin/python -c 'import asan'
```

You should be greeted with a bajillion leaks. Python (seems?) to leak a lot of objects. I've provided a sample LSan suppression file, which is crude, but enough for this demo. (As an aside, I wish there was a way to run LSan and capture all leaks as a suppression file to be used with future runs.)

```bash
DYLD_INSERT_LIBRARIES=/usr/local/Cellar/llvm/11.0.1/lib/clang/11.0.1/lib/darwin/libclang_rt.asan_osx_dynamic.dylib \
  ASAN_OPTIONS=detect_leaks=1 \
  LSAN_OPTIONS=suppressions=lsan-suppr.txt \
  env/bin/python -c 'import asan'
```

You should now get a clean run.  I'm going to omit the options going forward, to reduce clutter and because they are different on Linux.

### Testing ASan and LSan

There's a single module method, `asan.test` to play around with this. It takes two parameters, `index` and `leak`:

```bash
... env/bin/python -c 'import asan; asan.test(index=0, leak=False)'
```

First, `leak`. If it's false, a stack-allocated array is used. If it's true, a heap-allocated array is used (that is then leaked, if nothing else happens). Second, `index`. Both the stack and heap-allocated arrays have 100 elements. If you set `index` to anything between 0 and 99, for the stack one nothing happens, and the heap one will leak. If index is greater than 99, then either a `stack-buffer-overflow` or a `heap-buffer-overflow` is triggered. Fun times!


### Taking it further

ASan is obviously only to detect errors if code is exercised. But once an ASan-enabled build of an extension can be made, it's also easy to run an existing test suite against the extension. Ideally, this would be done via continuous integration, but that's a story for another post. The main difference to the example is that the ASan flags in `extra_compile_args` and `extra_link_args` could be set conditionally based on an environment variable, or passed in via `CPPFLAGS` and `LDFLAGS`, similar to the macOS example of using a different LLVM/clang version.

### Conclusion

It's probably worth spending an afternoon setting up memory error detection via LLVM/clang's address sanitizer (ASan), and a further day (or two...? :D) integrating it into the continuous integration runs for peace of mind with any Python C/C++ extension. However, leak sanitization (LSan) is imperfect when using Python as a host and may prove brittle, with many false-positives. The good news is that if ASan is set up, enabling LSan in the future should be straight-forward.
