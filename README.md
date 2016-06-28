**[GMP Backend CPython is HERE](https://github.com/s-wakaba/bitpacked-cpython/tree/gmp_backend_long-3.5.2)**

#Bit Packed CPython

This modified implementation of CPython aims at reducing usage of normal, dynamic allocated PyObject instances to save memory and processing power.

Prevalent 64bit architecture machines allocate dynamic objects on their memory space with the fixed size alignment (typically 8 bytes).
It means that we can use the lower 3bit of object addresses as flag-bits for other proposes.

This implementation uses these bits of `PyObject*` to indicates **NOT PyObject pointer** and **Remaining 61bit represent its actual value**.

Only objects satisfying some conditions can be stored it into alternative 61bit space instead of normal PyObject.

* Small enough to store
* Immutable
* Not allowing direct access to inside of object members via C-API (e.g. `str, tuple` and buffer protocol objects like `bytes`)

##Building Interpreter
This has been tested only for x86_64 POSIX systems.
Add `--with-bitpacked` option when running `./configure` script and compile.
```
$ ./configure --with-bitpacked --prefix=/somewhere/to/install
$ make
$ make test
$ make install
```

##Supported Types
Now, objects of following types allow storeing with the **bitpacked** mode.

* `int` (Not big absolute value, `INT_MIN <= n <= INT_MAX`)
* `bool`
* `NoneType`
* `NotImplementedType`
* `float` (Not extremely big or small absolute value, appx. in range: `4.656e-10 < abs(val) < 8.589e9` and `0.0, +inf, -inf, nan`)
* `range` (Not big absolute values of start, end or step)

##Difference from normal CPython
Results of `id(bitpacked_obj)` are not indicate memory address but packed data structure.
```py
>>> id(None)
4
>>> id(False)
18
>>> hex(id(True))
'0x100000012' # 1<<32+18
>>> hex(id(0x12345))
'0x1234500000002'
>>> '%016x' % id(range(0x1122, 0x3344, 0x55))
'006733441122550c' # Length(16bit),End(16bit),Start(16bit),Step(8bit),Type-ID
```

Results of `is` operator with the same bit-packed values are True even if values come from different operations.

```py
>>> (3.0 * 4.0) is (2.0 * 6.0)
True
>>> range(0, 300, 4)[::3] is range(0, 600, 2)[:150:6]
True
```

Because of bit-packed objects have no dinamically allocated memory spaces, they have no reference counters.
`sys.getrefcount` returns a constant, dummy number when its argument is bit-packed.

```py
>>> from sys import getrefcount
>>> a = [12345]
>>> getrefcount(a[0])
8128 # dummy number
>>> b = a * 10000
>>> getrefcount(a[0])
8128 # not incremented
```

In particular case, memory consumption can be significantly saved.

```
# bit-packed
>>> from os import getpid
>>> from subprocess import call
>>> a = [0.1 * i for i in range(10000000)]
>>> call(['ps', 'u' ,'-p', str(getpid())])
USER       PID %CPU %MEM    VSZ   RSS TTY      STAT START   TIME COMMAND
username  7692  3.4  0.1 215156 86440 pts/15   S+   12:35   0:02 ./python

# normal
>>> a = [0.1 * i for i in range(10000000)]
>>> call(['ps', 'u' ,'-p', str(getpid())])
USER       PID %CPU %MEM    VSZ   RSS TTY      STAT START   TIME COMMAND
username  7695 10.3  0.4 452912 323560 pts/15  S+   12:37   0:02 python3
```

##License

This is licenesed on **PYTHON SOFTWARE FOUNDATION LICENSE VERSION 2**.

Have fun!
