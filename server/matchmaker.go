package main

import (
	"crypto/rand"
	"encoding/hex"
	"sync"
)

type Matchmaker struct {
	mu          sync.RWMutex
	rooms       map[string]*Room
	store       *Store
	rankedQueue []PlayerClient
	tournaments map[string]*Tournament
}

func NewMatchmaker() *Matchmaker { return NewMatchmakerWithStore(nil) }

func NewMatchmakerWithStore(store *Store) *Matchmaker {
	return &Matchmaker{
		rooms:       make(map[string]*Room),
		store:       store,
		tournaments: make(map[string]*Tournament),
	}
}

func (mm *Matchmaker) CreateRoom(private bool) *Room {
	id := generateRoomID()
	room := NewRoom(id, private, mm)
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

func (mm *Matchmaker) JoinRankedQueue(c PlayerClient) {
	mm.mu.Lock()
	defer mm.mu.Unlock()
	for _, q := range mm.rankedQueue {
		if q == c {
			return
		}
	}
	mm.rankedQueue = append(mm.rankedQueue, c)
	if len(mm.rankedQueue) >= 2 {
		p1 := mm.rankedQueue[0]
		p2 := mm.rankedQueue[1]
		mm.rankedQueue = mm.rankedQueue[2:]
		room := NewRoom(generateRoomID(), false, mm)
		room.ranked = true
		mm.rooms[room.ID] = room
		room.Join(p1)
		p1.SetRoom(room)
		room.Join(p2)
		p2.SetRoom(room)
	}
}

func (mm *Matchmaker) LeaveRankedQueue(c PlayerClient) {
	mm.mu.Lock()
	defer mm.mu.Unlock()
	for i, q := range mm.rankedQueue {
		if q == c {
			mm.rankedQueue = append(mm.rankedQueue[:i], mm.rankedQueue[i+1:]...)
			break
		}
	}
}

func (mm *Matchmaker) CreateTournament(name string) *Tournament {
	id := generateRoomID()
	t := NewTournament(id, name, mm)
	mm.mu.Lock()
	mm.tournaments[id] = t
	mm.mu.Unlock()
	return t
}

func (mm *Matchmaker) GetTournament(id string) (*Tournament, bool) {
	mm.mu.RLock()
	t, ok := mm.tournaments[id]
	mm.mu.RUnlock()
	return t, ok
}
