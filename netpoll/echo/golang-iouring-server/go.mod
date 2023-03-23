module github.com/weedge/iowrapper/netpoll/echo/golang-iouring-server

go 1.19

require (
	github.com/iceber/iouring-go v0.0.0-20230308084639-d71579e9084b
	github.com/ii64/gouring v0.4.1
)

require golang.org/x/sys v0.1.0 // indirect

// replace github.com/ii64/gouring => ../../../../gouring
