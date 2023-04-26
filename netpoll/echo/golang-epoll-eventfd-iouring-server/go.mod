module github.com/weedge/iowrapper/netpoll/echo/golang-epoll-eventfd-iouring-server

go 1.19

require (
	github.com/ii64/gouring v0.4.1
	golang.org/x/sys v0.7.0
)

replace github.com/ii64/gouring => github.com/weedge/gouring v0.0.0-20230424045338-0bb8d1621980

//replace github.com/ii64/gouring => ../../../../gouring
