package main

import (
	"crypto/rand"
	"encoding/hex"
	"sync"
)

type Matchmaker struct {
	mu    sync.RWMutex
	rooms map[string]*Room
}

func NewMatchmaker() *Matchmaker {
	return &Matchmaker{
		rooms: make(map[string]*Room),
	}
}

func (mm *Matchmaker) CreateRoom(private bool) *Room {
	id := generateRoomID()
	room := NewRoom(id, private)

	mm.mu.Lock()
	mm.rooms[id] = room
	mm.mu.Unlock()

	return room
}

func (mm *Matchmaker) GetRoom(id string) (*Room, bool) {
	mm.mu.RLock()
	room, ok := mm.rooms[id]
	mm.mu.RUnlock()
	return room, ok
}

func (mm *Matchmaker) RemoveRoom(id string) {
	mm.mu.Lock()
	delete(mm.rooms, id)
	mm.mu.Unlock()
}

func generateRoomID() string {
	b := make([]byte, 4)
	rand.Read(b)
	return hex.EncodeToString(b)
}
