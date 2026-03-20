# Contributing to Ry2 & RyzOS 🚀

First off, thanks for taking the time to contribute! Ry2 is a high-performance, register-based language built for the future **RyzOS** ecosystem. Whether you are fixing a bug in the VM or adding a new system library, your help is appreciated.

## How Can I Contribute?

### The Ry Standard Library
My standard library is mostly built purely in Ry! You can contribute by:
- Adding more algorithms.
- Optimizing existing functions (like improving the Taylor series precision).
- Creating new libraries like `time.ry`.

### Core VM Development (C++)
If you know C++, you can help optimize the core of Ry2:
- **New OpCodes**: Implementing new instructions in `chunk.h`, `compiler.cpp`, and `vm.cpp`.
- **Optimization**: Improving the **Direct Threading** dispatch or **Constant Folding** logic.
- **Memory Management**: Creating a garbage collector or upgrading register allocation.

### RyzOS Integration
I am currently working on porting Ry2 to **RyzOS**. We need help with:
- Writing C-wrappers for RyzOS syscalls.
- Implementing graphics primitives (`DrawRect`, `DrawPixel`) that Ry2 can call.
- ELF loading compatibility.

## Your First Pull Request
1. **Fork** the repository.
2. **Create a branch** for your feature (`git checkout -b feature/AmazingFeature`).
3. **Commit** your changes (`git commit -m 'Add some AmazingFeature'`).
4. **Push** to the branch (`git push origin feature/AmazingFeature`).
5. **Open a Pull Request**.

## Coding Standards
- **Ry Scripts**: Use `data` for variable declarations and `func` for functions.
- **C++ Core**: Keep it "Modern C++" but lightweight. Avoid heavy dependencies like Boost.
- **Documentation**: If you add a feature, please update the **Wiki**!

Let's build the fastest register-based ecosystem together!
