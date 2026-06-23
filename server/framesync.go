package main

import "sync"

type FrameSync struct {
	mu        sync.Mutex
	inputs    [2]map[int]uint8
	nextFrame int
}

func NewFrameSync() *FrameSync {
	fs := &FrameSync{}
	fs.Reset()
	return fs
}

func (fs *FrameSync) Reset() {
	fs.mu.Lock()
	defer fs.mu.Unlock()

	fs.nextFrame = 0
	fs.inputs[0] = make(map[int]uint8)
	fs.inputs[1] = make(map[int]uint8)
}

func (fs *FrameSync) AddInput(player int, data InputData) {
	fs.mu.Lock()
	defer fs.mu.Unlock()

	if player < 0 || player > 1 {
		return
	}
	fs.inputs[player][data.Frame] = data.Input
}

func (fs *FrameSync) TryGetFrame() (frame int, inputs [2]uint8, ok bool) {
	fs.mu.Lock()
	defer fs.mu.Unlock()

	in0, has0 := fs.inputs[0][fs.nextFrame]
	in1, has1 := fs.inputs[1][fs.nextFrame]

	if !has0 || !has1 {
		return 0, [2]uint8{}, false
	}

	frame = fs.nextFrame
	inputs = [2]uint8{in0, in1}
	ok = true

	delete(fs.inputs[0], fs.nextFrame)
	delete(fs.inputs[1], fs.nextFrame)

	fs.nextFrame++
	return
}
