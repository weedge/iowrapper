package main

import (
	"flag"
	"net/http"
	_ "net/http/pprof"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/weedge/lib/log"
	"github.com/weedge/lib/poller"
)

type MockServerHandler struct {
}

func (m *MockServerHandler) OnConnect(c *poller.Conn) {
	log.Infof("connect fd %d addr %s", c.GetFd(), c.GetAddr())
}

func (m *MockServerHandler) OnMessage(c *poller.Conn, bytes []byte) {
	log.Infof("read:%s len:%d bytes from fd:%d", bytes, len(bytes), c.GetFd())
	c.Write(bytes)
}

func (m *MockServerHandler) OnClose(c *poller.Conn, err error) {
	log.Infof("close: %d err: %s", c.GetFd(), err.Error())
}

var port = flag.String("port", "8081", "port")
var msgSize = flag.Int("size", 512, "size")

func main() {
	flag.Parse()

	go func() {
		if err := http.ListenAndServe(":6060", nil); err != nil {
			log.Errorf("pprof failed: %v", err)
			return
		}
	}()

	server, err := poller.NewServer(":"+*port, &MockServerHandler{},
		poller.WithTimeout(10*time.Second, 3600*time.Second), poller.WithReadBufferLen(*msgSize))
	if err != nil {
		log.Info("err ", err.Error())
		return
	}

	go server.Run()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Info("server stop")
	server.Stop()
}
