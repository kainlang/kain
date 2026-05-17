package main

import "os"

const (
	packetCount    = 64
	wordsPerPacket = 4
	iterations     = int64(200000)
	modulus        = int64(1000000007)
	expected       = int64(924829641)
)

var sinkZeroCopyBinaryWire int64

func main() {
	var buffer [packetCount * wordsPerPacket]int64
	var acc int64
	for round := int64(0); round < iterations; round++ {
		for packet := 0; packet < packetCount; packet++ {
			packetValue := int64(packet)
			seq := (round * packetCount) + packetValue
			version := (packetValue % 4) + 1
			kind := ((packetValue * 3) + round) % 8
			flags := (round + packetValue) % 16
			route := ((packetValue * 5) + 7) % 64
			payload := ((seq * 13) + (route * 17) + 19) % 4096
			word0 := (seq * 4096) + (kind * 256) + (flags * 16) + version
			word1 := (payload * 128) + route
			word2 := ((seq % 97) * 2048) + ((payload % 127) * 16) + flags
			word3 := (word0 + word1 + word2 + 97) % 1000003
			base := packet * wordsPerPacket
			buffer[base+0] = word0
			buffer[base+1] = word1
			buffer[base+2] = word2
			buffer[base+3] = word3

			observed0 := buffer[base+0]
			observed1 := buffer[base+1]
			observed2 := buffer[base+2]
			observed3 := buffer[base+3]
			observedVersion := observed0 % 16
			observedFlags := (observed0 / 16) % 16
			observedKind := (observed0 / 256) % 16
			observedSeq := observed0 / 4096
			observedRoute := observed1 % 128
			observedPayload := observed1 / 128
			observedEpoch := observed2 / 2048
			acc = (acc + observedVersion + observedFlags + observedKind + (observedSeq % 97) + observedRoute + observedPayload + observedEpoch + observed3) % modulus
		}
	}

	sinkZeroCopyBinaryWire = acc
	if sinkZeroCopyBinaryWire != expected {
		os.Exit(1)
	}
}
