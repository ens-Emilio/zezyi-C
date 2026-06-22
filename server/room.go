package main

import (
	"log"
	"math/rand"
	"sync"
)

const (
	RoomMaxPlayers = 2
)

type Player struct {
	client *Client
	ready  bool
}

type Room struct {
	mu        sync.Mutex
	ID        string
	Private   bool
	players   []*Player
	sync      *FrameSync
	validator *Validator
	started   bool
}

func NewRoom(id string, private bool) *Room {
	return &Room{
		ID:      id,
		Private: private,
		sync:    NewFrameSync(),
	}
}

func (r *Room) Join(c *Client) {
	r.mu.Lock()
	defer r.mu.Unlock()

	if len(r.players) >= RoomMaxPlayers {
		c.sendError("room is full")
		return
	}

	// Leave previous room if any
	if c.room != nil && c.room != r {
		c.room.Leave(c)
	}

	r.players = append(r.players, &Player{client: c, ready: false})

	log.Printf("room %s: %s joined (%d/%d)",
		r.ID, c.playerName, len(r.players), RoomMaxPlayers)

	r.broadcastRoomState()
}

func (r *Room) Leave(c *Client) {
	r.mu.Lock()
	defer r.mu.Unlock()

	for i, p := range r.players {
		if p.client == c {
			r.players = append(r.players[:i], r.players[i+1:]...)
			break
		}
	}

	log.Printf("room %s: %s left (%d/%d)",
		r.ID, c.playerName, len(r.players), RoomMaxPlayers)

	if r.started {
		for _, p := range r.players {
			p.client.sendJSON(Message{
				Type: MsgEnd,
				Data: mustMarshal(EndData{
					Winner: "",
					Reason: "disconnect",
				}),
			})
		}
		r.started = false
	}

	if len(r.players) == 0 {
		r.started = false
	}
}

func (r *Room) SetReady(c *Client, ready bool) {
	r.mu.Lock()
	defer r.mu.Unlock()

	for _, p := range r.players {
		if p.client == c {
			p.ready = ready
			break
		}
	}

	if !r.started && len(r.players) == RoomMaxPlayers {
		allReady := true
		for _, p := range r.players {
			if !p.ready {
				allReady = false
				break
			}
		}
		if allReady {
			r.startMatch()
		}
	}

	r.broadcastRoomState()
}

func (r *Room) startMatch() {
	r.started = true
	r.sync.Reset()

	seed := uint32(rand.Int31())
	if seed == 0 {
		seed = 1
	}

	opponentNames := make([]string, len(r.players))
	for i, p := range r.players {
		opponentNames[i] = p.client.playerName
	}

	for i, p := range r.players {
		opp := opponentNames[1-i]
		p.client.sendJSON(Message{
			Type: MsgStart,
			Data: mustMarshal(StartData{
				Seed:     seed,
				Mode:     0,
				Opponent: opp,
				RoomID:   r.ID,
			}),
		})
	}

	log.Printf("room %s: match started (seed=%d)", r.ID, seed)
}

func (r *Room) HandleInput(c *Client, data InputData) {
	r.mu.Lock()
	defer r.mu.Unlock()

	if !r.started {
		return
	}

	playerIdx := -1
	for i, p := range r.players {
		if p.client == c {
			playerIdx = i
			break
		}
	}
	if playerIdx == -1 {
		return
	}

	r.sync.AddInput(playerIdx, data)
	r.relayFrames()
}

func (r *Room) HandleState(c *Client, data StateData) {
	r.mu.Lock()
	defer r.mu.Unlock()

	if r.validator == nil {
		c.sendJSON(Message{
			Type: MsgStateAck,
			Data: mustMarshal(StateAckData{
				Frame: data.Frame,
				Valid: true,
			}),
		})
		return
	}

	valid := r.validator.VerifyStateHash(data.Frame, data.Hash)

	if !valid {
		log.Printf("room %s: anti-cheat flag from %s at frame %d",
			r.ID, c.playerName, data.Frame)
	}

	c.sendJSON(Message{
		Type: MsgStateAck,
		Data: mustMarshal(StateAckData{
			Frame: data.Frame,
			Valid: valid,
		}),
	})
}

func (r *Room) relayFrames() {
	for {
		frame, inputs, ok := r.sync.TryGetFrame()
		if !ok {
			break
		}

		for i, p := range r.players {
			otherInput := inputs[1-i]
			p.client.sendJSON(Message{
				Type: MsgFrame,
				Data: mustMarshal(InputData{
					Frame: frame,
					Input: otherInput,
				}),
			})
		}
	}
}

func (r *Room) broadcastRoomState() {
	type PlayerInfo struct {
		Name  string `json:"name"`
		Ready bool   `json:"ready"`
	}
	type RoomState struct {
		RoomID  string       `json:"room_id"`
		Players []PlayerInfo `json:"players"`
	}

	players := make([]PlayerInfo, len(r.players))
	for i, p := range r.players {
		players[i] = PlayerInfo{
			Name:  p.client.playerName,
			Ready: p.ready,
		}
	}

	state := RoomState{
		RoomID:  r.ID,
		Players: players,
	}

	for _, p := range r.players {
		p.client.sendJSON(Message{
			Type: "room_state",
			Data: mustMarshal(state),
		})
	}
}
