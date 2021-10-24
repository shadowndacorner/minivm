# Instructions

This page contains documentation on the implemented VM instructions.

## loadc
Loads a constant value into a given register.

```
loadc rN $constantName
```

`rN` is loaded into `arg2`, the ID that corresponds to `constantName` is loaded into `warg0`.

## addi
Adds the values of two signed integers stored in `rDest` and `rAdd` and stores the result in `rDest`.

```
adds rDest rAdd
```

`rDest` is loaded into arg0.  `rAdd` is loaded into arg1.

## printi
Prints a signed integer stored in `rN`.

```
printi rN
```

`rN` is loaded into `arg0`.