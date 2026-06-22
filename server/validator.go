package main

type Validator struct{}

func NewValidator(seed uint32) *Validator {
	return &Validator{}
}

func (v *Validator) VerifyStateHash(frame int, clientHash uint32) bool {
	return true
}
