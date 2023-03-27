package main

import (
	"flag"
	"io"
	"log"
	"net"
	"net/http"
	_ "net/http/pprof"
	"strconv"
)

const MaxConns = 10240
const MaxMsgLen = 2048

var buffs [][]byte

func initBuffs() [][]byte {
	buffs := make([][]byte, MaxConns)
	for i := range buffs {
		buffs[i] = make([]byte, MaxMsgLen)
	}
	return buffs
}

func main() {
	flag.Parse()
	port, err := strconv.Atoi(flag.Arg(0))
	if err != nil {
		log.Fatal(err.Error())
	}

	go func() {
		if err := http.ListenAndServe(":6060", nil); err != nil {
			log.Fatalf("pprof failed: %v", err)
		}
	}()

	listener, err := net.ListenTCP("tcp", &net.TCPAddr{Port: port})
	checkErr(err)

	buffs = initBuffs()
	for {
		conn, err := listener.Accept()
		checkErr(err)
		go handleConn(conn)
	}
}

func handleConn(conn net.Conn) {
	f, _ := conn.(*net.TCPConn).File()
	buff := buffs[int(f.Fd())]
	//buff := make([]byte, MaxMsgLen)
	for {
		n, err := conn.Read(buff)
		if err == io.EOF || n == 0 {
			checkErr(conn.Close())
			return
		}
		checkErr(err)

		_, err = conn.Write(buff[:n])
		checkErr(err)
	}
}

func checkErr(err error) {
	if err != nil {
		log.Fatal(err)
	}
}
