## Object Oriented Programming

Ry uses classes to group data and functions together. This makes it easy to build complex things like characters in a game or UI elements in **Ryder**.

### Creating a Class

You can use the `class` keyword to create a class. Variables inside a class is called `fields` and functions are called `methods`.
Constructors are functions that take initialize the class you can create a constructor by creating an `init` function.

```python
class Player {
  data name
  data health = 100

  func greet() {
    out("Hello ${this.name}")
  }
  func init(data name) {
    this.name = name
  }
}

data p1 = Player("Ryzon")
p1.greet()
# Prints: Hello Ryzon
```

### Inheritance with `childof`

Instead of `extends` or `:`, Ry uses `childof`. This makes it clear that one class is a child of another.

```python
class Warrior childof Player {
  data shield = 10

  func defend() {
    out("Defending with ${this.shield} shield")
  }
}
```

### The `parent` keyword

If you wanna call a functions or access data from the parent class, you can use the `parent` keyword.

```python
class SuperWarrior childof Warrior {
  func greet(this) {
    parent.greet() # Calls Player's greet
    out("...and I am a SUPER warrior!")
  }
}
```

### Static Fields and Methods
These belong to the class itself, not the instance, use the `static` keyword to make them.

```python
class Player {
  static name = "Ryzon"

  static func greet() {
    out("Hello ${this.name}")
  }
}

Player.greet()
# Prints: Hello Ryzon
```

### Private Fields and Methods
`private` is a keyword for making fields and methods only accessible inside the class and throws an error if you try to access them outside.

```python
class Player {
  private name = "Ryzon"

  func getName() {
    return this.name
  }
}

data player = Player()
out(player.getName())
# Prints: Ryzon
# out(player.name) Throws an error
```

[Previous](02_basics.md) | [Next](04_FFI.md)