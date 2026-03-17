## Foreign Function Interface (FFI)

The Ry FFI allows the virtual machine to interface with native C++ libraries (`.so` on Linux, `.dll` on Windows). This is how Ry achieves high-performance system operations like file I/O and networking.

## Using Native Libraries
To bring a native library into your script, use the `use()` function. This turns the compiled object into a Ry map data type.

```python
# Loading a native binary
data native = use("libry_file.so")
```

## Creating a Wrapper (Recommended for Library Development)

Architecturally, it is best practice to wrap native calls inside a namespace. This allows you to handle safety checks and panic states before the VM executes the core logic.

**Example: File System Wrapper**
```python
# file.ry
data native = use("libry_file.so")

namespace File {
    func read(data path) {
        data result = native.read(path) 
        if result == null {
            panic "File Not Found."
        }
        return result
    }

    func write(data path, data content) {
        data success = native.write(path, content)
        if !success {
            panic "Failed to write to file '" + path + "'."
        }
        return success
    }
}
```

## The "Native" Logic
When you call a function on a native object, Ry's register-based VM will:

1. Locate a `ry_init_module` function in the `.so` or `.dll`
2. Turns functions declared by the `ry_init_module` into a ry map.
3. Returns the map into a `data` variable.

## Error Handling
If a native function fails or is not found within the binary, Ry will trigger a traceback. This ensures that even "black box" native code respects the language's focus on **Beautiful Error Reporting**.

## Native Implementation (C++)

To be compatible with Ry, the native library must export a `ry_init_module` symbol. This function is responsible for registering native methods into the Ry environment.

```cpp
extern "C" void init_ry_module(RyRegisterFn reg, void *target) { // Using the exact `RyRegisterFn reg, void *target` parameters is required since the vm needs them

// Your may use the `reg` "function" paramter to create a function for ry like the example below:
reg("examples_function", cpp_function_name, number_of_parameters, target);
}
```

**Architectural Requirements:**
- **Symbol Export**: Use `extern "C"` to prevent C++ name mangling so the VM can locate the symbol.
- **Compilation**: Compiling with the `-shared` `-fPIC` flags is recommended:
```bash
$ g++ -shared -fPIC -o libry_file.so file_wrapper.cpp
```
- **Memory Safety**: Since you are operating in C++ core territory, ensure your native functions validate the void *target and return valid Ry types to avoid crashing the VM.
- **The RyLibDk**: Use the official `rylib-dk.h` inside the `lib-dk` folder to avoid segmentation crashes and more low level crashes.