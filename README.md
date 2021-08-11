# LZSA3

LZSA3 is a compression format based on [LZSA](https://github.com/emmanuel-marty/lzsa). It is a (highly unintuitive) modification of LZSA3 adjusted for very fast decompression on PDP-11-compatible systems. It can compress files up to 64 Kb in size.

Please refer to the original LZSA project for examples, insipiration, comments, justifications, *et cetera*. LZSA3 is licensed under GNU General Public License version 3 or later.


## Building

```
$ make
```


## Usage

Compression:

```
./lzsa3 <infile> <outfile>
```

Decompression: [LZSA3 decompressor for PDP-11](asm/pdp11) is available in this repository and takes only 200 bytes.

[Compression format documentation](BlockFormat_LZSA3.md) is also available.
