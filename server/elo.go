package main

import "math"

const (
	KFactor          = 32
	BaseMMR          = 1000
	ProvisionalGames = 10
)

var tierThresholds = []struct {
	Name   string
	MinMMR int
}{
	{"Bronze", 0},
	{"Silver", 1200},
	{"Gold", 1400},
	{"Platinum", 1600},
	{"Diamond", 1800},
	{"Master", 2000},
}

type PlayerStats struct {
	Name        string `json:"name"`
	MMR         int    `json:"mmr"`
	Wins        int    `json:"wins"`
	Losses      int    `json:"losses"`
	GamesPlayed int    `json:"games_played"`
}

func CalculateELO(winnerMMR, loserMMR int, winnerGames, loserGames int) (int, int) {
	expectedWin := 1.0 / (1.0 + math.Pow(10, float64(loserMMR-winnerMMR)/400.0))
	expectedLose := 1.0 - expectedWin
	k := KFactor
	if winnerGames < ProvisionalGames {
		k = KFactor * 2
	}
	if loserGames < ProvisionalGames {
		k2 := KFactor * 2
		if k2 > k {
			k = k2
		}
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

func TierForMMR(mmr int) string {
	for i := len(tierThresholds) - 1; i >= 0; i-- {
		if mmr >= tierThresholds[i].MinMMR {
			return tierThresholds[i].Name
		}
	}
	return "Bronze"
}

func NewPlayerStats(name string) *PlayerStats {
	return &PlayerStats{Name: name, MMR: BaseMMR}
}
