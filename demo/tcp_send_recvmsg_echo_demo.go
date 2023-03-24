package main

import (
	"fmt"
	"net"
	"os"
	"syscall"
)

const (
	MAX_BUFFER_SIZE = 1024
)

func main() {
	// Create a TCP listener
	ln, err := net.Listen("tcp", ":8888")
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to create listener: %v\n", err)
		os.Exit(1)
	}
	fmt.Println("Server listening on port 8888...")

	defer func() {
		err = ln.Close()
		if err != nil {
			fmt.Fprintf(os.Stderr, "Failed to close listener: %v\n", err)
		}
	}()

	// Accept incoming connections and echo data back to clients
	for {
		conn, err := ln.Accept()
		if err != nil {
			fmt.Fprintf(os.Stderr, "Failed to accept connection: %v\n", err)
			continue
		}

		fmt.Printf("Accepted new connection from %s\n", conn.RemoteAddr())

		// Receive data from the client
		buffer := make([]byte, MAX_BUFFER_SIZE)

		s := conn.(*net.TCPConn)
		f, err := s.File()

		n, _, _, from, err := syscall.Recvmsg(int(f.Fd()), buffer, []byte{}, 0)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Failed to receive data from client: %v\n", err)
			conn.Close()
			continue
		}

		fmt.Printf("Received %d bytes from client %s: %s\n", n, conn.RemoteAddr(), string(buffer[:n]))

		err = syscall.Sendmsg(int(f.Fd()), buffer, []byte{}, from, 0)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Failed to send data to client: %v\n", err)
			conn.Close()
			continue
		}

		fmt.Printf("Echoed %d bytes to client %s\n", n, conn.RemoteAddr())

		// Close the connection
		/*
			err = conn.Close()
			if err != nil {
				fmt.Fprintf(os.Stderr, "Failed to close connect: %v\n", err)
			}
			fmt.Printf("closed conn\n")
		*/
	}
}
