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

var (
	buffs    [][]byte
	mapBuffs = map[int][]byte{}
)

// for test just use a fixed buffer
// if use map[conn][]buff, some GC happen
func initBuffs() [][]byte {
	buffs := make([][]byte, MaxConns)
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

const (
	entries = 10240
)

var clientAddr syscall.RawSockaddrAny
var clientAddrLen uint32 = syscall.SizeofSockaddrAny

func IOurigGoEchoServer() {
	if len(os.Args) < 2 {
		log.Fatalf("Usage: %s <host:port> (<mod>) \n", os.Args[0])
	}
	lfd, err := Listen(os.Args[1])
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
		}
	}

	ring, err := gouring.NewWithParams(uint32(entries), params)
	if err != nil {
		log.Fatalf("io_uring_init err:%s", err.Error())
	}

	if params.Features&gouring.IORING_FEAT_FAST_POLL == 0 {
		log.Fatalf("IORING_FEAT_FAST_POLL not available in the kernel, quiting...")
	}

	defer ring.Close()

	//todo: provide buffer or fixed buffer for kernel space; like buffer pool for user space

	// trigger start: accept connect
	ProduceSocketListenAcceptSqe(ring, lfd, 0)

	for {
		_, err := ring.SubmitAndWait(1)
		if err != nil {
			log.Printf("[error] ring.SubmitAndWait(1) %s", err.Error())
			continue
		}
		var cqe *gouring.IoUringCqe
		err = ring.WaitCqe(&cqe)
		if err != nil {
			log.Printf("[error] ring.WaitCqe %s", err.Error())
			continue
		}

		eventInfo := (*EventInfo)(cqe.UserData.GetUnsafe())
		switch eventInfo.etype {
		case ETypeAccept:
			connFd := cqe.Res
			if connFd < 0 {
				log.Printf("[error] connect failed")
			} else {
				// IOSQE_BUFFER_SELECT: select buffer for read with IORING_OP_PROVIDE_BUFFERS command
				//ProduceSocketConnRecvMsgSqe(ring, int(connFd),gouring.IOSQE_BUFFER_SELECT)
				ProduceSocketConnRecvMsgSqe(ring, int(connFd), 0)
			}

			// new connected client; read data from socket and re-add accept to
			// monitor for new connections
			ProduceSocketListenAcceptSqe(ring, lfd, 0)

		case ETypeRead:
			readBytesLen := cqe.Res
			if readBytesLen < 0 {
				// read failed
				// connection closed or error
				syscall.Close(eventInfo.cfd)
				break
			}
			if readBytesLen == 0 {
				log.Printf("[warn] empty request!\n")
				break
			}
			// bytes have been read into connected fd bufs, now add write to socket sqe
			ProduceSocketConnSendMsgSqe(ring, eventInfo.cfd, uint64(readBytesLen), 0)

		case ETypeWrite:
			ProduceSocketConnRecvMsgSqe(ring, int(eventInfo.cfd), 0)
		}

		ring.SeenCqe(cqe)
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

func ProduceSocketConnRecvMsgSqe(ring *gouring.IoUring, cfd int, flags uint8) {
	buff, ok := mapBuffs[cfd]
	if !ok {
		buff = make([]byte, MaxMsgLen)
		mapBuffs[cfd] = buff
	}

	sqe := ring.GetSqe()
	// man readv
	msghdr := &syscall.Msghdr{
		Iov: &syscall.Iovec{
			Base: &buff[0],
			Len:  MaxMsgLen,
		},
		Iovlen: 1,
	}
	gouring.PrepRecvmsg(sqe, cfd, msghdr, 0)
	sqe.Flags = flags

	connInfo := &EventInfo{
		cfd:   cfd,
		etype: ETypeRead,
	}
	sqe.UserData.SetUnsafe(unsafe.Pointer(connInfo))
}

func ProduceSocketConnSendMsgSqe(ring *gouring.IoUring, cfd int, msgSize uint64, flags uint8) {
	buff, ok := mapBuffs[cfd]
	if !ok {
		log.Printf("cfd:%d no buff,maybe closed; so make empty string echo", cfd)
		buff = make([]byte, MaxMsgLen)
		mapBuffs[cfd] = buff
	}
	sqe := ring.GetSqe()
	// man readv
	msghdr := &syscall.Msghdr{
		Iov: &syscall.Iovec{
			Base: &buff[0],
			Len:  msgSize,
		},
		Iovlen: 1,
	}
	gouring.PrepSendmsg(sqe, cfd, msghdr, 0)
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
