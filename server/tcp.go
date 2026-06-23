package main

import (
	"bufio"
	"encoding/json"
	"log"
	"net"
	"time"
)

const (
	tcpReadTimeout  = 60 * time.Second
	tcpWriteTimeout = 10 * time.Second
)

// TCPClient wraps a raw TCP connection with the same interface as Client.
type TCPClient struct {
	conn       net.Conn
	reader     *bufio.Reader
	mm         *Matchmaker
	playerName string
	room       *Room
	send       chan []byte
}

func NewTCPClient(conn net.Conn, mm *Matchmaker) *TCPClient {
	return &TCPClient{
		conn:   conn,
		reader: bufio.NewReader(conn),
		mm:     mm,
		send:   make(chan []byte, 256),
	}
}

func (c *TCPClient) PlayerName() string { return c.playerName }
func (c *TCPClient) Room() *Room        { return c.room }
func (c *TCPClient) SetRoom(r *Room)    { c.room = r }

func (c *TCPClient) readPump() {
	defer func() {
		if c.room != nil {
			c.room.Leave(c)
		}
		c.conn.Close()
	}()

	for {
		c.conn.SetReadDeadline(time.Now().Add(tcpReadTimeout))
		line, err := c.reader.ReadString('\n')
		if err != nil {
			break
		}

		var msg Message
		if err := json.Unmarshal([]byte(line), &msg); err != nil {
			c.sendError("invalid message format")
			continue
		}

		c.handleMessage(&msg)
	}
}

func (c *TCPClient) writePump() {
	ticker := time.NewTicker(54 * time.Second)
	defer func() {
		ticker.Stop()
		c.conn.Close()
	}()

	for {
		select {
		case data, ok := <-c.send:
			c.conn.SetWriteDeadline(time.Now().Add(tcpWriteTimeout))
			if !ok {
				return
			}
			if _, err := c.conn.Write(append(data, '\n')); err != nil {
				return
			}
		case <-ticker.C:
			c.conn.SetWriteDeadline(time.Now().Add(tcpWriteTimeout))
			if _, err := c.conn.Write([]byte("\n")); err != nil {
				return
			}
		}
	}
}

func (c *TCPClient) handleMessage(msg *Message) {
	switch msg.Type {
	case MsgJoin:
		var d JoinData
		if err := json.Unmarshal(msg.Data, &d); err != nil {
			c.sendError("invalid join data")
			return
		}
		c.playerName = d.PlayerName
		log.Printf("TCP player joined: %s", c.playerName)

	case MsgCreateRoom:
		var d CreateRoomData
		if err := json.Unmarshal(msg.Data, &d); err != nil {
			c.sendError("invalid create_room data")
			return
		}
		room := c.mm.CreateRoom(d.Private)
		if room.Join(c) {
			c.room = room
			c.sendJSON(Message{Type: MsgCreateRoom, Data: mustMarshal(
				map[string]string{"room_id": room.ID})})
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

	default:
		c.sendError("unknown message type")
	}
}

func (c *TCPClient) sendJSON(msg Message) {
	data, _ := json.Marshal(msg)
	select {
	case c.send <- data:
	default:
		log.Printf("TCP client %s send buffer full, dropping", c.playerName)
	}
}

func (c *TCPClient) sendError(msg string) {
	c.sendJSON(Message{
		Type: MsgError,
		Data: mustMarshal(ErrorData{Message: msg}),
	})
}
