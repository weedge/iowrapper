package main

import (
	"log"
	"net/http"
	_ "net/http/pprof"

	"github.com/weedge/iowrapper/netpoll/echo/golang-iouring-server/thirdparty"
)

func main() {

	go func() {
		if err := http.ListenAndServe(":6060", nil); err != nil {
			log.Fatalf("pprof failed: %v", err)
		}
	}()

	thirdparty.IOurigGoEchoServer()
}
