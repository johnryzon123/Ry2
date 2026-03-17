## Basic Syntax

### Variables and Data
In Ry we use the `data` keyword to define information. It's clear, direct, and tells the VM exactly what it's dealing with.

```python
data name = "John Smith"
data age = 20
```
You can use the `alias` keyword to create shorcuts for existing types or valus, keeping your code organized.

### Logic: The `Unless` and `Until` Advantage

Ry is designed to read like a sectence. Instead of a messy `!`(NOT) operators everywhere, we use `unless` and `do-until`.

**The `unless` Keyword**:
`unless` is the opposite of `if`. It runs the code only if the condition is false.

```python
data battery_low = false
unless battery_low {
  out("Keep coding!")
}
```

**The `do-until` keyword**
`do-until` is the opposite of `while`. It runs the code until the condition is true.

```python
data progress = 0
do {
  ++progress
  out(progress)
} until progress == 100
```

### Loops: `foreach` and `while`
Ry makes counting, iterating and looping simple. You don't need simple C-style loops for basic tasks.

**The `foreach` loop**

`foreach` is a loop used for iterating variables. It runs the code until it goes beyond the variables length.

**Couting with `to`**
```python
foreach data i in 0 to 10 {
  out(i)
}
```

**Iterating with `in`**
```python
data projects = ["Ry2", "Ryder"]
foreach data project in projects {
  out("Working on ${project}")
}
```

**The `while` loop**

`while` is a loop that runs until the condition is false.

```python
data isRunning = true
while isRunning {
  data name = input("Say your Name or type exit: ")
  if name == "exit" {
    isRunning = false
  } else {
    out("Hello ${name}")
  }
}
```

### Error Catching
Ry uses `attempt` and `fail` to catch errors instead of the default `try` and `catch`.

```python
attempt {
  panic "help" # This code will not run
} fail err {
  out("Caught Error: ${err}")
}
```

### Functions
Ry uses the `func` keyword to create functions. Functions help organize code by putting them into blocks and calling them later.

```python
func greet(data name) {
  out("Hello ${name}")
}
greet("Ryzon")
# Prints: Hello Ryzon
```

[Previous](README.md) | [Next](docs/03_Classes.md)

