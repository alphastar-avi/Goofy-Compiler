
# **Cookie Compiler**

It all started with C++. As we were learning language, we found ourselves wondering—how do compilers like GCC and Clang actually work? That curiosity led us to a simple yet ambitious idea: why not build a compiler ourselves?

Initially, the goal was just to understand how compilers function from the ground up. But as we kept working, the project naturally evolved—not only were we building a compiler, but we were also designing a programming language. We wanted Cookie to be simple, intuitive, and easy to use, making it accessible to anyone interested in programming.

---

## **Prerequisites**

Ensure you have the following installed:

- **LLVM**: Version 19.0 or later
- **Flex** and **Bison**
- **GCC / G++**
- **Docker** (optional, but let’s be real you’ll probably end up using it anyway)

---

## **Docker Pull & Run (Easier approach)**
Install the Docker Desktop: https://www.docker.com/products/docker-desktop/

If you have Docker installed, you can compile and run Cookie without summoning the dependency demons...

### **Pull the Image** 
```bash
docker pull alphastar59/cookie:latest
```

### **Running the Container**
In the same directory
Open your preferred text editor and save your cookie code in the format `filename.cook` and run

```bash
docker run --rm -it -v "$(pwd)/filename.cook":/cookie_dir/filename.cook cookie
```

You'll get the output of your code :)

---

## **Repository**

Clone the repository using:

```bash
git clone https://github.com/alphastar-avi/Cookie-lang.git
```
After cloning, make sure your PATH includes the necessary tool directories.

## **File Structure**

```
Cookie-Compiler/
├── filename.cook
├── lexer.l
├── parser.y
├── ast.h
├── ast.c
├── runtime.c
├── codegen.cpp
├── Dockerfile
└── cookie_extension/        // VS Code extension folder
    ├── package.json
    ├── tsconfig.json
    ├── language-configuration.json
    ├── syntaxes/
    │   └── langli.tmLanguage.json
    ├── src/
    │   └── extension.ts
    └── out/                 (generated after compilation)
```


## **Build Instructions**

### **For Mac & Linux**

1. **Build the Runtime Library:**
   ```bash
   gcc -shared -fPIC runtime.c -o libruntime.so
   ```

2. **Generate the Parser and Lexer Files:**
   ```bash
   bison -d parser.y
   flex lexer.l
   ```

3. **Compile the Source Files:**
   ```bash
   gcc -c ast.c -o ast.o
   gcc -c parser.tab.c -o parser.tab.o
   gcc -c lex.yy.c -o lex.yy.o
   gcc -c runtime.c -o runtime.o
   g++ -c codegen.cpp -o codegen.o $(llvm-config --cxxflags)
   g++ ast.o parser.tab.o lex.yy.o codegen.o runtime.o -o cookie $(llvm-config --ldflags --libs core)
   ```

4. **Run the Compiler:**
   To compile a Cookie source file (e.g., `filename.cook`):
   ```bash
   cat filename.cook | ./cookie > output.ll
   lli -load ./libruntime.so output.ll
   ```

5. **Clean Up Build Files:**
   ```bash
   rm -f ast.o parser.tab.c parser.tab.h parser.tab.o lex.yy.c lex.yy.o codegen.o cookie output.ll libruntime.so runtime.o
   ```

### **For Windows**

1. **Build the Runtime Library:**
   ```powershell
   gcc -shared -fPIC runtime.c -o libruntime.so
   ```

2. **Generate the Parser and Lexer Files:**
   ```powershell
   bison -d parser.y
   flex lexer.l
   ```

3. **Compile the Source Files:**
   ```powershell
   gcc -c ast.c -o ast.o
   gcc -c parser.tab.c -o parser.tab.o
   gcc -c lex.yy.c -o lex.yy.o
   gcc -c runtime.c -o runtime.o
   g++ -c codegen.cpp -o codegen.o $(llvm-config --cxxflags)
   g++ ast.o parser.tab.o lex.yy.o codegen.o runtime.o -o cookie $(llvm-config --ldflags --libs core)
   ```

4. **Run the Compiler:**
   ```powershell
   Get-Content .\filename.cook | .\cookie > output.ll
   lli -load ./libruntime.so output.ll
   ```

5. **Clean Up Build Files:**
   ```powershell
   Remove-Item -Force "ast.o", "parser.tab.c", "parser.tab.h", "parser.tab.o", "lex.yy.c", "lex.yy.o", "codegen.o", "cookie.exe", "output.ll", "libruntime.so", "runtime.o"
   ```

---

## **VS Code Extension**

Cookie also includes a VS Code extension to provide syntax highlighting, code-snippets & IntelliSense.

### **The repo already includes the .vsix complied package so you can install it simply by running:**
```bash
code --install-extension cookie_extension-7.0.0.vsix
```

### **If you wanna develope the extension by yourself:**

### **File Structure for the Extension**

```
cookie_extension/
├── package.json
├── tsconfig.json
├── language-configuration.json
├── syntaxes/
│   └── langli.tmLanguage.json
├── src/
│   └── extension.ts
└── out/               (generated after compilation)
```
### **Develop the features you want**

### **Development & Testing**

1. **Install Dependencies & Compile:**
   ```bash
   cd cookie_extension
   npm install
   npm run compile
   ```
2. **Launch Extension Development Host:**
   ```bash
   code .
   ```
   - Open the project in VS Code and press **F5**.
   - Create or open a file with the `.cook` extension to test syntax highlighting, code-snippets & IntelliSense.

3. **Packaging & Local Installation:**
   - Package the extension:
     ```bash
     npm install -g @vscode/vsce
     vsce package
     ```
     This generates a `.vsix` file.
   - Install the extension:
     ```bash    
     code --install-extension cookie_extension-<version>.vsix
     ```

     ---

### **We welcome all feature requests and bug reports, so feel free to open an issue. **

---
---

## Basic Syntax

### Print, Input, and Inline

- **print(expression);**
    
    Prints the evaluated value of the expression. If used as `print();`, it simply prints a new line.
    
    **Example:**
    
    ```
    print("Hello, Cookie!");
    print();  // Prints a new line.
    ```
    
- **input(expression);**
    
    Reads input from the user. The expression may serve as a prompt or an initializer for processing the input.
    
    **Example:**
    
    ```
    input("Enter your name: ");
    ```
    
- **inline(expression);**
    
    Evaluates the expression inline. This can be useful for immediate execution of a function or operation.
    
    **Example:**
    
    ```
    cook
    CopyEdit
    inline(2 + 3);
    
    ```
    

### Comments

Cookie supports two forms of single-line comments:

- Lines beginning with `//`
- Lines starting with `comment:`

Both types are ignored by the parser.

**Example:**

```
c
CopyEdit
// This is a single-line comment.
comment: This is another comment.

```

---

## Variable Declaration

Variables can be declared in two ways:

- **Implicit Declaration:** Using the `var` keyword.
- **Typed Declaration:** Using one of the specific data type keywords.

**Examples:**

```
var x = 10;          // Implicit declaration.
int y = 5;           // Declaration of an integer.
float pi = 3.14;     // Declaration of a floating-point number.

```

---

## 3. Data Types

Cookie has five primary data types with auto-detection when using literals:

- **int:** For integer values.
- **float:** For floating-point numbers.
- **bool:** For Boolean values (`true` or `false`).
- **char:** For single characters, enclosed in single quotes.
- **string:** For sequences of characters, enclosed in double quotes.

### Auto-detection & Type Conversions

- **Auto-detection:**
    
    When you write a literal (e.g., `123` or `"Hello"`), Cookie automatically detects its type.
    
- **Type Conversions:**
    
    Explicit conversion is available using a cast-like syntax:
    
    - `int(expression)`
    - `float(expression)`
    - `string(expression)`
    - `char(expression)`

These allow converting between compatible types.

**Example:**

```
int a = 10;
float b = 3.14;
bool flag = true;
char letter = 'c';
string greeting = "Hello";

// Converting a string to an int.
int num = int("123");

// Converting a string to a float.
float value = float("3.14");

```

---

## 4. Operators

### Arithmetic Operators

- **Addition:** `+`
- **Subtraction:** ``
- **Multiplication:** ``
- **Division:** `/`

**Example:**

```
int sum = 5 + 3;
```

### Relational Operators

- **Less than:** `lesser than` or `<`
- **Greater than:** `greater than` or `>`
- **Equals:** `equals` or `==`
- **Not equals:** `not equals` or `!=`
- **Less than or equals:** `lesser than equals` or `<=`
- **Greater than or equals:** `greater than equals` or `>=`

**Example:**

```
if (x lesser than 10) {
  print("x is less than 10");
}

if (y >= 3.14) {
  print("y is greater than or equal to 3.14");
}
```

### Logical Operators

- **And:** `and` or `&&`
- **Or:** `or` or `||`
- **Not:** `not` or `!`

**Example:**

```
if (flag and (x equals 5)) {
  print("Condition met!");
}
```

---

## 5. Control Structures

### Simple If

Executes a block if the condition is true.

**Syntax:**

```
if(condition) {
    // statements
}
```

**Example:**

```
if (x > 0) {
    print("Positive number");
}
```

### If Else

Provides an alternative block if the condition is false.

**Syntax:**

```
if (condition) {
    // if true
} else {
    // if false
}
```

**Example:**

```
if (x > 0) {
    print("Positive number");
} else {
    print("Zero or Negative");
}
```

### Else If Ladder

Chains multiple conditions.

**Syntax:**

**Example:**

```
if (x > 0) {
    print("Positive");
} else if (x < 0) {
    print("Negative");
} else {
    print("Zero");
}
```

### Switch Case

Evaluates an expression and executes code based on matching cases.

**Syntax:**

```
switch (expression) {
    case: value1
        // statements
    case: value2
        // statements
    default:
        // statements
}
```

**Example:**

```
switch (x) {
    case: 1
        print("One");
    case: 2
        print("Two");
    default:
        print("Other");
}
```

### Break Statement in Switch Case

Used to exit a switch-case block prematurely.

**Example:**

```
switch (x) {
    case: 1
        print("One");
        break;
    // further cases...
}
```

---

## 6. Arrays

Arrays are ordered collections of elements. In Cookie, arrays are declared with a specified type and support a built-in size function.

- **Declaration without initializer:**
    
    Specify the type, identifier, and size expression in brackets.
    
    **Example:**
    
    ```
    int numbers[10];
    ```
    
- **Declaration with initializer:**
    
    Use curly braces `{}` to list the elements.
    
    **Example:**
    
    ```
    int numbers[] = {1, 2, 3, 4, 5};
    ```
    

> Note: Array indexing in Cookie starts at 1 (i.e. the first element is at index 1).
> 

- **Size Function:**
    
    Use `size(array)` to get the number of elements.
    
    **Example:**
    
    ```
    int len = size(numbers);
    ```
    

---

## Loops

Cookie supports various loop constructs for fixed iteration, range-based traversal, and condition-based looping.

### Fixed Iteration Loop

Executes a block a fixed number of times.

**Syntax:**

```
loop (expression) {
    // statements
}
```

**Example:**

```
loop (5) {
    print("Iteration");
}
```

### Range-based Loop

Iterates over each element of an array.

**Syntax:**

```
loop identifier : array {
    // statements
}
```

**Example:**

```
loop i : numbers {
    print(i);
}
```

### Loop with Starting Index

By predefining a variable (e.g., *i*), you can start iterating from a specified index. This is useful for skipping initial elements.

**Example:**

```
int i = 2;
loop i : numbers {
    print(i);

```

In this example, iteration begins from the 2nd element of the array.

### Loop Until

Repeats the loop until the specified condition becomes true.

**Syntax:**

```
loop until (condition) {
    // statements
}
```

**Example:**

```
int x = 0;
loop until (x equals 10) {
    x = x + 1;
    print(x);
}
```

---

## 8. Functions

Functions in Cookie are defined using the `fun` keyword. They can either print a value directly or return a value for further use.

**Syntax:**

```
fun functionName(parameter_list) {
    // function body
    return expression;  // Optional, if a value is to be returned.
}
```

**Examples:**

- **Function That Prints a Value:**
    
    ```
    fun greet(name) {
        print("Hello, " + name);
    }
    ```
    
- **Function That Returns a Value:**
    
    ```
    fun add(a, b) {
        return (a + b);
    }
    ```