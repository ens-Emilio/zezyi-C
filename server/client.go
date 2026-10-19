package main

import (
	"encoding/json"
	"log"
	"time"

	"github.com/gorilla/websocket"
)

const (
	writeWait      = 10 * time.Second
	pongWait       = 60 * time.Second
	pingPeriod     = (pongWait * 9) / 10
	maxMessageSize = 4096
)

type Client struct {
	conn       *websocket.Conn
	mm         *Matchmaker
	playerName string
	room       *Room
	send       chan []byte
}

func NewClient(conn *websocket.Conn, mm *Matchmaker) *Client {
	return &Client{conn: conn, mm: mm, send: make(chan []byte, 256)}
}

func (c *Client) readPump() {
	defer func() {
		if c.room != nil {
			c.room.Leave(c)
		}
		c.conn.Close()
	}()
	c.conn.SetReadLimit(maxMessageSize)
	c.conn.SetReadDeadline(time.Now().Add(pongWait))
	c.conn.SetPongHandler(func(string) error { c.conn.SetReadDeadline(time.Now().Add(pongWait)); return nil })
	for {
		_, data, err := c.conn.ReadMessage()
		if err != nil {
			break
		}
		var msg Message
		if err := json.Unmarshal(data, &msg); err != nil {
			c.sendError("invalid message format")
			continue
		}
		c.handleMessage(&msg)
	}
}

func (c *Client) writePump() {
	ticker := time.NewTicker(pingPeriod)
	defer func() { ticker.Stop(); c.conn.Close() }()
	for {
		select {
		case data, ok := <-c.send:
			c.conn.SetWriteDeadline(time.Now().Add(writeWait))
			if !ok {
				c.conn.WriteMessage(websocket.CloseMessage, []byte{})
				return
			}
			if err := c.conn.WriteMessage(websocket.TextMessage, data); err != nil {
				return
			}
		case <-ticker.C:
			c.conn.SetWriteDeadline(time.Now().Add(writeWait))
			if err := c.conn.WriteMessage(websocket.PingMessage, nil); err != nil {
				return
			}
		}
	}
}

func convertEntries(in []PlayerStats) []LeaderboardEntry {
	out := make([]LeaderboardEntry, len(in))
	for i, p := range in {
		out[i] = LeaderboardEntry{Name: p.Name, MMR: p.MMR, Tier: TierForMMR(p.MMR), Wins: p.Wins, Losses: p.Losses}
	}
	return out
}

func (c *Client) handleMessage(msg *Message) {
	switch msg.Type {
	case MsgJoin:
		var d JoinData
		if err := json.Unmarshal(msg.Data, &d); err != nil {
			c.sendError("invalid join data")
			return
		}
		c.playerName = d.PlayerName
		log.Printf("player joined: %s", c.playerName)
	case MsgCreateRoom:
		var d CreateRoomData
		if err := json.Unmarshal(msg.Data, &d); err != nil {
			c.sendError("invalid create_room data")
			return
		}
		room := c.mm.CreateRoom(d.Private)
		if room.Join(c) {
			c.room = room
			c.sendJSON(Message{Type: MsgCreateRoom, Data: mustMarshal(map[string]string{"room_id": room.ID})})
		}
	case MsgJoinRoom:
		var d JoinRoomData
		if err := json.Unmarshal(msg.Data, &d); err != nil {
			c.sendError("invalid join_room data")
			return
		}
		room, ok := c.mm.GetRoom(d.RoomID)
		if !ok {
			c.sendError("room not found")
			return
		}
		if room.Join(c) {
			c.room = room
		}
	case MsgReady:
		if c.room == nil {
			c.sendError("not in a room")
			return
		}
		var d ReadyData
		if err := json.Unmarshal(msg.Data, &d); err != nil {
			c.sendError("invalid ready data")
			return
		}
		c.room.SetReady(c, d.Ready)
	case MsgInput:
		if c.room == nil {
			return
		}
		var d InputData
		if err := json.Unmarshal(msg.Data, &d); err != nil {
			return
		}
		c.room.HandleInput(c, d)
	case MsgState:
		if c.room == nil {
			return
		}
		var d StateData
		if err := json.Unmarshal(msg.Data, &d); err != nil {
			return
		}
		c.room.HandleState(c, d)
	case MsgGarbage:
		if c.room == nil {
			return
		}
		var d GarbageData
		if err := json.Unmarshal(msg.Data, &d); err != nil {
			return
		}
		c.room.HandleGarbage(c, d)
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
	case MsgLeaderboard:
		entries := []PlayerStats{}
		if c.mm.store != nil {
			entries = c.mm.store.Leaderboard(20)
		}
		c.sendJSON(Message{Type: MsgLeaderboard, Data: mustMarshal(LeaderboardData{Entries: convertEntries(entries)})})
	case MsgPlayerStats:
		var ps *PlayerStats
		if c.mm.store != nil {
			ps = c.mm.store.GetOrCreate(c.playerName)
		} else {
			np := NewPlayerStats(c.playerName)
			ps = np
		}
		c.sendJSON(Message{Type: MsgPlayerStats, Data: mustMarshal(PlayerStatsData{Name: ps.Name, MMR: ps.MMR, Tier: TierForMMR(ps.MMR), Wins: ps.Wins, Losses: ps.Losses})})
	case MsgTournamentJoin:
		var d TournamentJoinData
		if err := json.Unmarshal(msg.Data, &d); err != nil {
			c.sendError("invalid tournament_join data")
			return
		}
		t, ok := c.mm.GetTournament(d.TournamentID)
		if !ok {
			c.sendError("tournament not found")
			return
		}
		if !t.AddPlayer(c) {
			c.sendError("tournament full or started")
		}
	case MsgTournamentStart:
		var d TournamentStartData
		if err := json.Unmarshal(msg.Data, &d); err != nil {
			c.sendError("invalid tournament_start data")
			return
		}
		t, ok := c.mm.GetTournament(d.TournamentID)
		if !ok {
			c.sendError("tournament not found")
			return
		}
		t.Start()
		c.sendJSON(Message{Type: MsgTournamentState, Data: mustMarshal(map[string]interface{}{"rounds": t.BracketState()})})
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
	default:
		c.sendError("unknown message type")
	}
}

func (c *Client) sendJSON(msg Message) {
	data, _ := json.Marshal(msg)
	select {
	case c.send <- data:
	default:
		log.Printf("client %s send buffer full, dropping", c.playerName)
	}
}
func (c *Client) sendError(msg string) {
	c.sendJSON(Message{Type: MsgError, Data: mustMarshal(ErrorData{Message: msg})})
}
func (c *Client) PlayerName() string { return c.playerName }
func (c *Client) Room() *Room        { return c.room }
func (c *Client) SetRoom(r *Room)    { c.room = r }
