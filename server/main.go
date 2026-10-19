package main

import (
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
			return true // non-browser (health checks, wscat)
		}
		allowed := os.Getenv("ALLOWED_ORIGINS") // comma-separated, e.g. "https://example.com,https://app.example.com"
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
			// trim spaces
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

	mm := NewMatchmaker()

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

	tcpPort := os.Getenv("TCP_PORT")
	if tcpPort == "" {
		tcpPort = "8081"
	}
	startTCPServer(tcpPort, mm)

	log.Printf("zezyi server listening on :%s", port)
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
