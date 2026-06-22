package main

import (
	"crypto/rand"
	"encoding/hex"
	"sync"
)

// Matchmaker manages the set of active rooms and handles room creation,
// lookup, and the public match queue. This is a minimal stub implementation
// that Task 4 will replace with full matchmaking logic.
type Matchmaker struct {
	mu    sync.RWMutex
	rooms map[string]*Room
}

// NewMatchmaker creates a new Matchmaker with an empty room set.
func NewMatchmaker() *Matchmaker {
	return &Matchmaker{
		rooms: make(map[string]*Room),
	}
}

// CreateRoom creates a new room, registers it with the matchmaker, and
// returns it. The private flag is currently advisory; Task 4 will enforce
// private room visibility rules.
func (m *Matchmaker) CreateRoom(private bool) *Room {
	room := &Room{
		ID:      generateRoomID(),
		Private: private,
	}
	m.mu.Lock()
	m.rooms[room.ID] = room
	m.mu.Unlock()
	return room
}

// GetRoom looks up a room by ID. Returns ok=false if not found.
func (m *Matchmaker) GetRoom(id string) (*Room, bool) {
	m.mu.RLock()
	room, ok := m.rooms[id]
	m.mu.RUnlock()
	return room, ok
}

// Room represents a single game room holding two players. This is a minimal
// stub implementation; Task 4 will add full room state, ready coordination,
// frame-sync relay, and anti-cheat.
type Room struct {
	ID      string
	Private bool

	mu      sync.Mutex
	clients []*Client
}

// Join adds a client to the room.
func (r *Room) Join(c *Client) {
	r.mu.Lock()
	r.clients = append(r.clients, c)
	r.mu.Unlock()
}

// Leave removes a client from the room. Task 4 will notify the remaining
// peer and tear down the room when empty.
func (r *Room) Leave(c *Client) {
	r.mu.Lock()
	for i, cl := range r.clients {
		if cl == c {
			r.clients = append(r.clients[:i], r.clients[i+1:]...)
			break
		}
	}
	r.mu.Unlock()
}

// SetReady marks a client's ready state. Task 4 will coordinate match start
// once both players are ready.
func (r *Room) SetReady(c *Client, ready bool) {
	// Stub: ready state is tracked in Task 4.
}

// HandleInput relays a client input frame. Task 4 will forward it to the peer
// and run input validation / anti-cheat checks.
func (r *Room) HandleInput(c *Client, d InputData) {
	// Stub: relay implemented in Task 4.
}

// HandleState processes a state hash from the authoritative client. Task 4
// will compare hashes between peers and emit state_ack responses.
func (r *Room) HandleState(c *Client, d StateData) {
	// Stub: state validation implemented in Task 4.
}

// generateRoomID returns a short random hex ID suitable for room codes.
func generateRoomID() string {
	b := make([]byte, 4)
	if _, err := rand.Read(b); err != nil {
		return "00000000"
	}
	return hex.EncodeToString(b)
}
