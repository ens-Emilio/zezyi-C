package main

import "testing"

func TestELO(t *testing.T) {
	w, l := CalculateELO(1000, 1000, 0, 0)
	if w <= 1000 {
		t.Errorf("winner MMR should increase, got %d", w)
	}
	if l >= 1000 {
		t.Errorf("loser MMR should decrease, got %d", l)
	}
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
	tests := []struct{ mmr int; tier string }{
		{0, "Bronze"}, {1199, "Bronze"}, {1200, "Silver"}, {1399, "Silver"},
		{1400, "Gold"}, {1600, "Platinum"}, {1800, "Diamond"}, {2000, "Master"},
		{3000, "Master"},
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
		t.Errorf("win/loss not tracked w=%d l=%d", w.Wins, l.Losses)
	}
}

func TestStoreLeaderboard(t *testing.T) {
	s := NewStore("")
	s.UpdateMatch("Alice", "Bob")
	s.UpdateMatch("Alice", "Charlie")
	lb := s.Leaderboard(2)
	if len(lb) != 2 {
		t.Fatalf("want 2, got %d", len(lb))
	}
	if lb[0].MMR < lb[1].MMR {
		t.Errorf("leaderboard not sorted")
	}
}

func TestELOFloor(t *testing.T) {
	_, l := CalculateELO(3000, 10, 50, 50)
	if l < 0 {
		t.Errorf("MMR went negative: %d", l)
	}
}

func TestProvisionalDoubleK(t *testing.T) {
	w1, _ := CalculateELO(1000, 1000, 0, 0)
	w2, _ := CalculateELO(1000, 1000, 50, 50)
	if w1-w2 <= 0 {
		t.Errorf("provisional should give bigger swing: w1=%d w2=%d", w1, w2)
	}
}
