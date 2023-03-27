//go:build goiouring
// +build goiouring

package thirdparty

import (
	"errors"
	"log"
	"os"
	"strconv"
	"strings"
	"syscall"
	"unsafe"

	"github.com/ii64/gouring"
)

type EventInfo struct {
	lfd   int    // listen fd
	cfd   int    // connect fd
	etype uint16 // event type
	bid   uint16 // buff id in pool group
	gid   uint16 // buff group id
}

const (
	ETypeUnknow = iota
	ETypeAccept
	ETypeRead
	ETypeWrite
	ETypeProvidBuff
)

const (
	MaxConns  = 10240
	MaxMsgLen = 2048
)

const (
	Entries = 10240
)

var (
	buffs    [][]byte
	mapBuffs = map[int][]byte{}
)
var clientAddr syscall.RawSockaddrAny
var clientAddrLen uint32 = syscall.SizeofSockaddrAny

// for test just use a fixed buffer
// if use map[conn][]buff, some GC happen
func InitBuffs() [][]byte {
	buffs := make([][]byte, MaxConns+32)
	for i := range buffs {
		buffs[i] = make([]byte, MaxMsgLen)
	}
	return buffs
}

func GetIPPort(address string) (ip [4]byte, port int, err error) {
	strs := strings.Split(address, ":")
	if len(strs) != 2 {
		err = errors.New("addr error")
		return
	}

	if len(strs[0]) != 0 {
		ips := strings.Split(strs[0], ".")
		if len(ips) != 4 {
			err = errors.New("addr error")
			return
		}
		for i := range ips {
			data, err := strconv.Atoi(ips[i])
			if err != nil {
				return ip, 0, err
			}
			ip[i] = byte(data)
		}
	}

	port, err = strconv.Atoi(strs[1])
	return

}

func Listen(address string) (listenFD int, err error) {
	listenFD, err = syscall.Socket(syscall.AF_INET, syscall.SOCK_STREAM, 0)
	if err != nil {
		return
	}
	err = syscall.SetsockoptInt(listenFD, syscall.SOL_SOCKET, syscall.SO_REUSEADDR, 1)
	if err != nil {
		return
	}

	addr, port, err := GetIPPort(address)
	if err != nil {
		return
	}
	err = syscall.Bind(listenFD, &syscall.SockaddrInet4{
		Port: port,
		Addr: addr,
	})
	if err != nil {
		return
	}
	err = syscall.Listen(listenFD, 1024)
	if err != nil {
		return
	}

	log.Printf("listen addr %s port %d", addr, port)
	return
}

func IOurigGoEchoServer() {
	if len(os.Args) < 2 {
		log.Fatalf("Usage: %s <host:port> (<mod>) \n", os.Args[0])
	}
	addr := os.Args[1]
	if len(strings.Split(addr, ":")) == 1 {
		addr = ":" + os.Args[1]
	}

	lfd, err := Listen(addr)
	if err != nil {
		log.Fatalf("listen err:%s", err.Error())
	}

	params := &gouring.IoUringParams{}
	if len(os.Args) >= 3 {
		switch os.Args[2] {
		case "sqp":
			params.Flags |= gouring.IORING_SETUP_SQPOLL
			params.SqThreadCpu = uint32(1)
			params.SqThreadIdle = uint32(10000)
			println("sql mod setup")
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

	buffs = InitBuffs()

	//todo: provide buffer or fixed buffer for kernel space; like buffer pool for user space

	// trigger start: accept connect
	ProduceSocketListenAcceptSqe(ring, lfd, 0)

	for {
		_, err := ring.SubmitAndWait(1)
		if err != nil {
			log.Printf("[error] ring submit %s", err.Error())
			return
		}

		var cqe *gouring.IoUringCqe
		err = ring.WaitCqe(&cqe)
		if err != nil {
			log.Printf("[error] ring.WaitCqe %s", err.Error())
			continue
		}

		eventInfo := (*EventInfo)(cqe.UserData.GetUnsafe())
		//log.Printf("eventInfo: %+v res:%+v", eventInfo, cqe.Res)

		switch eventInfo.etype {
		case ETypeAccept:
			connFd := cqe.Res
			ring.SeenCqe(cqe)
			if connFd < 0 {
				log.Printf("[error] connect failed connFd %d\n", connFd)
				break
			}

			//log.Printf("Accepted new connection %d from %+v\n", connFd, clientAddr)

			// new connected client; read data from socket and re-add accept to
			// monitor for new connections
			ProduceSocketListenAcceptSqe(ring, lfd, 0)
			// IOSQE_BUFFER_SELECT: select buffer for read with IORING_OP_PROVIDE_BUFFERS command
			//ProduceSocketConnRecvMsgSqe(ring, int(connFd),gouring.IOSQE_BUFFER_SELECT)
			ProduceSocketConnRecvSqe(ring, int(connFd), 0)

		case ETypeRead:
			readBytesLen := cqe.Res
			ring.SeenCqe(cqe)
			if readBytesLen <= 0 {
				log.Printf("[error] read errNO %d", cqe.Res)
				// no bytes available on socket, client must be disconnected
				//syscall.Shutdown(lfd, syscall.SHUT_RDWR)
				syscall.Close(eventInfo.cfd)
				break
			}

			//log.Printf("Received %d bytes from client %+v\n", readBytesLen, clientAddr)

			// bytes have been read into connected fd bufs, now add write to socket sqe
			//ProduceSocketConnSendMsgSqe(ring, eventInfo.cfd, &clientAddr, int(readBytesLen), 0)
			ProduceSocketConnSendSqe(ring, eventInfo.cfd, int(readBytesLen), 0)

		case ETypeWrite:
			writeBytesLen := cqe.Res
			ring.SeenCqe(cqe)
			if writeBytesLen < 0 {
				// write failed
				// connection closed or error
				log.Printf("[error] write errNO %d", cqe.Res)
				syscall.Close(eventInfo.cfd)
				break
			}
			if writeBytesLen == 0 {
				log.Printf("[warn] empty response!\n")
			}

			//log.Printf("Echoed %d bytes to client %+v\n", writeBytesLen, clientAddr)

			//ProduceSocketConnRecvMsgSqe(ring, eventInfo.cfd, &clientAddr, 0)
			ProduceSocketConnRecvSqe(ring, eventInfo.cfd, 0)

		default:
			ring.SeenCqe(cqe)
			log.Panicf("unsupport event type %d\n", eventInfo.etype)
		}
	}

}

func ProduceSocketListenAcceptSqe(ring *gouring.IoUring, lfd int, flags uint8) {
	sqe := ring.GetSqe()
	gouring.PrepAccept(sqe, lfd, &clientAddr, (*uintptr)(unsafe.Pointer(&clientAddrLen)), 0)
	sqe.Flags = flags

	connInfo := &EventInfo{
		lfd:   lfd,
		etype: ETypeAccept,
	}
	sqe.UserData.SetUnsafe(unsafe.Pointer(connInfo))
}

func ProduceSocketConnRecvSqe(ring *gouring.IoUring, cfd int, flags uint8) {
	buff := buffs[cfd]

	sqe := ring.GetSqe()
	gouring.PrepRecv(sqe, cfd, &buff[0], len(buff), uint(flags))
	sqe.Flags = flags

	connInfo := EventInfo{
		cfd:   cfd,
		etype: ETypeRead,
	}
	sqe.UserData.SetUnsafe(unsafe.Pointer(&connInfo))

}

func ProduceSocketConnSendSqe(ring *gouring.IoUring, cfd int, msgSize int, flags uint8) {
	buff := buffs[cfd]

	sqe := ring.GetSqe()
	gouring.PrepSend(sqe, cfd, &buff[0], msgSize, uint(flags))
	sqe.Flags = flags

	connInfo := EventInfo{
		cfd:   cfd,
		etype: ETypeWrite,
	}
	sqe.UserData.SetUnsafe(unsafe.Pointer(&connInfo))

}

func ProduceSocketConnRecvMsgSqe(ring *gouring.IoUring, cfd int, rsa *syscall.RawSockaddrAny, flags uint8) {
	buff := buffs[cfd]

	var msghdr syscall.Msghdr
	msghdr.Name = (*byte)(unsafe.Pointer(rsa))
	msghdr.Namelen = uint32(syscall.SizeofSockaddrAny)
	var iov syscall.Iovec
	iov.Base = &buff[0]
	iov.SetLen(len(buff))
	msghdr.Iov = &iov
	msghdr.Iovlen = 1

	sqe := ring.GetSqe()
	gouring.PrepRecvmsg(sqe, cfd, &msghdr, 0)
	sqe.Flags = flags

	connInfo := EventInfo{
		cfd:   cfd,
		etype: ETypeRead,
	}
	sqe.UserData.SetUnsafe(unsafe.Pointer(&connInfo))
}

func ProduceSocketConnSendMsgSqe(ring *gouring.IoUring, cfd int, rsa *syscall.RawSockaddrAny, msgSize int, flags uint8) {
	//buff := buffs[cfd][:msgSize]
	buff := buffs[cfd]

	var msghdr syscall.Msghdr
	msghdr.Name = (*byte)(unsafe.Pointer(rsa))
	msghdr.Namelen = uint32(syscall.SizeofSockaddrAny)
	var iov syscall.Iovec
	iov.Base = &buff[0]
	iov.SetLen(int(msgSize))
	msghdr.Iov = &iov
	msghdr.Iovlen = 1

	sqe := ring.GetSqe()
	gouring.PrepSendmsg(sqe, cfd, &msghdr, 0)
	sqe.Flags = flags

	connInfo := &EventInfo{
		cfd:   cfd,
		etype: ETypeWrite,
	}
	sqe.UserData.SetUnsafe(unsafe.Pointer(connInfo))
}

func ProduceSocketConnRecvMsgSqeByBuff(ring *gouring.IoUring, cfd int, guid uint16, msgSize int, flags uint8) {
}
func ProduceSocketConnSendMsgSqeByBuff(ring *gouring.IoUring, cfd int, bid uint16, msgSize int, flags uint8) {
}

func InitProvideBuffer() {

}
func AddProvideBuffer() {

}

func InitFixedBuffer() {

}
func AddFixedBuffer() {

}
