package main

import "encoding/json"

type MessageType string

const (
	MsgJoin       MessageType = "join"
	MsgCreateRoom MessageType = "create_room"
	MsgJoinRoom   MessageType = "join_room"
	MsgReady      MessageType = "ready"
	MsgInput      MessageType = "input"
	MsgFrame      MessageType = "frame"
	MsgStart      MessageType = "start"
	MsgEnd        MessageType = "end"
	MsgError      MessageType = "error"
	MsgState      MessageType = "state"
	MsgStateAck   MessageType = "state_ack"
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

func mustMarshal(v interface{}) json.RawMessage {
	data, _ := json.Marshal(v)
	return data
}
