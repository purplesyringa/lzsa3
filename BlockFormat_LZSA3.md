# LZSA3 archive format

LZSA3 archives are composed from consecutive commands. Each command follows this format, in this order:

* token: <XYZ|MMM|LL>
* optional extra literal length
* literal values
* match offset
* optional extra match length

**token**

The first byte is the token byte which is broken down into three parts:

    7 6 5 4 3 2 1 0
    X Y Z M M M L L

* L: 2-bit literals length (0-2, or 3 if extended). If the number of literals for this command is 0 to 2, the length is encoded in the token and no extra bytes are required. Otherwise, a value of 3 is encoded and extra nibbles or bytes follow as 'optional extra literal length'
* M: 3-bit match length (0-6, or 7 if extended). It only makes sense to consider match lengths greater than or equal to 2; hence the match lengths 2 to 8 are stored as 0 to 6, respectively. If the match length for this command is greater than 8, the value 7 is stored here and extra nibbles or bytes follow as 'optional extra match length'.
* XYZ: 3-bit value that indicates how to decode the match offset

**optional extra literal length**

If the literals length is 3 or more, the 'L' bits in the token form the value 3, and an extra nibble is read:

* 1-15: the value plus 2 is the final literals length
* 0: an extra byte follows

If an extra byte follows, it can have two possible types of value:

* 1-255: 17 is added to the value to compose the final literals length. For instance: a length of 206 will be stored as 3 in the token + a nibble with the value of 0 + a single byte with the value of 189.
* 0: a second and third byte follow, forming a big-endian 16-bit value. The final literals value is that 16-bit value. For instance, a literals length of 1027 is stored as 3 in the token, a nibble with the value of 0, then byte values of 0, 4 and 3, as 3 + (4 * 256) = 1027.

**literal values**

Literal bytes, whose number is specified by the literals length, follow here. There can be zero literals in a command.

**match offset**

The match offset is decoded according to the XYZ bits in the token:

    XYZ
    000 16-bit offset: the offset is stored in big-endian in the following two bytes.
    001 repeat offset: reuse the offset value of the previous match command.
    01Z 9-bit offset: read a byte for offset bits 1-8 and use the bit Z for bit 0 of the offset. After that, increment the offset.
    10Z 13-bit offset: read a nibble for offset bits 9-12. Use the bit Z for bit 8 of the offset, then read a byte for offset bits 0-7. After that, add 512 to the offset.
    11Z 5-bit offset: read a nibble for offset bits 1-4 and use the bit Z of the token as bit 0 of the offset. After that, increment the offset.

Unlike LZSA2, the match offset is (intuitively) positive: it is substracted from the current decompressed location and not added, in order to locate the back-reference to copy.

**optional extra match length**

If the match length is 9 or more, the 'M' bits in the token form the value 7, and an extra nibble is read:

* 1-15: the value plus 8 is the final match length.
* 0: an extra byte follows

If an extra byte follows here, it can have two possible types of value:

* 1-234, 236-255: add 23 to this value to compose the final match length. For instance: a length of 150 will be stored as 7 in the token + a nibble with the value of 0 + a single byte with the value of 127.
* 235: end of data marker.
* 0: a second and third byte follow, forming a big-endian 16-bit value. The final match length is that 16-bit value plus 2.


# End Of Data detection

The last command in the stream has a 9-bit match offset set to zero and a match length byte 235. The reason for such a strange marker is performance. In this case the final match length equals 258=256+2. Note that the last command may still contain a literal.


# Reading nibbles

When the specification indicates that a nibble (4 bit value) must be read:

* If there are no nibbles ready, read a byte immediately. Return the inversed high 4 bits (bits 4-7) as the nibble and store the low 4 bits (not inversed) for later. Flag that a nibble is ready for next time.
* If a nibble is ready, return the previously stored low 4 bits (bits 0-3) and flag that no nibble is ready for next time.
