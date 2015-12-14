**[Bit Packed CPython is HERE](https://github.com/s-wakaba/bitpacked-cpython/tree/bitpacked-3.5.0)**

#GMP Backend CPython

This modified implementation of CPython uses GMP (The GNU Multiple Precision Arithmetic Library, see https://gmplib.org/) for the internal implementation of PyLong (default `int`) object.

GMP is a library focused high-performance calculation and there is an existing extension Python module using it.
In comparison with the module, this version of CPython does not require the installation of any additional modules and management for a special type of integer objects.

##Building Interpreter
This has been tested only for x86_64 and i386 POSIX systems.
This uses a GCC extensional compile option and it works GCC and compatible compilers including clang and Intel C Compiler.

###Building using system default GMP
Before building, please install GMP in your system.
Many systems support installation of GMP via default package repository: `gmp-devel` package (fedora), `libgmp-dev` package (ubuntu) and so on.
GMP version 4.1.3 and later are tested.

Add `--with-libgmp` option when running `./configure` script and compile.
```
$ ./configure --with-libgmp --prefix=/somewhere/to/install
$ make
$ make test
$ make install
```
The compiled interpreter links GMP using shared library.

###Building using GMP installed in specific path
Add `--with-libgmp=gmp_path` option when running `./configure` script and compile.
```
$ ./configure --with-libgmp=/somewhere/installed/gmp --prefix=/somewhere/to/install
$ make
$ make test
$ make install
```
The compiled interpreter links GMP using static library.

##Compatibility
Difference of behaviors between normal and modified version is little.
One of tiny difference is a value of `sys.int_info`.
```
>>> print(sys.int_info) # normal CPython in 64bit system
sys.int_info(bits_per_digit=30, sizeof_digit=4)
>>> print(sys.int_info) # modified CPython in 64bit system
sys.int_info(bits_per_digit=64, sizeof_digit=8, gmp_version=(6, 1, 0))
```
If you use some extension module which depends on internal implementation of PyLong object, it might not work properly.

##Performance
Performance of programs which require huge integer calculations can dramatically improve.
In the following case, the modified version is 4 times faster than the normal version.
```
$ cat bigintcalc.py 
from random import randrange
from sys import int_info
a = [sum(randrange(10) * (10 ** i) for i in range(randrange(400, 500))) for i in range(100)]
b = sum((x+1111111111) * (y+2222222222) * (z+3333333333) for x in a for y in a for z in a)
print('the result has %d digits' % len(str(b)))
print(int_info)

$ time ./python bigintcalc.py # normal CPython in 64bit system
the result has 1497 digits
sys.int_info(bits_per_digit=30, sizeof_digit=4)

real	0m15.149s
user	0m15.106s
sys	0m0.011s

$ time ./python bigintcalc.py # modified CPython in 64bit system
the result has 1498 digits
sys.int_info(bits_per_digit=64, sizeof_digit=8, gmp_version=(6, 1, 0))

real	0m3.650s
user	0m3.636s
sys	0m0.005s
```


##License

This is licensed on **PYTHON SOFTWARE FOUNDATION LICENSE VERSION 2**.

Have fun!
