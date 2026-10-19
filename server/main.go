package main

import (
	"encoding/json"
	"log"
	"net"
	"net/http"
	"os"

	"github.com/gorilla/websocket"
)

var upgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool {
		origin := r.Header.Get("Origin")
		if origin == "" {
			return true
		}
		allowed := os.Getenv("ALLOWED_ORIGINS")
		if allowed == "" {
			return true
		}
		for _, o := range splitComma(allowed) {
			if o == origin {
				return true
			}
		}
		return false
	},
}

func splitComma(s string) []string {
	var out []string
	start := 0
	for i := 0; i <= len(s); i++ {
		if i == len(s) || s[i] == ',' {
			t := s[start:i]
			for len(t) > 0 && (t[0] == ' ' || t[0] == '\t') {
				t = t[1:]
			}
			for len(t) > 0 && (t[len(t)-1] == ' ' || t[len(t)-1] == '\t') {
				t = t[:len(t)-1]
			}
			if t != "" {
				out = append(out, t)
			}
			start = i + 1
		}
	}
	return out
}

func main() {
	port := os.Getenv("PORT")
	if port == "" {
		port = "8080"
	}
	storePath := os.Getenv("STORE_PATH")
	if storePath == "" {
		storePath = "zezyi_stats.json"
	}
	store := NewStore(storePath)
	mm := NewMatchmakerWithStore(store)

	http.HandleFunc("/ws", func(w http.ResponseWriter, r *http.Request) {
		conn, err := upgrader.Upgrade(w, r, nil)
		if err != nil {
			log.Printf("upgrade error: %v", err)
			return
		}
		client := NewClient(conn, mm)
		go client.readPump()
		go client.writePump()
	})

	http.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusOK)
		w.Write([]byte("ok"))
	})

	http.HandleFunc("/leaderboard", func(w http.ResponseWriter, r *http.Request) {
		entries := store.Leaderboard(20)
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{"entries": convertEntries(entries)})
	})

	http.HandleFunc("/stats", func(w http.ResponseWriter, r *http.Request) {
		name := r.URL.Query().Get("name")
		if name == "" {
			http.Error(w, "missing name", 400)
			return
		}
		stats := store.Get(name)
		if stats == nil {
			np := NewPlayerStats(name)
			stats = np
		}
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(stats)
	})

	http.HandleFunc("/leaderboard/sprint", func(w http.ResponseWriter, r *http.Request) {
		records := store.GetTimeAttackRecords(1)
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(records)
	})
	http.HandleFunc("/leaderboard/ultra", func(w http.ResponseWriter, r *http.Request) {
		records := store.GetTimeAttackRecords(2)
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(records)
	})
	http.HandleFunc("/leaderboard/survival", func(w http.ResponseWriter, r *http.Request) {
		records := store.GetTimeAttackRecords(4)
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(records)
	})
	http.HandleFunc("/submit", func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodPost {
			http.Error(w, "POST only", 405)
			return
		}
		var rec TimeAttackRecord
		if err := json.NewDecoder(r.Body).Decode(&rec); err != nil {
			http.Error(w, "bad json", 400)
			return
		}
		if rec.Name == "" {
			http.Error(w, "missing name", 400)
			return
		}
		store.SubmitTimeAttack(rec.Name, rec.Mode, rec.TimeMs, rec.Score)
		w.WriteHeader(http.StatusOK)
		w.Write([]byte("ok"))
	})

	tcpPort := os.Getenv("TCP_PORT")
	if tcpPort == "" {
		tcpPort = "8081"
	}
	startTCPServer(tcpPort, mm)

	log.Printf("zezyi server listening on :%s (store=%s)", port, storePath)
	if err := http.ListenAndServe(":"+port, nil); err != nil {
		log.Fatal(err)
	}
}

func startTCPServer(port string, mm *Matchmaker) {
	ln, err := net.Listen("tcp", ":"+port)
	if err != nil {
		log.Fatalf("TCP listen failed: %v", err)
	}
	log.Printf("zezyi TCP server listening on :%s", port)
	go func() {
		for {
			conn, err := ln.Accept()
			if err != nil {
				log.Printf("TCP accept error: %v", err)
				continue
			}
			client := NewTCPClient(conn, mm)
			go client.readPump()
			go client.writePump()
		}
	}()
}
