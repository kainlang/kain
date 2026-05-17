package main

import (
	"math"
	"os"
)

type ray struct {
	originX    float64
	originY    float64
	originZ    float64
	directionX float64
	directionY float64
	directionZ float64
}

type sphere struct {
	centerX float64
	centerY float64
	centerZ float64
	radius  float64
}

func seededRay(index int64) ray {
	originX := -4.0 + float64(index)*0.31
	originY := -1.5 + float64(index%4)*0.45
	originZ := -6.0 + float64(index%3)*0.55
	directionX := 0.2 + float64(index%5)*0.07
	directionY := -0.1 + float64(index%3)*0.08
	directionZ := 1.0 + float64(index%4)*0.05
	length := math.Sqrt(directionX*directionX + directionY*directionY + directionZ*directionZ)
	return ray{
		originX:    originX,
		originY:    originY,
		originZ:    originZ,
		directionX: directionX / length,
		directionY: directionY / length,
		directionZ: directionZ / length,
	}
}

func seededSphere(index int64) sphere {
	return sphere{
		centerX: -1.8 + float64(index)*0.63,
		centerY: -0.7 + float64(index%3)*0.58,
		centerZ: 2.4 + float64(index)*0.71,
		radius:  0.75 + float64(index%4)*0.17,
	}
}

func hitDistance(rayValue ray, sphereValue sphere) float64 {
	localX := rayValue.originX - sphereValue.centerX
	localY := rayValue.originY - sphereValue.centerY
	localZ := rayValue.originZ - sphereValue.centerZ
	a := rayValue.directionX*rayValue.directionX + rayValue.directionY*rayValue.directionY + rayValue.directionZ*rayValue.directionZ
	b := 2.0 * ((localX * rayValue.directionX) + (localY * rayValue.directionY) + (localZ * rayValue.directionZ))
	c := (localX * localX) + (localY * localY) + (localZ * localZ) - (sphereValue.radius * sphereValue.radius)
	discriminant := (b * b) - (4.0 * a * c)
	if discriminant < 0.0 {
		return -1.0
	}
	root := math.Sqrt(discriminant)
	nearHit := (-b - root) / (2.0 * a)
	if nearHit > 0.001 {
		return nearHit
	}
	farHit := (-b + root) / (2.0 * a)
	if farHit > 0.001 {
		return farHit
	}
	return -1.0
}

const (
	iterations = int64(150000)
	modulus    = int64(1000000007)
	expected   = int64(48999657)
)

var sinkRaySphereIntersection int64

func main() {
	var rays [12]ray
	var spheres [8]sphere
	for index := 0; index < len(rays); index++ {
		rays[index] = seededRay(int64(index))
	}
	for index := 0; index < len(spheres); index++ {
		spheres[index] = seededSphere(int64(index))
	}

	var acc int64
	for round := int64(0); round < iterations; round++ {
		phase := round % 11
		for rayIndex := 0; rayIndex < len(rays); rayIndex++ {
			for sphereIndex := 0; sphereIndex < len(spheres); sphereIndex++ {
				distance := hitDistance(rays[rayIndex], spheres[sphereIndex])
				if distance > 0.0 {
					bucket := int64(math.Floor(distance * 128.0))
					acc = (acc + bucket + int64(rayIndex)*17 + int64(sphereIndex)*31 + phase) % modulus
				} else {
					acc = (acc + int64(rayIndex) + int64(sphereIndex) + 3) % modulus
				}
			}
		}
	}

	sinkRaySphereIntersection = acc
	if sinkRaySphereIntersection != expected {
		os.Exit(1)
	}
}
