package iouring_test

import (
	"bytes"
	"fmt"
	"os"
	"path/filepath"
	"sync"
	"testing"
	"time"

	"github.com/weedge/iowrapper/iouring"
)

func TestAll(t *testing.T) {
	err := iouring.Init()
	if err != nil {
		fmt.Println(err)
		return
	}
	defer iouring.Cleanup()

	go func() {
		for err := range iouring.Err() {
			fmt.Println(err)
		}
	}()

	wg := &sync.WaitGroup{}
	wg.Add(1)
	go func() {
		// Read a file.
		defer wg.Done()
		err = iouring.ReadFile("../testdata/static.html", func(buf []byte) {
			// handle buf
			println("read file sleep 1 s")
			time.Sleep(1 * time.Second)
		})
		if err != nil {
			fmt.Println(err)
			return
		}
	}()

	wg.Add(1)
	go func() {
		defer wg.Done()
		// Write something
		err = iouring.WriteFile("../testdata/dummy.txt", []byte("hello world"), 0644, func(n int) {
			// handle n
			println("write file sleep 1 s")
			time.Sleep(1 * time.Second)
		})
		if err != nil {
			fmt.Println(err)
			return
		}
	}()

	// Call Poll to let the kernel know to read the entries.
	iouring.Poll()
	// Wait till all callbacks are done.
	wg.Wait()
}

func TestRead(t *testing.T) {
	err := iouring.Init()
	if err != nil {
		t.Fatal(err)
	}
	defer iouring.Cleanup()

	go func() {
		for err := range iouring.Err() {
			t.Error(err)
		}
	}()

	var wg sync.WaitGroup

	helper := func(file string) {
		wg.Add(1)
		expected, err := os.ReadFile(file)
		if err != nil {
			t.Error(err)
			return
		}
		err = iouring.ReadFile(file, func(buf []byte) {
			defer wg.Done()
			if !bytes.Equal(buf, expected) {
				t.Errorf("unexpected result. Got %v, expected %v", buf, expected)
			}
		})
		if err != nil {
			t.Error(err)
		}
		iouring.Poll()
		wg.Wait()
	}

	t.Run("ZeroByte", func(t *testing.T) {
		helper("../testdata/zero_byte.txt")
	})

	t.Run("TwoBytes", func(t *testing.T) {
		helper("../testdata/two_bytes.txt")
	})

	t.Run("MediumFile", func(t *testing.T) {
		helper("../testdata/static.html")
	})
	t.Run("MultipleOf7", func(t *testing.T) {
		helper("../testdata/shire.txt")
	})
}

func TestQueueThreshold(t *testing.T) {
	err := iouring.Init()
	if err != nil {
		t.Fatal(err)
	}
	defer iouring.Cleanup()

	go func() {
		for err := range iouring.Err() {
			t.Error(err)
		}
	}()
	expected, err := os.ReadFile("../testdata/static.html")
	if err != nil {
		t.Error(err)
		return
	}

	var wg sync.WaitGroup
	wg.Add(6)

	// Trigger 6 reads and verify that results come,
	// without needing to call Poll.
	for i := 0; i < 6; i++ {
		err = iouring.ReadFile("../testdata/static.html", func(buf []byte) {
			defer wg.Done()
			if !bytes.Equal(buf, expected) {
				t.Errorf("unexpected result. Got %v, expected %v", buf, expected)
			}
		})
		if err != nil {
			t.Errorf("error:%s", err.Error())
		}
	}
	wg.Wait()
}

func TestWrite(t *testing.T) {
	err := iouring.Init()
	if err != nil {
		t.Fatal(err)
	}
	defer iouring.Cleanup()

	go func() {
		for err := range iouring.Err() {
			t.Error(err)
		}
	}()

	dir, err := os.MkdirTemp("", "iouring")
	if err != nil {
		t.Fatal(err)
	}
	defer os.RemoveAll(dir)

	var wg sync.WaitGroup

	helper := func(file string) {
		wg.Add(1)
		input, err := os.ReadFile("../testdata/" + file)
		if err != nil {
			t.Error(err)
			return
		}

		err = iouring.WriteFile(filepath.Join(dir, file), input, 0644, func(n int) {
			defer wg.Done()
			if n != len(input) {
				t.Errorf("unexpected result. Got %d, expected %d bytes", n, len(input))
			}
		})
		if err != nil {
			t.Error(err)
		}
		iouring.Poll()
		wg.Wait()
		got, err := os.ReadFile(filepath.Join(dir, file))
		if err != nil {
			t.Error(err)
			return
		}
		if !bytes.Equal(got, input) {
			t.Errorf("unexpected result. Got %v, expected %v", got, input)
		}
	}

	t.Run("ZeroByte", func(t *testing.T) {
		helper("zero_byte.txt")
	})

	t.Run("MediumFile", func(t *testing.T) {
		helper("static.html")
	})
}

var globalBuf []byte

func BenchmarkRead(b *testing.B) {
	b.Run("stdlib", func(b *testing.B) {
		for i := 0; i < b.N; i++ {
			buf, err := os.ReadFile("../testdata/zero_byte.txt")
			if err != nil {
				b.Error(err)
			}
			globalBuf = buf
			buf, err = os.ReadFile("../testdata/static.html")
			if err != nil {
				b.Error(err)
			}
			globalBuf = buf
		}
	})

	b.Run("stdlib", func(b *testing.B) {
		iouring.Init()
		defer iouring.Cleanup()
		go func() {
			for err := range iouring.Err() {
				b.Error(err)
			}
		}()
		for i := 0; i < b.N; i++ {
			err := iouring.ReadFile("../testdata/zero_byte.txt", func(buf []byte) {
				globalBuf = buf
			})
			if err != nil {
				b.Error(err)
			}
			err = iouring.ReadFile("../testdata/static.html", func(buf []byte) {
				globalBuf = buf
			})
			if err != nil {
				b.Error(err)
			}
		}
	})
}
