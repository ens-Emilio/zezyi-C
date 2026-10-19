package main

import (
	"log"
	"sync"
)

const MaxTournamentPlayers = 32

type TournamentMatch struct {
	Player1   PlayerClient
	Player2   PlayerClient
	Winner    PlayerClient
	Room      *Room
	Completed bool
}

type Tournament struct {
	mu           sync.Mutex
	ID           string
	Name         string
	Players      []PlayerClient
	Bracket      [][]TournamentMatch
	CurrentRound int
	Started      bool
	mm           *Matchmaker
}

func NewTournament(id, name string, mm *Matchmaker) *Tournament {
	return &Tournament{ID: id, Name: name, mm: mm}
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
	log.Printf("tournament %s: %s joined (%d players)", t.ID, c.PlayerName(), len(t.Players))
	return true
}

func (t *Tournament) Start() {
	t.mu.Lock()
	defer t.mu.Unlock()
	if t.Started || len(t.Players) < 2 {
		return
	}
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
		if round[i].Player2 == nil {
			round[i].Winner = round[i].Player1
			round[i].Completed = true
		}
	}
	t.Bracket = [][]TournamentMatch{round}
	t.CurrentRound = 0
	t.Started = true
	log.Printf("tournament %s started with %d players, bracket size %d", t.ID, len(t.Players), n)
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
	room := t.mm.CreateRoom(false)
	room.tournament = t
	room.tournamentMatchIdx = matchIdx
	if match.Player1 != nil {
		room.Join(match.Player1)
	}
	if match.Player2 != nil {
		room.Join(match.Player2)
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
	log.Printf("tournament %s: match %d winner = %s", t.ID, matchIdx, winnerName)
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
		log.Printf("tournament %s: champion = %s", t.ID, winners[0].PlayerName())
		return
	}
	if len(winners) == 0 {
		return
	}
	roundSize := len(winners) / 2
	round := make([]TournamentMatch, roundSize)
	for i := 0; i < roundSize; i++ {
		round[i].Player1 = winners[i*2]
		if i*2+1 < len(winners) {
			round[i].Player2 = winners[i*2+1]
		}
		if round[i].Player2 == nil {
			round[i].Winner = round[i].Player1
			round[i].Completed = true
		}
	}
	t.Bracket = append(t.Bracket, round)
	t.CurrentRound++
	log.Printf("tournament %s: advanced to round %d (%d matches)", t.ID, t.CurrentRound, len(round))
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
			matches[i] = MatchInfo{Done: m.Completed}
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
