package main

import (
	"log"
	"net/http"
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

func main() {
	go func() {
		if err := http.ListenAndServe(":6060", nil); err != nil {
			log.Fatalf("pprof failed: %v", err)
		}
	}()
	server, err := poller.NewServer(":8081", &MockServerHandler{}, &MockDecoder{},
		poller.WithTimeout(10*time.Second, 3600*time.Second), poller.WithReadBufferLen(128))
	if err != nil {
		log.Println("err")
		return
	}

	server.Run()
}
