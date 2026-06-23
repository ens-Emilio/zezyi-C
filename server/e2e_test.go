package main

import (
	"bufio"
	"encoding/json"
	"net"
	"strings"
	"testing"
	"time"
)

func startTestServer(t *testing.T) string {
	tcpPort := "19081"
	mm := NewMatchmaker()
	go startTCPServer(tcpPort, mm)
	time.Sleep(200 * time.Millisecond)
	return tcpPort
}

func tcpConnect(t *testing.T, port string) (net.Conn, *bufio.Reader) {
	conn, err := net.Dial("tcp", "localhost:"+port)
	if err != nil {
		t.Fatalf("dial failed: %v", err)
	}
	return conn, bufio.NewReader(conn)
}

func sendMsg(t *testing.T, conn net.Conn, msg Message) {
	data, _ := json.Marshal(msg)
	conn.Write(append(data, '\n'))
}

func recvMsg(t *testing.T, reader *bufio.Reader) Message {
	line, err := reader.ReadString('\n')
	if err != nil {
		t.Fatalf("read failed: %v", err)
	}
	line = strings.TrimSpace(line)
	var msg Message
	if err := json.Unmarshal([]byte(line), &msg); err != nil {
		t.Fatalf("unmarshal failed: %v (line: %s)", err, line)
	}
	return msg
}

// recvUntil reads messages until one matching the desired type is found,
// skipping intermediate messages like room_state broadcasts.
func recvUntil(t *testing.T, reader *bufio.Reader, want MessageType) Message {
	for {
		msg := recvMsg(t, reader)
		if msg.Type == want {
			return msg
		}
	}
}

func TestE2ETCPRoomAndFrameSync(t *testing.T) {
	tcpPort := startTestServer(t)

	// Connect two TCP clients
	conn1, reader1 := tcpConnect(t, tcpPort)
	defer conn1.Close()
	conn2, reader2 := tcpConnect(t, tcpPort)
	defer conn2.Close()

	// Both send join
	sendMsg(t, conn1, Message{
		Type: MsgJoin,
		Data: mustMarshal(JoinData{PlayerName: "Alice"}),
	})
	sendMsg(t, conn2, Message{
		Type: MsgJoin,
		Data: mustMarshal(JoinData{PlayerName: "Bob"}),
	})

	// Client 1 creates a room
	sendMsg(t, conn1, Message{
		Type: MsgCreateRoom,
		Data: mustMarshal(CreateRoomData{Private: false}),
	})

	// Client 1 receives create_room response (skip room_state messages
	// broadcast by room.Join during create_room handling)
	resp1 := recvUntil(t, reader1, MsgCreateRoom)
	var roomResp struct {
		RoomID string `json:"room_id"`
	}
	json.Unmarshal(resp1.Data, &roomResp)
	if roomResp.RoomID == "" {
		t.Fatal("empty room_id")
	}
	t.Logf("room created: %s", roomResp.RoomID)

	// Client 2 joins the room
	sendMsg(t, conn2, Message{
		Type: MsgJoinRoom,
		Data: mustMarshal(JoinRoomData{RoomID: roomResp.RoomID}),
	})

	// Both should receive room_state messages (player joined)
	recvUntil(t, reader1, "room_state")
	recvUntil(t, reader2, "room_state")

	// Both send ready
	sendMsg(t, conn1, Message{
		Type: MsgReady,
		Data: mustMarshal(ReadyData{Ready: true}),
	})
	sendMsg(t, conn2, Message{
		Type: MsgReady,
		Data: mustMarshal(ReadyData{Ready: true}),
	})

	// Both should receive start message (skip room_state messages
	// broadcast by SetReady)
	start1 := recvUntil(t, reader1, MsgStart)
	var sd1 StartData
	json.Unmarshal(start1.Data, &sd1)
	t.Logf("client 1 start: seed=%d opponent=%s", sd1.Seed, sd1.Opponent)

	start2 := recvUntil(t, reader2, MsgStart)
	var sd2 StartData
	json.Unmarshal(start2.Data, &sd2)
	t.Logf("client 2 start: seed=%d opponent=%s", sd2.Seed, sd2.Opponent)

	// Verify same seed
	if sd1.Seed != sd2.Seed {
		t.Fatalf("seed mismatch: %d vs %d", sd1.Seed, sd2.Seed)
	}
	if sd1.Opponent != "Bob" || sd2.Opponent != "Alice" {
		t.Fatalf("opponent mismatch: %s vs %s", sd1.Opponent, sd2.Opponent)
	}

	// Both send frame 0 input
	sendMsg(t, conn1, Message{
		Type: MsgInput,
		Data: mustMarshal(InputData{Frame: 0, Input: 0x01}),
	})
	sendMsg(t, conn2, Message{
		Type: MsgInput,
		Data: mustMarshal(InputData{Frame: 0, Input: 0x02}),
	})

	// Both should receive the opponent's frame input
	frame1 := recvUntil(t, reader1, MsgFrame)
	var fd1 InputData
	json.Unmarshal(frame1.Data, &fd1)
	if fd1.Input != 0x02 {
		t.Fatalf("client 1 expected input 0x02, got 0x%02x", fd1.Input)
	}
	t.Logf("client 1 received frame %d input 0x%02x", fd1.Frame, fd1.Input)

	frame2 := recvUntil(t, reader2, MsgFrame)
	var fd2 InputData
	json.Unmarshal(frame2.Data, &fd2)
	if fd2.Input != 0x01 {
		t.Fatalf("client 2 expected input 0x01, got 0x%02x", fd2.Input)
	}
	t.Logf("client 2 received frame %d input 0x%02x", fd2.Frame, fd2.Input)

	t.Log("E2E TCP test passed!")
}
