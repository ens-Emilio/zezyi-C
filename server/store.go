package main

import (
	"encoding/json"
	"log"
	"os"
	"sort"
	"sync"
)

// TimeAttackRecord stores best time/score per mode for global ranking.
type TimeAttackRecord struct {
	Name   string `json:"name"`
	Mode   int    `json:"mode"`
	TimeMs int    `json:"time_ms"`
	Score  int    `json:"score"`
}

type Store struct {
	mu       sync.RWMutex
	path     string
	players  map[string]*PlayerStats
	records  []TimeAttackRecord
}

func NewStore(path string) *Store {
	s := &Store{
		path:    path,
		players: make(map[string]*PlayerStats),
	}
	if path != "" {
		s.load()
	}
	return s
}

func (s *Store) load() {
	data, err := os.ReadFile(s.path)
	if err != nil {
		return
	}
	var raw struct {
		Players map[string]*PlayerStats `json:"players"`
		Records []TimeAttackRecord      `json:"records"`
	}
	if err := json.Unmarshal(data, &raw); err != nil {
		// legacy: flat map
		var flat map[string]*PlayerStats
		if err2 := json.Unmarshal(data, &flat); err2 == nil {
			s.players = flat
			return
		}
		log.Printf("store: corrupt %s: %v", s.path, err)
		return
	}
	if raw.Players != nil {
		s.players = raw.Players
	}
	s.records = raw.Records
}

func (s *Store) save() {
	if s.path == "" {
		return
	}
	raw := struct {
		Players map[string]*PlayerStats `json:"players"`
		Records []TimeAttackRecord      `json:"records"`
	}{Players: s.players, Records: s.records}
	data, _ := json.Marshal(raw)
	tmp := s.path + ".tmp"
	if err := os.WriteFile(tmp, data, 0644); err != nil {
		log.Printf("store: write tmp failed: %v", err)
		return
	}
	if err := os.Rename(tmp, s.path); err != nil {
		log.Printf("store: rename failed: %v", err)
	}
}

func (s *Store) Get(name string) *PlayerStats {
	s.mu.RLock()
	p, ok := s.players[name]
	s.mu.RUnlock()
	if !ok {
		return nil
	}
	cp := *p
	return &cp
}

func (s *Store) GetOrCreate(name string) *PlayerStats {
	s.mu.Lock()
	defer s.mu.Unlock()
	p, ok := s.players[name]
	if !ok {
		np := NewPlayerStats(name)
		p = np
		s.players[name] = p
		s.save()
	}
	cp := *p
	return &cp
}

func (s *Store) getOrCreateLocked(name string) *PlayerStats {
	p, ok := s.players[name]
	if !ok {
		np := NewPlayerStats(name)
		p = np
		s.players[name] = p
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

func (s *Store) Leaderboard(n int) []PlayerStats {
	s.mu.RLock()
	defer s.mu.RUnlock()
	all := make([]PlayerStats, 0, len(s.players))
	for _, p := range s.players {
		all = append(all, *p)
	}
	sort.Slice(all, func(i, j int) bool { return all[i].MMR > all[j].MMR })
	if n > len(all) {
		n = len(all)
	}
	return all[:n]
}

// Time-attack API
func (s *Store) SubmitTimeAttack(name string, mode int, timeMs int, score int) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.records = append(s.records, TimeAttackRecord{Name: name, Mode: mode, TimeMs: timeMs, Score: score})
	s.save()
}

const (
	ModeSprint = 1 // must match GameMode MODE_SPRINT in config.h
	ModeCheese = 5 // MODE_CHEESE
)

func (s *Store) GetTimeAttackRecords(mode int) []TimeAttackRecord {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var filtered []TimeAttackRecord
	for _, r := range s.records {
		if r.Mode == mode {
			filtered = append(filtered, r)
		}
	}
	// time modes (Sprint/Cheese) ascending time, score modes descending score
	timeMode := mode == ModeSprint || mode == ModeCheese // MODE_SPRINT=1, MODE_CHEESE=5 per config.h
	sort.Slice(filtered, func(i, j int) bool {
		if timeMode {
			if filtered[i].TimeMs == 0 { return false }
			if filtered[j].TimeMs == 0 { return true }
			return filtered[i].TimeMs < filtered[j].TimeMs
		}
		return filtered[i].Score > filtered[j].Score
	})
	if len(filtered) > 50 {
		filtered = filtered[:50]
	}
	return filtered
}
