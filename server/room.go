package main

import (
	"log"
	"math/rand"
	"sync"
)

const (
	RoomMaxPlayers = 2
)

type PlayerClient interface {
	sendJSON(Message)
	sendError(string)
	PlayerName() string
	Room() *Room
	SetRoom(*Room)
}

type Player struct {
	client PlayerClient
	ready  bool
}

type Room struct {
	mu                   sync.Mutex
	ID                   string
	Private              bool
	players              []*Player
	spectators           []PlayerClient
	sync                 *FrameSync
	validator            *Validator
	started              bool
	ranked               bool
	mm                   *Matchmaker
	tournament           interface{ RecordResult(int, string) }
	tournamentMatchIdx   int
}

func NewRoom(id string, private bool, mm *Matchmaker) *Room {
	return &Room{
		ID:      id,
		Private: private,
		sync:    NewFrameSync(),
		mm:      mm,
	}
}

func (r *Room) Join(c PlayerClient) bool {
	if c.Room() != nil && c.Room() != r {
		c.Room().Leave(c)
		c.SetRoom(nil)
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	if len(r.players) >= RoomMaxPlayers {
		c.sendError("room is full")
		return false
	}
	r.players = append(r.players, &Player{client: c, ready: false})
	log.Printf("room %s: %s joined (%d/%d)", r.ID, c.PlayerName(), len(r.players), RoomMaxPlayers)
	r.broadcastRoomState()
	return true
}

func (r *Room) Leave(c PlayerClient) {
	r.mu.Lock()
	defer r.mu.Unlock()
	for i, p := range r.players {
		if p.client == c {
			r.players = append(r.players[:i], r.players[i+1:]...)
			break
		}
	}
	for i, s := range r.spectators {
		if s == c {
			r.spectators = append(r.spectators[:i], r.spectators[i+1:]...)
			break
		}
	}
	log.Printf("room %s: %s left (%d/%d)", r.ID, c.PlayerName(), len(r.players), RoomMaxPlayers)
	if r.started {
		var winner string
		if len(r.players) > 0 {
			winner = r.players[0].client.PlayerName()
			if r.ranked && r.mm != nil && r.mm.store != nil {
				loser := c.PlayerName()
				r.mm.store.UpdateMatch(winner, loser)
				log.Printf("room %s: ranked result %s beats %s", r.ID, winner, loser)
			}
			if r.tournament != nil {
				r.tournament.RecordResult(r.tournamentMatchIdx, winner)
			}
		}
		for _, p := range r.players {
			p.client.sendJSON(Message{
				Type: MsgEnd,
				Data: mustMarshal(EndData{Winner: winner, Reason: "disconnect"}),
			})
		}
		for _, s := range r.spectators {
			s.sendJSON(Message{
				Type: MsgEnd,
				Data: mustMarshal(EndData{Winner: winner, Reason: "disconnect"}),
			})
		}
		r.started = false
	}
	if len(r.players) == 0 {
		r.started = false
		go r.mm.RemoveRoom(r.ID)
	}
}

func (r *Room) SetReady(c PlayerClient, ready bool) {
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
	r.validator = NewValidator(seed)
	opponentNames := make([]string, len(r.players))
	for i, p := range r.players {
		opponentNames[i] = p.client.PlayerName()
	}
	for i, p := range r.players {
		opp := ""
		if len(opponentNames) > 1 {
			opp = opponentNames[1-i]
		}
		p.client.sendJSON(Message{
			Type: MsgStart,
			Data: mustMarshal(StartData{Seed: seed, Mode: 0, Opponent: opp, RoomID: r.ID}),
		})
	}
	log.Printf("room %s: match started (seed=%d ranked=%v)", r.ID, seed, r.ranked)
}

func (r *Room) HandleInput(c PlayerClient, data InputData) {
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

func (r *Room) HandleState(c PlayerClient, data StateData) {
	r.mu.Lock()
	defer r.mu.Unlock()
	if r.validator == nil {
		c.sendJSON(Message{Type: MsgStateAck, Data: mustMarshal(StateAckData{Frame: data.Frame, Valid: true})})
		return
	}
	valid := r.validator.VerifyStateHash(data.Frame, data.Hash)
	if !valid {
		log.Printf("room %s: anti-cheat flag from %s at frame %d", r.ID, c.PlayerName(), data.Frame)
	}
	c.sendJSON(Message{Type: MsgStateAck, Data: mustMarshal(StateAckData{Frame: data.Frame, Valid: valid})})
}

func (r *Room) HandleGarbage(c PlayerClient, data GarbageData) {
	r.mu.Lock()
	defer r.mu.Unlock()
	if !r.started {
		return
	}
	for _, p := range r.players {
		if p.client != c {
			p.client.sendJSON(Message{Type: MsgGarbage, Data: mustMarshal(data)})
			break
		}
	}
}

func (r *Room) AddSpectator(c PlayerClient) {
	r.mu.Lock()
	defer r.mu.Unlock()
	for _, s := range r.spectators {
		if s == c {
			return
		}
	}
	r.spectators = append(r.spectators, c)
	log.Printf("room %s: %s spectating", r.ID, c.PlayerName())
}

func (r *Room) RemoveSpectator(c PlayerClient) {
	r.mu.Lock()
	defer r.mu.Unlock()
	for i, s := range r.spectators {
		if s == c {
			r.spectators = append(r.spectators[:i], r.spectators[i+1:]...)
			break
		}
	}
}

func (r *Room) relayFrames() {
	for {
		frame, inputs, ok := r.sync.TryGetFrame()
		if !ok {
			break
		}
		for i, p := range r.players {
			otherInput := inputs[1-i]
			p.client.sendJSON(Message{Type: MsgFrame, Data: mustMarshal(InputData{Frame: frame, Input: otherInput})})
		}
		// relay to spectators: send both inputs as separate frame messages
		for _, s := range r.spectators {
			for j := 0; j < len(inputs); j++ {
				s.sendJSON(Message{Type: MsgFrame, Data: mustMarshal(InputData{Frame: frame, Input: inputs[j]})})
			}
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
		players[i] = PlayerInfo{Name: p.client.PlayerName(), Ready: p.ready}
	}
	state := RoomState{RoomID: r.ID, Players: players}
	for _, p := range r.players {
		p.client.sendJSON(Message{Type: "room_state", Data: mustMarshal(state)})
	}
}
