package main

import "os"

const (
	rounds   = int64(220000)
	mask     = int64(2147483647)
	expected = int64(1528465470)
)

var sinkCryptoBlockCipher int64

var keys = [8]int64{1267611, 2386093, 1059128, 5596791, 9022413, 3227993, 2562088, 4342338}

func rotl31(value int64, shift uint) int64 {
	return (((value << shift) & mask) | (value >> (31 - shift))) & mask
}

func main() {
	var acc int64
	for index := int64(0); index < rounds; index++ {
		left := ((index * 1103515) + 12345) & mask
		right := ((index * 2654435) + 54321) & mask
		for _, roundKey := range keys {
			mixed := (rotl31((left+roundKey+13)&mask, 5) ^ right) & mask
			nextRight := (mixed + ((right & 255) * 17) + roundKey) & mask
			left = right
			right = nextRight
		}
		acc = (acc + left + right + (left ^ right)) & mask
	}

	sinkCryptoBlockCipher = acc
	if sinkCryptoBlockCipher != expected {
		os.Exit(1)
	}
}
