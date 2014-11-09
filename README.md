#Bit Packed CPython

This modified implementation of CPython aims at reducing usage of normal, dynamic allocated PyObject instances to save memory and processing power.

Prevalent 64bit architecture machines allocate dynamic objects on their memory space with the fixed size alignment (typically 8 bytes).
It means that we can use the lower 3bit of object addresses as flag-bits for other proposes.

This implementation uses these bits of `PyObject*` to indicates **NOT PyObject pointer** and **Remaining 61bit represent its actual value**.

Only objects satisfying some conditions can be stored it into alternative 61bit space instead of normal PyObject.

* Small enough to store
* Immutable
* Not allowing direct access to inside of object via C-API (e.g. buffer protocol objects)

Now, objects of following types allow storeing with the **bitpacked** mode.

* `NoneType`
* `NotImplementedType`
* `float` (Not extremely big or small absolute value)
* `range` (Not big absolute values of start, end or step)

I have plan to additional support of following types.

* `int` (Not big absolute value)
* `bool`
* `store` (Short and using limited charactor set)
* `slice`

Have fun!
