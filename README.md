# Quickbuild
> [!WARNING]
> Quickbuild is currently in early beta and is undergoing major changes. Expect frequent crashes, segmentation faults, and undefined behaviour. Please see the roadmap for more information.

## "What is this?"

Tired of looking up what `$<` and `$@` does? Say hello to `[depends]` and `[whatever-you-fancy]`. Your build system shouldn't get in the way of your next big idea. Quickbuild is an intuitive way of writing simple build scripts for your projects without having to worry about googling the Make syntax anymore.

## Features
- 🌱 Intuitive & pain-free setup
- ⚙️ Make-style configuration
- 🪶 Lightweight & performant

Quickbuild trades a slightly more verbose configuration with intuitive and simpler syntax that takes the pain out of writing makefiles. This makes it suitable for small to medium sized projects and for those who are starting out with low level development. However, this is *not* a replacement for Make - Quickbuild does not have, and will never achieve, feature parity with other build systems such as Make or CMake.

Quickbuild is fast enough that you never have to worry about performance. It is written in C++, and performance analysis is regularly applied to spot any bottlenecks. Based on limited testing, Quickbuild seems to be within 5% of the runtime of Make.

## Installation
Quickbuild is available from the AUR as `quickbuild-git`. If you aren't running Arch, you can also build it from source.

### Building from source

> Dependencies:
> - make _or_ quickbuild
> - clang >= 17

Normal installation using Make:
```
$ git clone https://gitlab.com/nordtektiger/quickbuild
$ cd quickbuild
$ make
# make install
```

Alternatively, you can use a previous version of Quickbuild to build a newer one from source:
```
$ git clone https://gitlab.com/nordtektiger/quickbuild
$ cd quickbuild
$ quickbuild
# quickbuild install
```

## Syntax
All configuration is to be stored at project root in a file named "quickbuild". The structure of a Quickbuild config is very similar to that of a Makefile, but with slightly more verbose syntax.


### Variables
Quickbuild implements three fundamental types: strings, booleans, and lists. Variable types are automatically inferred, and variables are lazily evaluated, cached, and declared using the following syntax:
```
# this is a comment!
my_string = "foo-bar";
my_bool = true;
my_fancy_list = "element1", "elem2", "hi";

# beware - the following does not evaluate!!
my_faulty_list = "foo1", false, "foo3";     # can't have two different types in the same list
```

If an asterisk (wildcard) is present in a string, it is automatically expended into all matching filepaths. A notable exception to this rule is when it's present in a replacement operator, where the asterisks serve as a matching rule instead.
```
my_source_files = "src/*.cpp";      # expands into "src/foo.cpp", "src/bar.cpp", ...
my_header_files = "src/*.hpp";      # expands into "src/baz.hpp", "src/another.hpp", ...
```

There is also an in-built operator for a simple search-and-replace (often called the replacement operator).
```
sources = "src/thing.cpp", "src/another.cpp";
objects = sources: "src/*.cpp" -> "obj/*.o";        # expands into "obj/thing.o", "obj/another.o"
```

String interpolation is also implemented, and requires the use of brackets.
```
foo = "World";
bar = "Hello, [foo]!";      # "Hello, World!"
```

### Tasks 
Every task has to have a name and may optionally contain any number of additional fields field. Tasks are equivalent to Make targets, and can be declared as follows.
```
# the topmost task will always execute when quickbuild is run with no arguments
"my_project" {
    run = "gcc foo.c";
    my_field = false;
    foo = "elem1", "elem2";
}
```
When Quickbuild evaluates a task, it will first look for a field called `depends`. For every element in this list (or string), it will attempt to either evaluate the task it's referring to or assert that the file required is present. The following task will assert that "foo.c" and "bar.c" are both present. Furhermore, this dependency list is what allows Quickbuild to determine whether a full recompilation is necessary or not. If parallelization is necessary, you should set the `depends_parallel` field to true in your desired task.
```
# this will only execute if the dependencies have changed
"another_project" {
    depends = "foo.c", "bar.c";
    depends_parallel = true;        # is set to false by default, but should be enabled when
                                      dependencies can be compiled in parallel
}
```

After all dependencies have been evaluated or found, Quickbuild looks for a field called `run`. This can either be a single string or a list of strings, which will be executed sequentially by your shell. If you don't want a task to execute a command, you can the `run` field blank. If you want the commands to execute in parallel, you can set the `run_parallel` field to true, similarly to how you would declare the parallelization of your dependencies.
```
"a_phony_task" {
    depends = some_other_stuff;
    run = "";
}
```

```
"a_complex_task" {
    run = "mkdir my_folders", "gcc my_files", "ld link_everything", "./install.sh", "./cleanup.sh";
}
```

Here's an example of a task being evaluated as a dependency.
```
my_deps = "foo.c";

"my_main_project" {
    depends = my_deps;
    run = "./my_output_binary"
}

my_deps {
    run = "gcc [my_deps]";
}
```

Finally, iterators can be used for tasks that apply to multiple values. These are essentially equivalent to $@ in Make.
```
my_files = "a.c", "b.c", "c.c";

"project" {
    depends = my_files;
    run = "./output";
}

my_files as current_source_file {
    run = "gcc -c [current_source_file]";
}

# this would execute:
# gcc -c a.c
# gcc -c b.c
# gcc -c c.c
# ./output
```

### Examples
All of these features are usually combined to create more powerful build scripts. There will eventually be some examples in the examples/ folder, but for now, you can check out the current Quickbuild config in this project or in some of the other projects currently powered by Quickbuild. Or, you can check out the comprehensive reference config that was originally used to boostrap Quickbuild:
```
# general compiler arguments
compiler = "clang++";
flags = "-g -O0 -Wall -Wextra -pthread -pedantic-errors";

# files to compile
sources = "src/*.cpp";
headers = "src/*.hpp";

# files to create
objects = sources: "src/*.cpp" -> "obj/*.o";
binary = "./bin/quickbuild";

# main task
"quickbuild" {
  depends = objects, headers;
  run = "[compiler] [flags] [objects] -o [binary]";
}

# object files
objects as obj {
  depends = obj: "obj/*.o" -> "src/*.cpp";
  run = "[compiler] [flags] -c [depends] -o [obj]";
}

# run
"run" {
  depends = "quickbuild";
  run = "[binary]";
}

# clean
"clean" {
  run = "rm [objects]",
        "rm [binary]";
}
```
