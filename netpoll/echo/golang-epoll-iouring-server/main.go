package main

import (
	"errors"
	"flag"
	"fmt"
	"log"
	"net/http"
	_ "net/http/pprof"
	"os"
	"os/signal"
	"runtime"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"unsafe"

	"github.com/ii64/gouring"
	"golang.org/x/sys/unix"
)

const (
	EpollReadEvents = unix.EPOLLIN | unix.EPOLLET
)

type EventCallBack func(info *EventInfo) error
type EventInfo struct {
	lfd   int    // listen fd
	cfd   int    // connect fd
	etype uint16 // event type
	bid   uint16 // buff id in pool group
	gid   uint16 // buff group id
	cb    EventCallBack
	cqe   gouring.IoUringCqe
	ring  *gouring.IoUring
}

const (
	ETypeUnknow = iota
	ETypeAccept
	ETypeRead
	ETypeWrite
	ETypeProvidBuff
	ETypeClose
)

const (
	MaxConns  = 10240
	MaxMsgLen = 2048
)

const (
	Entries = 2048
)

var (
	buffs [][]byte
)
var clientAddr syscall.RawSockaddrAny
var clientAddrLen uint32 = syscall.SizeofSockaddrAny

var mapEvent map[gouring.UserData]*EventInfo
var userDataEventLock sync.RWMutex // rwlock for mapUserDataEvent

var ringCn = flag.Int("ringCn", 0, "ring cn")
var port = flag.String("port", "8888", "port")
var mode = flag.String("mode", "", "sqp")

func init() {
	buffs = InitBuffs()
	mapEvent = make(map[gouring.UserData]*EventInfo)
}

func main() {
	flag.Parse()

	go func() {
		if err := http.ListenAndServe(":6060", nil); err != nil {
			log.Fatalf("pprof failed: %v", err)
		}
	}()

	lfd := listen(":" + *port)

	ring, err := initIouring(0)
	if err != nil {
		log.Fatalf("initIouring failed: %v", err)
	}

	eventfd, err := registerEventFd(ring)
	if err != nil {
		log.Fatalf("registerEventFd failed: %v", err)
	}

	signalCh := make(chan struct{}, 1)
	stopCh := make(chan struct{})
	go subCqeEventInfo(ring, signalCh, stopCh)
	err = initEpollWaitEventFD(eventfd, signalCh)
	if err != nil {
		log.Fatalf("initEpollWaitEventFD failed: %v", err)
	}
	log.Println("start server ok, runing...")

	// trigger start: accept connect
	ProduceSocketListenAcceptSqe(accpetCb, ring, lfd, 0)

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	close(stopCh)
	ring.Close()
}

func accpetCb(info *EventInfo) error {
	connFd := info.cqe.Res
	if connFd < 0 {
		err := fmt.Errorf("[error] connect failed connFd %d", connFd)
		return err
	}

	ProduceSocketListenAcceptSqe(accpetCb, info.ring, info.lfd, 0)
	ProduceSocketConnRecvSqe(readCb, info.ring, int(connFd), 0)

	return nil
}

func readCb(info *EventInfo) error {
	readBytesLen := info.cqe.Res
	if readBytesLen <= 0 {
		if info.cqe.Res != -int32(syscall.EBADF) {
			ProduceSocketConnCloseSqe(closeCb, info.ring, info.cfd)
		}

		err := fmt.Errorf("[error] read errNO %d connectFd %d", info.cqe.Res, info.cfd)
		return err
	}

	//log.Printf("Received %d bytes from client %+v\n", readBytesLen, clientAddr)

	ProduceSocketConnSendSqe(writeCb, info.ring, info.cfd, int(readBytesLen), 0)
	return nil
}

func writeCb(info *EventInfo) error {
	ProduceSocketConnRecvSqe(readCb, info.ring, info.cfd, 0)

	return nil
}

func closeCb(info *EventInfo) error {
	log.Printf("close cqeRes %d connectFD %d \n", info.cqe.Res, info.cfd)
	return nil
}

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

	//log.Printf("listen addr %s port %d\n", addr, port)
	return
}

func listen(addr string) (lfd int) {
	if len(strings.Split(addr, ":")) == 1 {
		addr = ":" + os.Args[1]
	}

	lfd, err := Listen(addr)
	if err != nil {
		log.Fatalf("listen err:%s", err.Error())
	}

	return
}

func initIouring(id int) (ring *gouring.IoUring, err error) {
	params := &gouring.IoUringParams{}
	switch *mode {
	case "sqp":
		params.Flags |= gouring.IORING_SETUP_SQPOLL | gouring.IORING_SETUP_SQ_AFF
		params.SqThreadCpu = uint32(id % runtime.NumCPU())
		params.SqThreadIdle = uint32(10_000) // 10s
		println("id", id, "sqp mod setup")
	}

	ring, err = gouring.NewWithParams(uint32(Entries), params)
	if err != nil {
		log.Fatalf("id %d io_uring_init err:%s ring %v", id, err.Error(), ring)
	}

	if params.Features&gouring.IORING_FEAT_FAST_POLL == 0 {
		log.Fatalf("IORING_FEAT_FAST_POLL not available in the kernel, quiting...")
	}

	return
}

func registerEventFd(ring *gouring.IoUring) (eventfd int, err error) {
	eventfd, err = unix.Eventfd(0, unix.EFD_NONBLOCK|unix.EFD_CLOEXEC)
	if err != nil {
		return
	}

	err = ring.RegisterEventFd(eventfd)
	if err != nil {
		return
	}

	return
}

func initEpollWaitEventFD(eventFD int, signCh chan<- struct{}) (err error) {
	pollFD, err := unix.EpollCreate1(unix.EPOLL_CLOEXEC)
	if err != nil {
		return
	}

	err = unix.EpollCtl(pollFD, unix.EPOLL_CTL_ADD, eventFD, &unix.EpollEvent{
		Events: EpollReadEvents,
		Fd:     int32(eventFD),
	})

	go func() {
		waitIouringEventFdEvents(pollFD, signCh)
	}()

	return
}

func waitIouringEventFdEvents(pollFD int, signCh chan<- struct{}) {
	epollEvents := make([]unix.EpollEvent, 100)
	for {
		n, err := unix.EpollWait(pollFD, epollEvents, -1)
		if err != nil {
			continue
		}
		for i := 0; i < n; i++ {
			signCh <- struct{}{}
		}
	}
}

func subCqeEventInfo(ring *gouring.IoUring, signCh, stopCh <-chan struct{}) {
	for {
		select {
		case <-signCh:
			var cqe *gouring.IoUringCqe
			err := ring.PeekCqe(&cqe)
			if err != nil {
				log.Printf("peekCqe err %s \n", err.Error())
				continue
			}
			if cqe == nil {
				log.Println("cqe is nil")
				continue
			}

			userDataEventLock.Lock()
			info, ok := mapEvent[cqe.UserData]
			if !ok {
				errStr := fmt.Sprintf("cqe %+v userData %d get event info: %+v empty", cqe, cqe.UserData, info)
				userDataEventLock.Unlock()
				log.Println(errStr)
				ring.SeenCqe(cqe)
				continue
			}
			if info != nil && (info.cb == nil || info.etype == ETypeUnknow) {
				userDataEventLock.Unlock()
				log.Println("[error] event infoPtr unknow")
				ring.SeenCqe(cqe)
				continue
			}
			delete(mapEvent, cqe.UserData)
			info.cqe = *cqe
			userDataEventLock.Unlock()

			ring.SeenCqe(cqe)
			err = info.cb(info)
			if err != nil {
				log.Printf("[error] cb error %s", err.Error())
				continue
			}

			//log.Printf("[debug] cqe %+v userData %d get event info: %+v callback ok", cqe, cqe.UserData, info)
		case <-stopCh:
			return
		}
	}
}

func ProduceSocketListenAcceptSqe(cb EventCallBack, ring *gouring.IoUring, lfd int, flags uint8) {
	sqe := ring.GetSqe()
	gouring.PrepAccept(sqe, lfd, &clientAddr, (*uintptr)(unsafe.Pointer(&clientAddrLen)), 0)
	sqe.Flags = flags

	connInfo := &EventInfo{
		lfd:   lfd,
		etype: ETypeAccept,
		cb:    cb,
		ring:  ring,
	}
	submit(ring, sqe, connInfo)
}

func ProduceSocketConnRecvSqe(cb EventCallBack, ring *gouring.IoUring, cfd int, flags uint8) {
	buff := buffs[cfd]

	sqe := ring.GetSqe()
	gouring.PrepRecv(sqe, cfd, &buff[0], len(buff), uint(flags))
	//gouring.PrepRecv(sqe, cfd, &buff[0], 0, uint(flags))
	sqe.Flags = flags

	connInfo := &EventInfo{
		cfd:   cfd,
		etype: ETypeRead,
		cb:    cb,
		ring:  ring,
	}
	submit(ring, sqe, connInfo)
}

func ProduceSocketConnSendSqe(cb EventCallBack, ring *gouring.IoUring, cfd int, msgSize int, flags uint8) {
	buff := buffs[cfd]

	sqe := ring.GetSqe()
	gouring.PrepSend(sqe, cfd, &buff[0], msgSize, uint(flags))
	sqe.Flags = flags

	connInfo := &EventInfo{
		cfd:   cfd,
		etype: ETypeWrite,
		cb:    cb,
		ring:  ring,
	}
	submit(ring, sqe, connInfo)
}

func ProduceSocketConnCloseSqe(cb EventCallBack, ring *gouring.IoUring, cfd int) {
	sqe := ring.GetSqe()
	gouring.PrepClose(sqe, cfd)

	connInfo := &EventInfo{
		cfd:   cfd,
		etype: ETypeClose,
		cb:    cb,
		ring:  ring,
	}
	submit(ring, sqe, connInfo)
}

func submit(ring *gouring.IoUring, sqe *gouring.IoUringSqe, eventInfo *EventInfo) {
	//sqe.UserData.SetUnsafe(unsafe.Pointer(eventInfo))
	sqe.UserData = gouring.UserData(uintptr(unsafe.Pointer(eventInfo)))
	userDataEventLock.Lock()
	mapEvent[sqe.UserData] = eventInfo
	userDataEventLock.Unlock()
	_, err := ring.Submit()
	if err != nil {
		userDataEventLock.Lock()
		delete(mapEvent, sqe.UserData)
		userDataEventLock.Unlock()
		log.Printf("[error] submit eventInfo %+v fail", *eventInfo)
		return
	}
	//log.Printf("[debug] submit userData %d eventInfo %+v ok", sqe.UserData, *eventInfo)
}
