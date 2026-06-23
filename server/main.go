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
		return true
	},
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
