package main

import "encoding/json"

type MessageType string

const (
	MsgJoin            MessageType = "join"
	MsgCreateRoom      MessageType = "create_room"
	MsgJoinRoom        MessageType = "join_room"
	MsgReady           MessageType = "ready"
	MsgInput           MessageType = "input"
	MsgFrame           MessageType = "frame"
	MsgStart           MessageType = "start"
	MsgEnd             MessageType = "end"
	MsgError           MessageType = "error"
	MsgState           MessageType = "state"
	MsgStateAck        MessageType = "state_ack"
	MsgGarbage         MessageType = "garbage"
	MsgRankedQueue     MessageType = "ranked_queue"
	MsgRankedMatch     MessageType = "ranked_match"
	MsgMatchResult     MessageType = "match_result"
	MsgLeaderboard     MessageType = "leaderboard"
	MsgPlayerStats     MessageType = "player_stats"
	MsgTournamentJoin  MessageType = "tournament_join"
	MsgTournamentStart MessageType = "tournament_start"
	MsgTournamentState MessageType = "tournament_state"
	MsgSpectate        MessageType = "spectate"
)

type Message struct {
	Type MessageType     `json:"type"`
	Data json.RawMessage `json:"data,omitempty"`
}

type JoinData struct {
	PlayerName string `json:"player_name"`
}

type CreateRoomData struct {
	Private bool `json:"private"`
}

type JoinRoomData struct {
	RoomID string `json:"room_id"`
}

type ReadyData struct {
	Ready bool `json:"ready"`
}

type InputData struct {
	Frame int   `json:"frame"`
	Input uint8 `json:"input"`
}

type StartData struct {
	Seed     uint32 `json:"seed"`
	Mode     int    `json:"mode"`
	Opponent string `json:"opponent"`
	RoomID   string `json:"room_id"`
}

type EndData struct {
	Winner string `json:"winner"`
	Reason string `json:"reason"`
}

type StateData struct {
	Frame int    `json:"frame"`
	Hash  uint32 `json:"hash"`
}

type StateAckData struct {
	Frame int  `json:"frame"`
	Valid bool `json:"valid"`
}

type ErrorData struct {
	Message string `json:"message"`
}

type GarbageData struct {
	Frame   int `json:"frame"`
	Lines   int `json:"lines"`
	HoleCol int `json:"hole_col"`
}

type RankedQueueData struct {
	Queue bool `json:"queue"`
}

type MatchResultData struct {
	Winner     string `json:"winner"`
	Loser      string `json:"loser"`
	WinnerMMR  int    `json:"winner_mmr"`
	LoserMMR   int    `json:"loser_mmr"`
	WinnerTier string `json:"winner_tier"`
	LoserTier  string `json:"loser_tier"`
}

type LeaderboardEntry struct {
	Name   string `json:"name"`
	MMR    int    `json:"mmr"`
	Tier   string `json:"tier"`
	Wins   int    `json:"wins"`
	Losses int    `json:"losses"`
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

type SpectateData struct {
	RoomID string `json:"room_id"`
}

func mustMarshal(v interface{}) json.RawMessage {
	data, err := json.Marshal(v)
	if err != nil {
		panic("mustMarshal: " + err.Error())
	}
	return data
}
