package main

import (
	"flag"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/weedge/lib/poller"
)

type MockDecoder struct {
}

func (*MockDecoder) Decode(c *poller.Conn) (err error) {
	buff := c.GetBuff()
	bytes := buff.ReadAll()
	//log.Printf("read:%s len:%d bytes from fd:%d", bytes, len(bytes), c.GetFd())
	_, err = syscall.Write(int(c.GetFd()), bytes)

	return
}

type MockServerHandler struct {
}

func (m *MockServerHandler) OnConnect(c *poller.Conn) {
	log.Printf("connect fd %d addr %s", c.GetFd(), c.GetAddr())
}

func (m *MockServerHandler) OnMessage(c *poller.Conn, bytes []byte) {
}

func (m *MockServerHandler) OnClose(c *poller.Conn, err error) {
	log.Printf("close: %d err: %s", c.GetFd(), err.Error())
}

var port = flag.String("port", "8081", "port")
var msgSize = flag.Int("size", 512, "size")

func main() {
	flag.Parse()

	go func() {
		if err := http.ListenAndServe(":6060", nil); err != nil {
			log.Fatalf("pprof failed: %v", err)
		}
	}()

	server, err := poller.NewServer(":"+*port, &MockServerHandler{}, &MockDecoder{},
		poller.WithTimeout(10*time.Second, 3600*time.Second), poller.WithReadBufferLen(*msgSize))
	if err != nil {
		log.Println("err")
		return
	}

	go server.Run()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Println("server stop")
	server.Stop()
}
