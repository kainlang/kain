package main

import "os"

const (
	entityCount = 32
	iterations  = int64(350000)
	modulus     = int64(1000000007)
	expected    = int64(886666628)
)

var sinkECSArchetypeQuery int64

func main() {
	var positionX [entityCount]int64
	var positionY [entityCount]int64
	var velocityX [entityCount]int64
	var velocityY [entityCount]int64
	var health [entityCount]int64
	var team [entityCount]int64
	var active [entityCount]bool

	for index := 0; index < entityCount; index++ {
		i := int64(index)
		positionX[index] = ((i * 17) % 97) + 3
		positionY[index] = ((i * 29) % 89) + 5
		velocityX[index] = ((i * 7) % 11) + 1
		velocityY[index] = ((i * 5) % 13) + 2
		health[index] = ((i * 19) % 41) + 9
		team[index] = i % 4
		active[index] = (i % 3) != 1
	}

	var acc int64
	for round := int64(0); round < iterations; round++ {
		roundPhase := round % 5
		roundBias := round % 7
		for lane := 0; lane < entityCount; lane++ {
			laneValue := int64(lane)
			if active[lane] && health[lane] > ((round+laneValue)%11) {
				motion := positionX[lane] + velocityX[lane]*(roundPhase+1)
				support := positionY[lane] + velocityY[lane]*((roundBias%3)+2)
				if ((team[lane] + round + laneValue) % 3) == 0 {
					acc = (acc + motion + support + health[lane] + laneValue) % modulus
				} else {
					acc = (acc + motion + (support * 2) + team[lane] + 17) % modulus
				}
			} else {
				acc = (acc + team[lane] + laneValue + 23) % modulus
			}
		}
	}

	sinkECSArchetypeQuery = acc
	if sinkECSArchetypeQuery != expected {
		os.Exit(1)
	}
}
