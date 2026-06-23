# Zezyi Plan 2 — Competitive Gameplay Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add ranked MMR, tournament brackets, spectate mode, advanced competitive scoring, ghost VS overlay, online garbage system, and global time attack rankings.

**Architecture:** Extends the Foundation server (Go) and client (C) with competitive features. Server gains ELO/MMR storage, tournament brackets, and observer relay. Client gains ghost overlay rendering and competitive scoring rules.

**Tech Stack:** C11 (client), Go 1.21+ (server), SQLite or JSON file (MMR storage), existing TCP protocol

---

## File Structure

### Go Server (new + modified)

| File | Responsibility |
|---|---|
| `server/elo.go` | **NEW** — ELO/MMR calculation, tier assignment |
| `server/store.go` | **NEW** — Persistent storage (JSON file or SQLite) for MMR, match history |
| `server/tournament.go` | **NEW** — Bracket generation, round tracking, observer routing |
| `server/spectator.go` | **NEW** — Observer connection handling, frame broadcast |
| `server/protocol.go` | **MODIFY** — Add ranked/tournament/spectate message types |
| `server/room.go` | **MODIFY** — Add ranked flag, spectator list, garbage relay |
| `server/main.go` | **MODIFY** — Add HTTP endpoints for stats/leaderboard |
| `server/matchmaker.go` | **MODIFY** — Add ranked matchmaking queue |

### C Client (new + modified)

| File | Responsibility |
|---|---|
| `include/competitive.h` | **NEW** — Competitive scoring constants, garbage rules |
| `src/competitive.c` | **NEW** — Advanced scoring (T-spin solo, garbage offset, B2B doubled) |
| `src/render_sdl.c` | **MODIFY** — Ghost overlay rendering, ranked UI, spectate indicator |
| `src/app.c` | **MODIFY** — Ranked/tournament/spectate state handling |
| `include/config.h` | **MODIFY** — Add APP_RANKED, APP_TOURNAMENT, APP_SPECTATE states |
| `src/rules.c` | **MODIFY** — Integrate competitive scoring hooks |

### Tests

| File | Responsibility |
|---|---|
| `tests/test_competitive.c` | **NEW** — Advanced scoring tests |
| `server/competitive_test.go` | **NEW** — ELO, tournament, spectate Go tests |

---

## Task 1: ELO/MMR System (Server)

**Files:**
- Create: `server/elo.go`
- Create: `server/store.go`
- Modify: `server/protocol.go` (add MMR types)

- [ ] **Step 1: Create elo.go**

```go
package main

import "math"

// ELO constants
const (
	KFactor    = 32   // Standard K-factor
	BaseMMR    = 1000  // Starting MMR
	ProvisionalGames = 10 // Games before MMR stabilizes
)

// Tier thresholds (minimum MMR for each tier)
var tierThresholds = []struct {
	Name string
	MinMMR int
}{
	{"Bronze", 0},
	{"Silver", 1200},
	{"Gold", 1400},
	{"Platinum", 1600},
	{"Diamond", 1800},
	{"Master", 2000},
}

// PlayerStats tracks a player's competitive record
type PlayerStats struct {
	Name       string `json:"name"`
	MMR        int    `json:"mmr"`
	Wins       int    `json:"wins"`
	Losses     int    `json:"losses"`
	GamesPlayed int   `json:"games_played"`
}

// CalculateELO updates both players' MMR based on outcome.
// winnerMMR and loserMMR are current ratings.
// Returns new winnerMMR, loserMMR.
func CalculateELO(winnerMMR, loserMMR int, winnerGames, loserGames int) (int, int) {
	expectedWin := 1.0 / (1.0 + math.Pow(10, float64(loserMMR-winnerMMR)/400.0))
	expectedLose := 1.0 - expectedWin

	k := KFactor
	if winnerGames < ProvisionalGames {
		k = KFactor * 2 // Double K for provisional players
	}
	if loserGames < ProvisionalGames {
		k = KFactor * 2
	}

	winnerDelta := int(float64(k) * (1.0 - expectedWin))
	loserDelta := int(float64(k) * (0.0 - expectedLose))

	newWinner := winnerMMR + winnerDelta
	newLoser := loserMMR + loserDelta

	if newLoser < 0 {
		newLoser = 0
	}

	return newWinner, newLoser
}

// TierForMMR returns the tier name for a given MMR
func TierForMMR(mmr int) string {
	for i := len(tierThresholds) - 1; i >= 0; i-- {
		if mmr >= tierThresholds[i].MinMMR {
			return tierThresholds[i].Name
		}
	}
	return "Bronze"
}

// NewPlayerStats creates default stats for a new player
func NewPlayerStats(name string) PlayerStats {
	return PlayerStats{
		Name: name,
		MMR:  BaseMMR,
	}
}
```

- [ ] **Step 2: Create store.go (JSON file storage)**

```go
package main

import (
	"encoding/json"
	"os"
	"sync"
)

// Store persists player stats to a JSON file
type Store struct {
	mu      sync.RWMutex
	path    string
	players map[string]*PlayerStats
}

func NewStore(path string) *Store {
	s := &Store{
		path:    path,
		players: make(map[string]*PlayerStats),
	}
	s.load()
	return s
}

func (s *Store) load() {
	data, err := os.ReadFile(s.path)
	if err != nil {
		return // File doesn't exist yet — fine
	}
	json.Unmarshal(data, &s.players)
}

func (s *Store) save() {
	data, _ := json.Marshal(s.players)
	os.WriteFile(s.path, data, 0644)
}

func (s *Store) Get(name string) *PlayerStats {
	s.mu.RLock()
	p, ok := s.players[name]
	s.mu.RUnlock()
	if !ok {
		return nil
	}
	return p
}

func (s *Store) GetOrCreate(name string) *PlayerStats {
	s.mu.Lock()
	defer s.mu.Unlock()

	p, ok := s.players[name]
	if !ok {
		p = NewPlayerStats(name)
		s.players[name] = p
		s.save()
	}
	return p
}

func (s *Store) UpdateMatch(winner, loser string) (PlayerStats, PlayerStats) {
	s.mu.Lock()
	defer s.mu.Unlock()

	w := s.getOrCreateLocked(winner)
	l := s.getOrCreateLocked(loser)

	w.MMR, l.MMR = CalculateELO(w.MMR, l.MMR, w.GamesPlayed, l.GamesPlayed)
	w.Wins++
	w.GamesPlayed++
	l.Losses++
	l.GamesPlayed++

	s.save()
	return *w, *l
}

func (s *Store) getOrCreateLocked(name string) *PlayerStats {
	p, ok := s.players[name]
	if !ok {
		p = NewPlayerStats(name)
		s.players[name] = p
	}
	return p
}

// Leaderboard returns top N players by MMR
func (s *Store) Leaderboard(n int) []PlayerStats {
	s.mu.RLock()
	defer s.mu.RUnlock()

	all := make([]PlayerStats, 0, len(s.players))
	for _, p := range s.players {
		all = append(all, *p)
	}

	// Simple sort by MMR descending
	for i := 0; i < len(all); i++ {
		for j := i + 1; j < len(all); j++ {
			if all[j].MMR > all[i].MMR {
				all[i], all[j] = all[j], all[i]
			}
		}
	}

	if n > len(all) {
		n = len(all)
	}
	return all[:n]
}
```

- [ ] **Step 3: Add MMR message types to protocol.go**

Add to `server/protocol.go`:

```go
const (
	MsgRankedQueue MessageType = "ranked_queue"
	MsgRankedMatch  MessageType = "ranked_match"
	MsgMatchResult  MessageType = "match_result"
	MsgLeaderboard  MessageType = "leaderboard"
	MsgPlayerStats  MessageType = "player_stats"
)

type RankedQueueData struct {
	Queue bool `json:"queue"`
}

type MatchResultData struct {
	Winner    string `json:"winner"`
	Loser     string `json:"loser"`
	WinnerMMR int    `json:"winner_mmr"`
	LoserMMR  int    `json:"loser_mmr"`
	WinnerTier string `json:"winner_tier"`
	LoserTier  string `json:"loser_tier"`
}

type LeaderboardEntry struct {
	Name  string `json:"name"`
	MMR   int    `json:"mmr"`
	Tier  string `json:"tier"`
	Wins  int    `json:"wins"`
	Losses int   `json:"losses"`
}

type LeaderboardData struct {
	Entries []LeaderboardEntry `json:"entries"`
}

type PlayerStatsData struct {
	Name   string `json:"name"`
	MMR    int    `json:"mmr"`
	Tier   string `json:"tier"`
	Wins   int    `json:"wins"`
	Losses int    `json:"losses"`
}
```

- [ ] **Step 4: Add store initialization to main.go**

In `server/main.go`, add store initialization:

```go
	store := NewStore("zezyi_stats.json")
	_ = store // Used by matchmaker for ranked
```

Pass `store` to the matchmaker for ranked matchmaking.

- [ ] **Step 5: Add leaderboard HTTP endpoint**

In `server/main.go`, add:

```go
	http.HandleFunc("/leaderboard", func(w http.ResponseWriter, r *http.Request) {
		entries := store.Leaderboard(20)
		json.NewEncoder(w).Encode(entries)
	})

	http.HandleFunc("/stats", func(w http.ResponseWriter, r *http.Request) {
		name := r.URL.Query().Get("name")
		if name == "" {
			http.Error(w, "missing name", 400)
			return
		}
		stats := store.Get(name)
		if stats == nil {
			stats = NewPlayerStats(name)
		}
		json.NewEncoder(w).Encode(stats)
	})
```

- [ ] **Step 6: Add store to Matchmaker and handle ranked queue**

Update `Matchmaker` struct to include store and ranked queue:

```go
type Matchmaker struct {
	mu          sync.RWMutex
	rooms       map[string]*Room
	store       *Store
	rankedQueue []*Client // Players waiting for ranked match
}
```

Update `NewMatchmaker`:

```go
func NewMatchmaker(store *Store) *Matchmaker {
	return &Matchmaker{
		rooms: make(map[string]*Room),
		store: store,
	}
}
```

Add ranked queue method:

```go
func (mm *Matchmaker) JoinRankedQueue(c *Client) {
	mm.mu.Lock()
	defer mm.mu.Unlock()

	mm.rankedQueue = append(mm.rankedQueue, c)

	// If 2+ players in queue, match them
	if len(mm.rankedQueue) >= 2 {
		p1 := mm.rankedQueue[0]
		p2 := mm.rankedQueue[1]
		mm.rankedQueue = mm.rankedQueue[2:]

		// Create a ranked room
		room := mm.CreateRoom(false)
		room.ranked = true
		room.Join(p1)
		room.Join(p2)
	}
}

func (mm *Matchmaker) LeaveRankedQueue(c *Client) {
	mm.mu.Lock()
	defer mm.mu.Unlock()

	for i, q := range mm.rankedQueue {
		if q == c {
			mm.rankedQueue = append(mm.rankedQueue[:i], mm.rankedQueue[i+1:]...)
			break
		}
	}
}
```

- [ ] **Step 7: Add ranked flag to Room and handle match end**

In `server/room.go`, add `ranked bool` field to Room.

In `Leave()`, when match ends and room is ranked, update MMR:

```go
	if r.started && r.ranked && len(r.players) > 0 {
		// The remaining player wins by forfeit
		winner := r.players[0].client.PlayerName()
		// Find loser — the one who left
		// We need to track this. For now, the disconnecting player loses.
		// This is handled by passing the loser name.
	}
```

Actually, better approach: add a method to Room that handles match end with winner/loser:

```go
func (r *Room) EndMatch(winner, loser string) {
	if !r.ranked || r.mm == nil || r.mm.store == nil {
		return
	}
	wStats, lStats := r.mm.store.UpdateMatch(winner, loser)
	
	// Notify both players of their new MMR
	for _, p := range r.players {
		p.sendJSON(Message{
			Type: MsgMatchResult,
			Data: mustMarshal(MatchResultData{
				Winner:     winner,
				Loser:      loser,
				WinnerMMR:  wStats.MMR,
				LoserMMR:   lStats.MMR,
				WinnerTier: TierForMMR(wStats.MMR),
				LoserTier:  TierForMMR(lStats.MMR),
			}),
		})
	}
}
```

- [ ] **Step 8: Handle ranked queue messages in client.go and tcp.go**

Add `MsgRankedQueue` handling to both `handleMessage` methods:

```go
	case MsgRankedQueue:
		var d RankedQueueData
		if err := json.Unmarshal(msg.Data, &d); err != nil {
			c.sendError("invalid ranked_queue data")
			return
		}
		if d.Queue {
			c.mm.JoinRankedQueue(c)
		} else {
			c.mm.LeaveRankedQueue(c)
		}
```

Also add `MsgLeaderboard` and `MsgPlayerStats` request handling:

```go
	case MsgLeaderboard:
		entries := c.mm.store.Leaderboard(20)
		c.sendJSON(Message{
			Type: MsgLeaderboard,
			Data: mustMarshal(LeaderboardData{
				Entries: convertEntries(entries),
			}),
		})

	case MsgPlayerStats:
		c.sendJSON(Message{
			Type: MsgPlayerStats,
			Data: mustMarshal(PlayerStatsData{
				Name: c.playerName,
				MMR:  c.mm.store.GetOrCreate(c.playerName).MMR,
				Tier: TierForMMR(c.mm.store.GetOrCreate(c.playerName).MMR),
				Wins: c.mm.store.GetOrCreate(c.playerName).Wins,
				Losses: c.mm.store.GetOrCreate(c.playerName).Losses,
			}),
		})
```

- [ ] **Step 9: Build and test**

```bash
cd server
go build -o zezyi-server
go vet ./...
./zezyi-server &
curl http://localhost:8080/leaderboard
curl "http://localhost:8080/stats?name=TestPlayer"
kill %1
```

- [ ] **Step 10: Write Go test for ELO**

Create `server/elo_test.go`:

```go
package main

import "testing"

func TestELO(t *testing.T) {
	// Two players at base MMR
	w, l := CalculateELO(1000, 1000, 0, 0)
	if w <= 1000 {
		t.Errorf("winner MMR should increase, got %d", w)
	}
	if l >= 1000 {
		t.Errorf("loser MMR should decrease, got %d", l)
	}

	// Higher-rated player beats lower — smaller gain
	w2, l2 := CalculateELO(1800, 1000, 20, 20)
	wGain := w2 - 1800
	lLoss := 1000 - l2
	if wGain >= 16 {
		t.Errorf("favorite should gain less, got +%d", wGain)
	}
	if lLoss >= 16 {
		t.Errorf("underdog should lose less, got -%d", lLoss)
	}
}

func TestTierForMMR(t *testing.T) {
	tests := []struct {
		mmr   int
		tier  string
	}{
		{0, "Bronze"},
		{1199, "Bronze"},
		{1200, "Silver"},
		{1399, "Silver"},
		{1400, "Gold"},
		{1600, "Platinum"},
		{1800, "Diamond"},
		{2000, "Master"},
	}
	for _, tt := range tests {
		got := TierForMMR(tt.mmr)
		if got != tt.tier {
			t.Errorf("TierForMMR(%d) = %s, want %s", tt.mmr, got, tt.tier)
		}
	}
}

func TestStoreUpdateMatch(t *testing.T) {
	s := NewStore("")
	w, l := s.UpdateMatch("Alice", "Bob")
	if w.MMR <= BaseMMR {
		t.Errorf("winner should gain MMR")
	}
	if l.MMR >= BaseMMR {
		t.Errorf("loser should lose MMR")
	}
	if w.Wins != 1 || l.Losses != 1 {
		t.Errorf("win/loss not tracked")
	}
}
```

- [ ] **Step 11: Run tests**

```bash
cd server
go test -v -run TestELO ./
go test -v -run TestTier ./
go test -v -run TestStore ./
```

- [ ] **Step 12: Commit**

```bash
git add server/
git commit -m "feat: add ELO/MMR system with persistent storage and leaderboard"
```

---

## Task 2: Tournament Bracket System (Server)

**Files:**
- Create: `server/tournament.go`
- Modify: `server/protocol.go` (add tournament types)
- Modify: `server/room.go` (tournament match integration)

- [ ] **Step 1: Create tournament.go**

```go
package main

import (
	"sync"
	"log"
)

const (
	MaxTournamentPlayers = 32
)

type TournamentMatch struct {
	Player1   PlayerClient
	Player2   PlayerClient
	Winner    PlayerClient
	Room      *Room
	Completed bool
}

type Tournament struct {
	mu          sync.Mutex
	ID          string
	Name        string
	Players     []PlayerClient
	Bracket     [][]TournamentMatch // rounds[0] = round of 16, etc
	CurrentRound int
	Started     bool
	mm          *Matchmaker
}

type PlayerClient interface {
	PlayerName() string
	SetRoom(*Room)
}

func NewTournament(id, name string, mm *Matchmaker) *Tournament {
	return &Tournament{
		ID:  id,
		Name: name,
		mm:  mm,
	}
}

func (t *Tournament) AddPlayer(c PlayerClient) bool {
	t.mu.Lock()
	defer t.mu.Unlock()

	if t.Started {
		return false
	}
	if len(t.Players) >= MaxTournamentPlayers {
		return false
	}

	t.Players = append(t.Players, c)
	log.Printf("tournament %s: %s joined (%d players)",
		t.ID, c.PlayerName(), len(t.Players))
	return true
}

func (t *Tournament) Start() {
	t.mu.Lock()
	defer t.mu.Unlock()

	if t.Started || len(t.Players) < 2 {
		return
	}

	// Pad to next power of 2 with byes
	n := nextPow2(len(t.Players))
	roundSize := n / 2

	round := make([]TournamentMatch, roundSize)
	for i := 0; i < roundSize; i++ {
		p1Idx := i * 2
		p2Idx := i*2 + 1

		if p1Idx < len(t.Players) {
			round[i].Player1 = t.Players[p1Idx]
		}
		if p2Idx < len(t.Players) {
			round[i].Player2 = t.Players[p2Idx]
		}

		// If only one player (bye), they auto-advance
		if round[i].Player2 == nil {
			round[i].Winner = round[i].Player1
			round[i].Completed = true
		}
	}

	t.Bracket = [][]TournamentMatch{round}
	t.CurrentRound = 0
	t.Started = true

	log.Printf("tournament %s started with %d players, bracket size %d",
		t.ID, len(t.Players), n)
}

func (t *Tournament) CreateMatch(matchIdx int) {
	t.mu.Lock()
	defer t.mu.Unlock()

	if t.CurrentRound >= len(t.Bracket) {
		return
	}
	match := &t.Bracket[t.CurrentRound][matchIdx]
	if match.Completed || match.Player1 == nil || match.Player2 == nil {
		return
	}

	// Create room for this match
	room := t.mm.CreateRoom(false)
	room.tournament = t
	room.tournamentMatchIdx = matchIdx

	// Convert PlayerClient to *Client/TCPClient for Join
	// This requires PlayerClient to also satisfy the room's Join interface
	// For now, we'll handle this via type assertion
	if c1, ok := match.Player1.(*Client); ok {
		room.Join(c1)
	}
	if c2, ok := match.Player2.(*Client); ok {
		room.Join(c2)
	}

	match.Room = room
}

func (t *Tournament) RecordResult(matchIdx int, winnerName string) {
	t.mu.Lock()
	defer t.mu.Unlock()

	if t.CurrentRound >= len(t.Bracket) {
		return
	}

	match := &t.Bracket[t.CurrentRound][matchIdx]
	if match.Completed {
		return
	}

	if match.Player1 != nil && match.Player1.PlayerName() == winnerName {
		match.Winner = match.Player1
	} else if match.Player2 != nil && match.Player2.PlayerName() == winnerName {
		match.Winner = match.Player2
	}
	match.Completed = true

	log.Printf("tournament %s: match %d winner = %s",
		t.ID, matchIdx, winnerName)

	// Check if round is complete
	allDone := true
	for i := range t.Bracket[t.CurrentRound] {
		if !t.Bracket[t.CurrentRound][i].Completed {
			allDone = false
			break
		}
	}

	if allDone {
		t.advanceRound()
	}
}

func (t *Tournament) advanceRound() {
	winners := make([]PlayerClient, 0, len(t.Bracket[t.CurrentRound])/2)
	for _, m := range t.Bracket[t.CurrentRound] {
		if m.Winner != nil {
			winners = append(winners, m.Winner)
		}
	}

	if len(winners) == 1 {
		// Tournament over!
		log.Printf("tournament %s: champion = %s",
			t.ID, winners[0].PlayerName())
		return
	}

	// Create next round
	roundSize := len(winners) / 2
	round := make([]TournamentMatch, roundSize)
	for i := 0; i < roundSize; i++ {
		round[i].Player1 = winners[i*2]
		round[i].Player2 = winners[i*2+1]
		if round[i].Player2 == nil {
			round[i].Winner = round[i].Player1
			round[i].Completed = true
		}
	}

	t.Bracket = append(t.Bracket, round)
	t.CurrentRound++

	log.Printf("tournament %s: advanced to round %d (%d matches)",
		t.ID, t.CurrentRound, len(round))
}

func (t *Tournament) BracketState() interface{} {
	type MatchInfo struct {
		Player1 string `json:"player1"`
		Player2 string `json:"player2"`
		Winner  string `json:"winner"`
		Done    bool   `json:"done"`
	}
	type RoundInfo struct {
		Matches []MatchInfo `json:"matches"`
	}

	rounds := make([]RoundInfo, len(t.Bracket))
	for r, round := range t.Bracket {
		matches := make([]MatchInfo, len(round))
		for i, m := range round {
			matches[i] = MatchInfo{
				Done: m.Completed,
			}
			if m.Player1 != nil {
				matches[i].Player1 = m.Player1.PlayerName()
			}
			if m.Player2 != nil {
				matches[i].Player2 = m.Player2.PlayerName()
			}
			if m.Winner != nil {
				matches[i].Winner = m.Winner.PlayerName()
			}
		}
		rounds[r] = RoundInfo{Matches: matches}
	}

	return rounds
}

func nextPow2(n int) int {
	p := 1
	for p < n {
		p *= 2
	}
	return p
}
```

- [ ] **Step 2: Add tournament fields to Room**

In `server/room.go`, add to Room struct:

```go
	tournament        *Tournament
	tournamentMatchIdx int
```

In `Leave()`, when a ranked/tournament match ends due to disconnect:

```go
	if r.started && r.tournament != nil {
		// Remaining player wins by forfeit
		if len(r.players) > 0 {
			winnerName := r.players[0].client.PlayerName()
			r.tournament.RecordResult(r.tournamentMatchIdx, winnerName)
		}
	}
```

- [ ] **Step 3: Add tournament message types to protocol.go**

```go
const (
	MsgTournamentJoin  MessageType = "tournament_join"
	MsgTournamentStart MessageType = "tournament_start"
	MsgTournamentState MessageType = "tournament_state"
)

type TournamentJoinData struct {
	TournamentID string `json:"tournament_id"`
}

type TournamentStartData struct {
	TournamentID string `json:"tournament_id"`
	Round        int    `json:"round"`
	MatchIdx     int    `json:"match_idx"`
}

type TournamentStateData struct {
	Rounds []interface{} `json:"rounds"`
}
```

- [ ] **Step 4: Add tournament manager to matchmaker**

In `server/matchmaker.go`, add:

```go
	tournaments map[string]*Tournament
```

Add methods:
```go
func (mm *Matchmaker) CreateTournament(name string) *Tournament {
	id := generateRoomID() // reuse ID generator
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
```

- [ ] **Step 5: Build and test**

```bash
cd server
go build -o zezyi-server
go vet ./...
```

- [ ] **Step 6: Commit**

```bash
git add server/
git commit -m "feat: add tournament bracket system"
```

---

## Task 3: Spectate Mode (Server + Client)

**Files:**
- Create: `server/spectator.go`
- Modify: `server/room.go` (add spectator list, broadcast)
- Modify: `server/protocol.go` (add spectate types)
- Modify: `src/app.c` (add APP_SPECTATE state)
- Modify: `include/config.h` (add state)

- [ ] **Step 1: Add spectator support to Room**

In `server/room.go`, add to Room:

```go
	spectators []PlayerClient
```

Add methods:

```go
func (r *Room) AddSpectator(c PlayerClient) {
	r.mu.Lock()
	defer r.mu.Unlock()
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

func (r *Room) broadcastToSpectators(msg Message) {
	for _, s := range r.spectators {
		s.sendJSON(msg)
	}
}
```

In `relayFrames()`, after relaying to players, also broadcast to spectators:

```go
	// Also relay to spectators
	r.broadcastToSpectators(Message{
		Type: MsgFrame,
		Data: mustMarshal(InputData{Frame: frame, Input: inputs[0]}),
	})
	r.broadcastToSpectators(Message{
		Type: MsgFrame,
		Data: mustMarshal(InputData{Frame: frame, Input: inputs[1]}),
	})
```

- [ ] **Step 2: Add spectate message types**

In `server/protocol.go`:

```go
const (
	MsgSpectate MessageType = "spectate"
)

type SpectateData struct {
	RoomID string `json:"room_id"`
}
```

- [ ] **Step 3: Handle spectate message in client.go and tcp.go**

Add to handleMessage:

```go
	case MsgSpectate:
		var d SpectateData
		if err := json.Unmarshal(msg.Data, &d); err != nil {
			c.sendError("invalid spectate data")
			return
		}
		room, ok := c.mm.GetRoom(d.RoomID)
		if !ok {
			c.sendError("room not found")
			return
		}
		room.AddSpectator(c)
```

- [ ] **Step 4: Add APP_SPECTATE to client config.h**

```c
    APP_SPECTATE,     /* watching a match in real time */
```

- [ ] **Step 5: Build and test**

```bash
make && cd server && go build && go vet ./...
```

- [ ] **Step 6: Commit**

```bash
git add include/config.h server/ src/
git commit -m "feat: add spectate mode with observer relay"
```

---

## Task 4: Advanced Competitive Scoring (C Client)

**Files:**
- Create: `include/competitive.h`
- Create: `src/competitive.c`
- Create: `tests/test_competitive.c`
- Modify: `Makefile`

- [ ] **Step 1: Create competitive.h**

```c
#pragma once
#include "config.h"

/* Competitive scoring — used in online/ranked matches */

/* Garbage lines sent for each clear type (competitive mode) */
int competitive_garbage_for_clear(int lines, int tspin, int b2b, int all_clear);

/* Garbage with offset hole (random column 0-9) */
typedef struct {
	int lines;
	int hole_col;  /* 0-9 */
} GarbageQueue;

void garbage_queue_init(GarbageQueue *gq);
void garbage_queue_add(GarbageQueue *gq, int lines, uint32_t rng_seed);
int  garbage_queue_pending(const GarbageQueue *gq);

/* T-spin solo scoring (no line clear needed) */
int competitive_tspin_solo_score(int level);

/* B2B bonus multiplier (doubled in competitive) */
int competitive_b2b_bonus(int base_score, int b2b);
```

- [ ] **Step 2: Create competitive.c**

```c
#include "competitive.h"

void garbage_queue_init(GarbageQueue *gq)
{
	gq->lines = 0;
	gq->hole_col = 0;
}

void garbage_queue_add(GarbageQueue *gq, int lines, uint32_t rng_seed)
{
	gq->lines += lines;
	/* Random hole column based on seed */
	gq->hole_col = (int)(rng_seed % 10);
}

int garbage_queue_pending(const GarbageQueue *gq)
{
	return gq->lines;
}

/* Competitive garbage rules:
 * T-spin solo (no lines): 4 lines
 * T-spin single: 4 lines
 * T-spin double: 6 lines
 * T-spin triple: 8 lines
 * Quad: 6 lines
 * B2B: doubled
 * All clear: +10 lines
 */
int competitive_garbage_for_clear(int lines, int tspin, int b2b, int all_clear)
{
	int garbage = 0;

	if (tspin) {
		switch (lines) {
			case 0: garbage = 4; break;  /* T-spin solo */
			case 1: garbage = 4; break;
			case 2: garbage = 6; break;
			case 3: garbage = 8; break;
			default: break;
		}
	} else {
		switch (lines) {
			case 1: garbage = 0; break;
			case 2: garbage = 1; break;
			case 3: garbage = 2; break;
			case 4: garbage = 4; break; /* Quad */
			default: break;
		}
	}

	/* B2B bonus: doubled in competitive */
	if (b2b) garbage *= 2;

	/* All clear: bonus garbage */
	if (all_clear) garbage += 10;

	return garbage;
}

int competitive_tspin_solo_score(int level)
{
	/* T-spin solo (no line clear): 400 * (level+1) */
	return 400 * (level + 1);
}

int competitive_b2b_bonus(int base_score, int b2b)
{
	if (b2b) return base_score * 3 / 2;
	return base_score;
}
```

- [ ] **Step 3: Create test_competitive.c**

```c
#include <stdio.h>
#include "config.h"
#include "competitive.h"

static int g_pass = 0, g_fail = 0;
#define EXPECT(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL [%s:%d] %s\n", __FILE__, __LINE__, msg); } \
} while (0)

int main(void)
{
	printf("\n== Competitive Scoring ==\n");

	/* Garbage for clears */
	EXPECT(competitive_garbage_for_clear(0, 1, 0, 0) == 4, "T-spin solo = 4 garbage");
	EXPECT(competitive_garbage_for_clear(1, 1, 0, 0) == 4, "T-spin single = 4 garbage");
	EXPECT(competitive_garbage_for_clear(2, 1, 0, 0) == 6, "T-spin double = 6 garbage");
	EXPECT(competitive_garbage_for_clear(3, 1, 0, 0) == 8, "T-spin triple = 8 garbage");
	EXPECT(competitive_garbage_for_clear(4, 0, 0, 0) == 4, "Quad = 4 garbage");
	EXPECT(competitive_garbage_for_clear(2, 0, 0, 0) == 1, "Double = 1 garbage");
	EXPECT(competitive_garbage_for_clear(1, 0, 0, 0) == 0, "Single = 0 garbage");

	/* B2B doubled */
	EXPECT(competitive_garbage_for_clear(4, 0, 1, 0) == 8, "B2B Quad = 8 garbage");
	EXPECT(competitive_garbage_for_clear(2, 1, 1, 0) == 12, "B2B T-spin double = 12");

	/* All clear bonus */
	EXPECT(competitive_garbage_for_clear(4, 0, 0, 1) == 14, "Quad + all clear = 14");

	/* T-spin solo score */
	EXPECT(competitive_tspin_solo_score(0) == 400, "T-spin solo L0 = 400");
	EXPECT(competitive_tspin_solo_score(5) == 2400, "T-spin solo L5 = 2400");

	/* B2B bonus */
	EXPECT(competitive_b2b_bonus(800, 1) == 1200, "B2B bonus 800 → 1200");
	EXPECT(competitive_b2b_bonus(800, 0) == 800, "No B2B = same");

	/* Garbage queue */
	GarbageQueue gq;
	garbage_queue_init(&gq);
	EXPECT(garbage_queue_pending(&gq) == 0, "empty queue = 0");
	garbage_queue_add(&gq, 4, 0xABCDEF);
	EXPECT(garbage_queue_pending(&gq) == 4, "after add = 4");
	garbage_queue_add(&gq, 2, 0x123456);
	EXPECT(garbage_queue_pending(&gq) == 6, "after second add = 6");

	printf("\n%d passed, %d failed\n", g_pass, g_fail);
	return g_fail > 0 ? 1 : 0;
}
```

- [ ] **Step 4: Add Makefile target**

```makefile
COMPETITIVE_SRC = tests/test_competitive.c \
                  src/competitive.c
test_competitive: $(COMPETITIVE_SRC)
	$(CC) -std=c11 $(WARN) -g -O0 \
	    $(COMPETITIVE_SRC) -Iinclude -DZEZYI_HEADLESS -o test_competitive
	./test_competitive
```

- [ ] **Step 5: Run test**

```bash
make test_competitive
```

- [ ] **Step 6: Commit**

```bash
git add include/competitive.h src/competitive.c tests/test_competitive.c Makefile
git commit -m "feat: add competitive scoring with garbage system"
```

---

## Task 5: Ghost VS Overlay (C Client)

**Files:**
- Modify: `include/config.h` (add ghost game state to GameState)
- Modify: `src/render_sdl.c` (ghost overlay rendering)
- Modify: `src/app.c` (ghost game initialization)
- Modify: `src/rules.c` (ghost tick)

- [ ] **Step 1: Add ghost game state to GameState**

In `include/config.h`, add to GameState:

```c
    /* Ghost VS — parallel game for replay overlay */
    GameState ghost_game;
    ReplayPlayer ghost_replay;
    int ghost_active;      /* 1 if ghost overlay is running */
```

- [ ] **Step 2: Add ghost rendering to render_sdl.c**

Add a function to render the ghost board with semi-transparent overlay:

```c
static void render_ghost_overlay(SDL_Renderer *renderer, const GameState *g,
                                  const Layout *lay)
{
    if (!g->ghost_active) return;

    /* Render ghost board at offset, semi-transparent */
    /* Use alpha blending */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    /* Draw ghost cells with alpha 128 */
    const GameState *gh = &g->ghost_game;
    int cell = lay->cell_px;

    for (int y = 0; y < BOARD_H; y++) {
        for (int x = 0; x < BOARD_W; x++) {
            if (gh->board.cells[y][x] != CELL_EMPTY) {
                SDL_Rect r = {
                    lay->board.x + x * cell,
                    lay->board.y + y * cell,
                    cell, cell
                };
                /* Cyan tint for ghost */
                SDL_SetRenderDrawColor(renderer, 0, 245, 255, 100);
                SDL_RenderFillRect(renderer, &r);
            }
        }
    }

    /* Draw ghost active piece outline */
    if (gh->current.kind < PIECE_COUNT) {
        for (int py = 0; py < PIECE_DIM; py++) {
            for (int px = 0; px < PIECE_DIM; px++) {
                if (piece_cell_at(gh->current.kind, gh->current.rotation, px, py)) {
                    int gx = gh->current.x + px;
                    int gy = gh->current.y + py;
                    if (gx >= 0 && gx < BOARD_W && gy >= 0 && gy < BOARD_H) {
                        SDL_Rect r = {
                            lay->board.x + gx * cell,
                            lay->board.y + gy * cell,
                            cell, cell
                        };
                        SDL_SetRenderDrawColor(renderer, 0, 245, 255, 150);
                        SDL_RenderDrawRect(renderer, &r);
                    }
                }
            }
        }
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}
```

Call this from `render_game` when `g->ghost_active` is true.

- [ ] **Step 3: Initialize ghost from replay in app.c**

In the `APP_PLAYING` or `APP_ONLINE_VS` initialization:

```c
    /* Load ghost replay if available */
    if (ghost_replay_available) {
        game_init_for_mode(&ctx->game.ghost_game, ctx->game.ghost_replay.seed,
                           ctx->game.ghost_replay.mode);
        ctx->game.ghost_active = 1;
    }
```

- [ ] **Step 4: Tick ghost game in game_tick**

In `src/rules.c`, at the end of `game_tick`:

```c
    /* Tick ghost game if active */
    if (g->ghost_active) {
        InputSnapshot ghost_in = {0};
        replay_get_input(&g->ghost_replay, (int)g->ghost_game.tick, &ghost_in);
        game_tick(&g->ghost_game, &ghost_in, 1.0 / 60.0);
    }
```

- [ ] **Step 5: Build and verify**

```bash
make
make test
```

- [ ] **Step 6: Commit**

```bash
git add include/config.h src/render_sdl.c src/app.c src/rules.c
git commit -m "feat: add ghost VS overlay rendering"
```

---

## Task 6: Online Garbage System (Client + Server)

**Files:**
- Modify: `src/app.c` (garbage send/receive)
- Modify: `server/room.go` (garbage relay)
- Modify: `server/protocol.go` (garbage message)
- Modify: `include/netcode.h` (garbage message type)
- Modify: `src/netcode.c` (garbage parse)

- [ ] **Step 1: Add garbage message to protocol**

In `server/protocol.go`:

```go
const (
	MsgGarbage MessageType = "garbage"
)

type GarbageData struct {
	Frame  int `json:"frame"`
	Lines  int `json:"lines"`
	HoleCol int `json:"hole_col"`
}
```

- [ ] **Step 2: Relay garbage in server**

In `server/room.go`, add method:

```go
func (r *Room) HandleGarbage(c PlayerClient, data GarbageData) {
	r.mu.Lock()
	defer r.mu.Unlock()

	if !r.started {
		return
	}

	// Relay garbage to the other player
	for i, p := range r.players {
		if p.client != c {
			p.client.sendJSON(Message{
				Type: MsgGarbage,
				Data: mustMarshal(data),
			})
			_ = i
			break
		}
	}
}
```

Handle `MsgGarbage` in client.go and tcp.go handleMessage.

- [ ] **Step 3: Add garbage to C netcode**

In `include/netcode.h`, add:

```c
	NC_MSG_GARBAGE
```

Add fields to NetMessage:

```c
	int garbage_lines;
	int garbage_hole_col;
```

In `src/netcode.c`, add `net_send_garbage`:

```c
int net_send_garbage(NetContext *nc, int frame, int lines, int hole_col)
{
	char buf[128];
	snprintf(buf, sizeof(buf),
		"{\"type\":\"garbage\",\"data\":{\"frame\":%d,\"lines\":%d,\"hole_col\":%d}}",
		frame, lines, hole_col);
	return send_line(nc, buf);
}
```

Parse garbage messages in `net_poll`.

- [ ] **Step 4: Send/receive garbage in app.c**

In `APP_ONLINE_VS`, after `game_tick`, check for line clears and send garbage:

```c
    /* Check if we cleared lines → send garbage */
    int lines_cleared = /* compare total_lines before/after tick */;
    if (lines_cleared > 0) {
        int garbage = competitive_garbage_for_clear(
            lines_cleared, g->tspin, g->b2b, g->all_clear);
        if (garbage > 0) {
            net_send_garbage(ctx->net, ctx->online_frame,
                            garbage, rand() % 10);
        }
    }
```

On receiving `NC_MSG_GARBAGE`, add to garbage queue and apply to board.

- [ ] **Step 5: Build and test**

```bash
make && cd server && go build && go vet ./...
./tests/test_e2e.sh
```

- [ ] **Step 6: Commit**

```bash
git add include/netcode.h src/netcode.c src/app.c server/
git commit -m "feat: add online garbage system with competitive rules"
```

---

## Task 7: Time Attack Global Ranking (Server)

**Files:**
- Modify: `server/store.go` (add time attack records)
- Modify: `server/main.go` (add leaderboard endpoint)
- Modify: `server/protocol.go` (add time attack types)

- [ ] **Step 1: Add time attack records to store**

In `server/store.go`, add:

```go
type TimeAttackRecord struct {
	Name     string `json:"name"`
	Mode     int    `json:"mode"`     // GameMode enum
	TimeMs   int    `json:"time_ms"`  // Sprint/Cheese completion time
	Score    int    `json:"score"`    // Ultra/Blitz/Marathon/Survival score
}

func (s *Store) GetTimeAttackRecords(mode int) []TimeAttackRecord {
	// Return top records for the given mode
	// Store in a separate map or file
}

func (s *Store) SubmitTimeAttack(name string, mode int, timeMs int, score int) {
	// Add record, sort, keep top N
}
```

- [ ] **Step 2: Add HTTP endpoint for time attack leaderboard**

In `server/main.go`:

```go
	http.HandleFunc("/leaderboard/sprint", func(w http.ResponseWriter, r *http.Request) {
		records := store.GetTimeAttackRecords(int(1)) // MODE_SPRINT
		json.NewEncoder(w).Encode(records)
	})

	http.HandleFunc("/leaderboard/ultra", func(w http.ResponseWriter, r *http.Request) {
		records := store.GetTimeAttackRecords(int(2)) // MODE_ULTRA
		json.NewEncoder(w).Encode(records)
	})
```

- [ ] **Step 3: Handle match end with time attack submission**

In `server/room.go`, when a match ends:

```go
	if r.ranked {
		// Submit to time attack leaderboard
		var timeMs int
		var score int
		// Parse from match end data
		r.mm.store.SubmitTimeAttack(winnerName, mode, timeMs, score)
	}
```

- [ ] **Step 4: Build and test**

```bash
cd server
go build && go vet ./...
```

- [ ] **Step 5: Commit**

```bash
git add server/
git commit -m "feat: add time attack global ranking"
```

---

## Summary

| Task | What | Files |
|---|---|---|
| 1 | ELO/MMR system + leaderboard | server/elo.go, store.go, protocol.go |
| 2 | Tournament bracket system | server/tournament.go |
| 3 | Spectate mode | server/spectator.go, client states |
| 4 | Advanced competitive scoring | competitive.h, competitive.c |
| 5 | Ghost VS overlay | render_sdl.c, rules.c |
| 6 | Online garbage system | netcode, room.go, protocol.go |
| 7 | Time attack ranking | store.go, main.go |

**Deliverable:** Full competitive mode — ranked with ELO, tournaments with brackets, spectate, competitive scoring rules, ghost VS overlay, online garbage, global leaderboards.