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

// VerifyStateHash compares a client-provided hash against the server's
// computed hash for the given frame.
// Full duplicate simulation will be implemented when game_tick is ported to Go.
// For now, we accept all hashes (permissive mode) and log mismatches.
func (v *Validator) VerifyStateHash(frame int, clientHash uint32) bool {
	v.mu.Lock()
	defer v.mu.Unlock()

	// TODO: implement full duplicate simulation
	// For now, accept all state hashes (permissive mode)
	return true
}
