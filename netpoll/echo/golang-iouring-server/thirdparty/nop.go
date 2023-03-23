//go:build !goiouring && !iouringgo
// +build !goiouring,!iouringgo

package thirdparty

import "log"

func IOurigGoEchoServer() {
	log.Println("hi, hello world")
}
