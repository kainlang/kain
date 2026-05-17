package main

import "os"

const (
	kernelCount = int64(64)
	iterations  = int64(1800000)
	modulus     = int64(1000000007)
	expected    = int64(185456717)
)

var sinkDynamicVtableThrashing int64

type kernel interface {
	score(value int64) int64
}

type addKernel struct{ bias int64 }
type multiplyKernel struct{ bias int64 }
type modKernel struct{ bias int64 }
type squareKernel struct{ bias int64 }
type biasSquareKernel struct{ bias int64 }
type foldKernel struct{ bias int64 }
type expandKernel struct{ bias int64 }
type xorKernel struct{ bias int64 }

func (k addKernel) score(value int64) int64        { return value + (k.bias * 3) + 7 }
func (k multiplyKernel) score(value int64) int64   { return (value * (k.bias + 5)) + 11 }
func (k modKernel) score(value int64) int64        { return ((value + k.bias) % 257) + (k.bias * 13) }
func (k squareKernel) score(value int64) int64     { return (value * value) + (k.bias * 17) + 3 }
func (k biasSquareKernel) score(value int64) int64 { return (value * 9) + (k.bias * k.bias) + 19 }
func (k foldKernel) score(value int64) int64       { return (((value + 31) * (k.bias + 7)) % 4099) + 23 }
func (k expandKernel) score(value int64) int64     { return (value * 5) + ((k.bias + 1) * 29) }
func (k xorKernel) score(value int64) int64        { return ((value * 7) ^ (k.bias * 41)) + 37 }

func makeKernel(kind int64, bias int64) kernel {
	switch kind {
	case 0:
		return addKernel{bias: bias}
	case 1:
		return multiplyKernel{bias: bias}
	case 2:
		return modKernel{bias: bias}
	case 3:
		return squareKernel{bias: bias}
	case 4:
		return biasSquareKernel{bias: bias}
	case 5:
		return foldKernel{bias: bias}
	case 6:
		return expandKernel{bias: bias}
	default:
		return xorKernel{bias: bias}
	}
}

func main() {
	kernels := make([]kernel, 0, kernelCount)
	for slot := int64(0); slot < kernelCount; slot++ {
		kind := ((slot * 5) + 3) % 8
		bias := ((slot * 17) % 23) + 1
		kernels = append(kernels, makeKernel(kind, bias))
	}

	var acc int64
	for index := int64(0); index < iterations; index++ {
		slot := index % kernelCount
		value := ((index * 13) + 7) % 1009
		score := kernels[int(slot)].score(value)
		acc = (acc + score + slot) % modulus
	}

	sinkDynamicVtableThrashing = acc
	if sinkDynamicVtableThrashing != expected {
		os.Exit(1)
	}
}
