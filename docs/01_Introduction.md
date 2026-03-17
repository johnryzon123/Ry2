## Welcome to Ry
**Ry** (short for `Ry's for you`) is a high performance, register based scripting language. It is designed to be a tool --meaning it is built to be developed, compiled and easy to use.

Whether your building a simple script or a complex game like `Snake`, Ry is designed to be instituive and fast.

## The Phisolophy: Ry's for You
Unlike many other toy languages that depend on tools, Ry is purely built from c++ which has doesn't use any tools like flex, bison and more. Ry focuses on:

* **Independence**: Everything you need is on one repo
* **Purity**: Ry isn't a clone; it has it's own keywords (`unless`, `until`, `foreach`, `childof`) and it's own way of thinking
* **Performance by Design**: Using a register-based Virtual Machine and Direct Threading to make every instruction count.

## Technical Core
Ry is built with a professional 3-tier Architecture:

1. **The Backend+Frontend**: a custom Lexer and Parser that handles pure Ry Syntax
2. **The MiddleEnd**: A Bytecode Compiler and a "Pre-calculation" Optimizer that resolves logic during the parsing phase.
3. **The VM**: A high-speed execution engine featuring:
* **Register based Architecture**: Reducing stack churn and making instructions more effecient
* **Direct Threading**: Using a computed-goto dispatch table for elite performance.
* **Dynamic Loader**: A custom C++ extension system (`loader.cpp`) that lets Ry load native `.so` or `.dll` modules at runtime.

## The Ecosystem
Ry is more than just an interpreter; it is a full development environment:
* **Ryder**: The "Ry's Developing Editor," built in Godot, designed to eventually be powered by Ry itself ( Unreleased ).
* **Standard Library**: A collection of native modules like `types.ry`, `string.ry`, and `math.ry`
* **C++ Extensions**: Write high speed modules written in c++ for things like I/O and System Calls

[Previous](../README.md) | [Next](02_basics.md)