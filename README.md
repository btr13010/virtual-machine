# LC-3 Virtual Machine

Midterm Project ELEC2030

**Description**: A simple LC-3 virtual machine written in C. The machine simulates how the CPU works inside a computer to execute an image program (compiled assembly program). The completed program is then tested with the provided 2048, hangman, and rogue game files.

**How to run**:

There are three functions in the utils.c file that are operating system dependent. You need to refer to this file and uncomment the functions corresponding to your OS before compiling the program.

Compile the program:
```bash
make main
```
Run 2048 game:
```bash
./main ./games/2048.obj
```
Run rogue game:
```bash
./main ./games/rogue.obj
```
Run hangman game:
```bash
./main ./games/hangman.obj
```

**Contributors**: Tran Quoc Bao, Tran Huy Hoang Anh, Pham Anh Quan
