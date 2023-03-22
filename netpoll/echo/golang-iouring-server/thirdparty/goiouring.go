//go:build goiouring
// +build goiouring

package thirdparty

import (
	"log"

	_ "github.com/ii64/gouring"
)

func IOurigGoEchoServer() {
	log.Println("Ops...")
}
