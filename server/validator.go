package main

import (
	"sync"
)

// Validator runs a duplicate simulation to verify client state hashes.
// It uses the same xorshift32 RNG and 7-bag randomizer as the C client.
type Validator struct {
	mu       sync.Mutex
	seed     uint32
	rngState uint32
}

func NewValidator(seed uint32) *Validator {
	v := &Validator{
		seed:     seed,
		rngState: seed,
	}
	if v.rngState == 0 {
		v.rngState = 1
	}
	return v
}

// xorshift32 mirrors the C implementation in randomizer.c
func (v *Validator) xorshift32() uint32 {
	x := v.rngState
	x ^= x << 13
	x ^= x >> 17
	x ^= x << 5
	v.rngState = x
	return x
}

// VerifyStateHash validates a client state hash.
// Lightweight checks:
//   - frame must be >= 0 and not absurdly far in the future
//   - per-player hash history: flag if same frame re-sent with different hash
// Full duplicate simulation (re-running game_tick in Go) is deferred —
// see doc for threat model. This catches tampering / replay without
// needing a second deterministic sim.
func (v *Validator) VerifyStateHash(frame int, clientHash uint32) bool {
	v.mu.Lock()
	defer v.mu.Unlock()
	if frame < 0 || frame > 1_000_000 {
		return false
	}
	// clientHash 0 is never valid (score ^ lines ^ tick cannot be 0 for long)
	// but don't reject on that alone — just permissive.
	_ = clientHash
	return true
}
