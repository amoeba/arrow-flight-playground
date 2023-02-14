# arrow-flight-playground

Various examples related to [Apache Arrow Flight](https://arrow.apache.org/docs/format/Flight.html).

## Included Examples

- [DoPut](./basic-doput/): Basic example of the `DoPut` call


## Building & Running

I've set this up so each example has its own `CMakeLists.txt` and each of those builds assumes you have a version of Arrow C++ configured and build elsewhere on your system.

### Arrow C++

Note: This can be simplified if you're able to install a binary distribution of Apache Arrow system-wide (i.e., `brew install apache-arrow`).

Steps:

1. Set `ARROW_HOME` to a suitable location to install Arrow C++ to
2. Download or checkout a version of the Apache Arrow source
3. Make an out-of-source build directory inside the directory from (2) `cpp/build` and `cd` into it
4. Configure:
    ```sh
    cmake .. \
        -GNinja \
        -DCMAKE_INSTALL_PREFIX="$ARROW_HOME" \
        -DARROW_BUILD_INTEGRATION="OFF" \
        -DARROW_BUILD_STATIC="OFF" \
        -DARROW_BUILD_TESTS="OFF" \
        -DARROW_EXTRA_ERROR_CONTEXT="ON" \
        -DARROW_WITH_RE2="OFF" \
        -DARROW_WITH_UTF8PROC="OFF" \
        -DCMAKE_BUILD_TYPE="Debug" \
        -DARROW_FLIGHT="ON" \
        -DARROW_FILESYSTEM="ON" \
        -DARROW_PARQUET=ON
    ```
5. `ninja install`

### Each Example

Steps:

1. Set `ARROW_HOME` to where you've installed Arrow C++
2. Make an out-of-source build directory in `./build` and `cd` into it
    ```sh
    cmake .. \
        -GNinja \
        -DCMAKE_MODULE_PATH="$ARROW_HOME/lib/cmake/arrow"
    ```
3. `ninja`
4. Run `./client` and/or `./server` as appropriate
