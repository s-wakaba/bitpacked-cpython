#Bit Packed CPython

This modified implementation of CPython aims at reducing usage of normal, dynamic allocated PyObject instances to save memory and processing power.

Prevalent 64bit architecture machines allocate dynamic objects on their memory space with the fixed size alignment (typically 8 bytes).
It means that we can use the lower 3bit of object addresses as flag-bits for other proposes.

This implementation uses these bits of `PyObject*` to indicates **NOT PyObject pointer** and **Remaining 61bit represent its actual value**.

Only objects satisfying some conditions can be stored it into alternative 61bit space instead of normal PyObject.

* Small enough to store
* Immutable
* Not allowing direct access to inside of object members via C-API (e.g. `str, tuple` and buffer protocol objects like `bytes`)

##Supported Types
Now, objects of following types allow storeing with the **bitpacked** mode.

* `int` (Not big absolute value)
* `bool`
* `NoneType`
* `NotImplementedType`
* `float` (Not extremely big or small absolute value, appx. in range: `4.656e-10 < abs(val) < 8.589e9` and `0.0, +inf, -inf, nan`)
* `range` (Not big absolute values of start, end or step)

##Difference from normal CPython
Results of `id(bitpacked_obj)` are not indicate memory address but packed data structure.
```py
>>> id(None)
2
>>> id(3.14159265) % 8
6
>>> '%016x' % id(range(0x1122, 0x3344, 0x55))
'0067334411225512' # Length(16bit),End(16bit),Start(16bit),Step(8bit),Type-ID
```

Results of `is` operator with the same bit-packed values are True even if values come from different operations.

```py
>>> (3.0 * 4.0) is (2.0 * 6.0)
True
>>> range(0, 300, 4)[::3] is range(0, 600, 2)[:150:6]
True
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
