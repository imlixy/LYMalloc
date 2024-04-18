Follow these steps to compile and run the project:

1. **Unzip the Code**:
   - Unzip the provided code package. Inside, you should find a directory called `src` that contains two `.h` files, three `.c` files, and a `Makefile`. The file structure within the `src` directory is as follows:
     ```
     src
     ├── benchmark.c
     ├── benchmark.h
     ├── LYMalloc.c
     ├── LYMalloc.h
     ├── main.c
     └── Makefile
     ```

2. **Change Directory**:
   - Open your terminal and change to the `src` directory by entering the following command:
     ```
     cd src
     ```

3. **Compile the Project**:
   - Compile the project by running the `make` command in the terminal:
     ```
     make
     ```

4. **Run the Program**:
   - Execute the compiled program using the following command format:
     ```
     ./allocator x y z
     ```
     Where:
     - `x=1` to use the standard `malloc` function.
     - `x=2` to use the custom `LYMalloc` allocator.
     - `y` is the number of threads to be used.
     - `z` is the number of allocations to perform.

   Example command to run the program with the standard malloc, using 4 threads and performing 1000 allocations:
   ```
   ./allocator 1 4 1000
   ```