package main

import (
	"fmt"
	"log"
	"net"
	"os"
	"syscall"
	"unsafe"

	"github.com/ii64/gouring"
)

const (
	MAX_BUFFER_SIZE = 1024
	Entries         = 1024
)

func main() {
	// Create a TCP listener
	ln, err := net.Listen("tcp", ":8888")
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to create listener: %v\n", err)
		os.Exit(1)
	}

	tcpln := ln.(*net.TCPListener)
	cf, _ := tcpln.File()
	ln.Close()

	lfd := cf.Fd() // 拿到对应的fd
	println(lfd)

	fmt.Println("Server listening on port 8888...", "listen socket fd", lfd)

	defer func() {
		err = tcpln.Close()
		if err != nil {
			fmt.Fprintf(os.Stderr, "Failed to close listener: %v\n", err)
		}
	}()

	params := &gouring.IoUringParams{}
	if len(os.Args) >= 2 {
		switch os.Args[1] {
		case "sqp":
			params.Flags |= gouring.IORING_SETUP_SQPOLL
			params.SqThreadCpu = uint32(1)
			params.SqThreadIdle = uint32(10000)
			fmt.Printf("sqp mod\n")
		}
	}

	ring, err := gouring.NewWithParams(uint32(Entries), params)
	if err != nil {
		log.Fatalf("io_uring_init err:%s", err.Error())
	}

	if params.Features&gouring.IORING_FEAT_FAST_POLL == 0 {
		log.Fatalf("IORING_FEAT_FAST_POLL not available in the kernel, quiting...")
	}

	defer ring.Close()

	// Accept incoming connections and echo data back to clients
	for {
		var clientAddr syscall.RawSockaddrAny
		var clientAddrLen uint32 = syscall.SizeofSockaddrAny

		sqe := ring.GetSqe()
		gouring.PrepAccept(sqe, int(lfd), &clientAddr, (*uintptr)(unsafe.Pointer(&clientAddrLen)), 0)
		ring.Submit()
		var cqe *gouring.IoUringCqe
		err = ring.WaitCqe(&cqe)
		if err != nil {
			log.Printf("[error] ring.WaitCqe %s", err.Error())
			continue
		}
		ring.SeenCqe(cqe)
		fd := cqe.Res
		if fd < 0 {
			log.Printf("[error] connect failed connFd %d\n", fd)
			break
		}

		log.Printf("Accepted new connection from %+v\n", clientAddr)

		/*
			conn, err := ln.Accept()
			if err != nil {
				fmt.Fprintf(os.Stderr, "Failed to accept connection: %v\n", err)
				continue
			}
			fmt.Printf("Accepted new connection from %s\n", conn.RemoteAddr())
			s := conn.(*net.TCPConn)
			f, _ := s.File()
			fd := f.Fd()
		*/

		// Receive data from the client
		//buffer := make([]byte, MAX_BUFFER_SIZE)
		buffer := [MAX_BUFFER_SIZE]byte{}

		/*
			n, _, _, from, err := syscall.Recvmsg(int(fd), buffer[:], []byte{}, 0)
			if err != nil {
				fmt.Fprintf(os.Stderr, "Failed to receive data from client: %v\n", err)
				conn.Close()
				continue
			}
		*/

		var msghdr syscall.Msghdr
		msghdr.Name = (*byte)(unsafe.Pointer(&clientAddr))
		msghdr.Namelen = uint32(syscall.SizeofSockaddrAny)
		var iov syscall.Iovec
		iov.Base = &buffer[0]
		iov.SetLen(len(buffer))
		msghdr.Iov = &iov
		msghdr.Iovlen = 1

		sqe = ring.GetSqe()
		gouring.PrepRecvmsg(sqe, int(fd), &msghdr, 0)
		ring.Submit()
		err = ring.WaitCqe(&cqe)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Failed to receive data from client: %v\n", err)
			syscall.Close(int(fd))
			continue
		}
		ring.SeenCqe(cqe)
		n := cqe.Res
		if n < 0 {
			fmt.Fprintf(os.Stderr, "received failed errNo: %d\n", n)
			syscall.Close(int(fd))
			continue
		}
		if n == 0 {
			fmt.Fprintf(os.Stderr, "received empty \n")
			continue
		}

		//fmt.Printf("Received %d bytes from client %s: %s\n", n, conn.RemoteAddr(), string(buffer[:n]))
		fmt.Printf("Received %d bytes from client %+v: %s\n", n, clientAddr, string(buffer[:n]))

		//err = syscall.Sendmsg(int(fd), buffer[:], []byte{}, from, 0)

		var msg syscall.Msghdr
		msg.Name = (*byte)(unsafe.Pointer(&clientAddr))
		msg.Namelen = uint32(syscall.SizeofSockaddrAny)
		iov.Base = &buffer[0]
		iov.SetLen(int(n))
		msg.Iov = &iov
		msg.Iovlen = 1

		sqe = ring.GetSqe()
		gouring.PrepSendmsg(sqe, int(fd), &msg, 0)
		ring.Submit()
		err = ring.WaitCqe(&cqe)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Failed to send data to client: %v\n", err)
			syscall.Close(int(fd))
			continue
		}
		ring.SeenCqe(cqe)
		m := cqe.Res
		if m < 0 {
			fmt.Fprintf(os.Stderr, "send failed errNo: %d\n", m)
			syscall.Close(int(fd))
			continue
		}

		//fmt.Printf("Echoed %d bytes to client %s\n", n, conn.RemoteAddr())
		fmt.Printf("Echoed %d bytes to client %+v\n", n, clientAddr)

		// Close the connection
		//err = conn.Close()
		err = syscall.Close(int(fd))
		if err != nil {
			fmt.Fprintf(os.Stderr, "Failed to close connect: %v\n", err)
		}
		fmt.Printf("closed connfd %d\n", fd)
	}
}
