# Bomb Lab
This is bomb 373.
It belongs to nenw

## Solution
### Commands
^X + a					-> Toggle TUI mode
p /x *(int *)($rbp+0x4)	-> Print 0x4(%rbp)

### Prob1
Answer:
When a problem comes along, you must zip it!

### Prob2
Breakpoints:
`0x400f90`

Answer:
`1 3 8 19 42 89`

### Prob 3
Mechanism:
```
iVar1 = switch (local_l4 % 7)
	- 739 (case 0)
	- 0x1 (case 1)
	- 0xed (case 2)
	- 0x0 (case 3, 5, 7)
	- 0x363 (case 4, 6)

local_l0 = iVar1 - 0x363

iVar2 = switch (local_l4)
	- iVar1 - 0x360 (case 0)
	- iVar1 - 0x367 (case 1)
	- iVar1 - 0x35c (case 2)
	- iVar1 - 0x35e (case 3)

local_c = iVar2

local_l4, local_l0, local_c = 0, -128, -125
```

Answer:
`0 -128 -125`

### Prob 4
Mechanism:
```
local_l0, ?local_c = input(int, int)

assert local_10 < 14

uVar2 = func4(local_l0 as ulong, 0xf);
uVar3 = uVar2 as int
assert uVar2 = local_c[0]
```

Answer:
`13 15`

### Prob 5
Mechanism:
```
local_l0, ?local_c = input(uint, int)
local_l0 = local_l0 % 16

array = [0x0a, 0x02, 0x0e, 0x07, 0x08, 0x0c, 0x0f, 0x0b, 0x00, 0x04, 0x01, 0x0d, 0x03, 0x09, 0x06, 0x05]
iVar = 0
iVar2 = 0
do {
	iVar2++
	local_10 = array[local_10]
	iVar1 = iVar1 + local_10;
} while local_l0 != 0xf

assert iVar2 == 0xf
assert iVar1 == local_c[0]

# 15 <- 14 <- 2 <- 1 <- 10 <- 0 <- 8 <- 4 <- 9 <- 13 <- 11 <- 7 <- 3 <- 12 <- 5
```

Answer:
`5 115`

### Prob 6
Mechanism:
```
i1, i2, i3, i4, i5, i6 = input(int, int, int, int, int, int)

i = 0
# Each i1, ... i6 is less than 6 and different
while True:
	assert i{i} < 6
	i++
	
	if i == 6: break
	
	assert i{i_} !== $i{i_ + ?}

node = [68, 02, 00, 00, 01, 00, 00, 00, 00, 67, 96, 00, 00, 00, 00, 00]

j = 0
while j !== 6:
	k = 1;
	if 1 < i{j}:
		local_58[i{j}] = node1[i{j}]
	j += 1

0x401263

1 2 3 4 5 6
384, 580
431, 384


2 3 4 5 6 1
431, 384

3 4 5 6 1 2
83, 431
56, 83
417, 56

4 5 6 1 2 3
56, 83
417, 56

1 3 2 4 5 6
431, 580
384, 431
83, 384
56, 83
417, 56


first: next value
1 -> 580
2 -> 384
3 -> 431
4 -> 83
5 -> 56
6 -> 417

second: current value
1 3 6 2 4 5
```

Answer:
`1 3 6 2 4 5`