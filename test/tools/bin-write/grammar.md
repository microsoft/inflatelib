
# bin-write Grammar

## Output Mode
The bin-write program has 3 output modes that dictate how much data gets written per input:
* Binary: Input is a stream of bits that are written such that each successive bit is written to the least significant bit available in the next output byte.
Effectively this means that bits "appear" reversed in the output.
Inputs need not be divisible by 8, nor are they "padded" to be byte aligned.
For example, the input: `110010 10000001 10` would produce the the two bytes `0x53 0x60`, assuming the output is byte-aligned at the start of the input.
* 8-bit: Each input represents a single byte in the output.
Bit positions in the input do not change; any padding is applied to the most significant bits.
E.g. `1011` would become `0x0B` in the output.
Inputs greater than 8 bits are invalid.
* 16-bit: Same as the 8-bit write mode, however each input represents two bytes in the ouptut.
Bytes are written as little-endian (least significant byte first).

The output mode is changed by a right chevron character (`>`) followed by the size of the output mode, in bits (`1`, `8`, or `16`).
I.e. `>1`, `>8`, or `>16`.
When the output mode is changed from binary to either 8 or 16-bits, the output is first byte aligned by padding any remaining bits with zeroes.
The file starts out in 8-bit (byte) output mode.

## Input mode
The bin-write program has 3 input modes that dictate how input gets interpreted:
* Binary: Input is interpreted as base-2
* Decimal: Input is interpreted as base-10
* Hexadecimal: Input is interpreted as base-16

The input mode is changed by a left chevron character (`<`) followed by eiher `bin`, `dec`, or `hex` for binary, decimal, and hexadecimal respectively.
I.e. `<bin`, `<dec`, or `<hex`.

Additionally, a change in the output mode carries with it a change in the input mode as well as restrictions on which input modes are allowed:
* Binary output mode: Changes the input mode to binary; restricts the input mode to only be binary
* 8-bit output mode: Changes the input mode to hexadecimal; no restrictions on the input mode
* 16-bit output mode: Changes the input mode to hexadecimal; no restrictions on the input mode

## Repeats
Data can be repeated a specified number of times using `repeat(N){ data... }`.
The input/output modes at the start of `data...` are the same as they were prior to the `repeat` statement and revert back to what they were after the end of the `repeat` statement.
`data...` is allowed to change the input and output modes as much as it wishes, so long as the rules outlined above are followed.
Note that because of the byte alignment that occurs when the output switches from binary to something else, it's not guaranteed that the repeated data appear exactly the same in the output.

## Sequences
Sequences can be created by specifying `...` between two different input values.
The delta between each successive value is always +/- 1.
The "direction" of the sequence depends on whether or not the second value is greater or less than the first value.
For example, `1...5` will create the output sequence `1 2 3 4 5`.
Similarly, `5...1` would create the output sequence `5 4 3 2 1`.

When the output mode is binary, the number of bits in the first and second value must be identical.
For example, the input `000...111` would create the sequence `000 001 010 011 100 101 110 111`.
When converted into a bit sequence, this becomes `0xA0 0x9C 0xEE`.

## Comments
Comments are initiated by the `#` character and continue until the end of line is reached.
