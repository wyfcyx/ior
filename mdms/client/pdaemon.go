package main

import (
	"net"
	"fmt"
	"strings"
)

func main() {
	l, err := net.Listen("tcp4", "localhost:6789")
	if err != nil {
		fmt.Println(err)
		return
	}
	defer l.Close()
	c, err := l.Accept()
	if err != nil {
		fmt.Println(err)
		return
	}
	defer c.Close()

	for {
		recv := make([]byte, 1024)
		_, err := c.Read(recv)
		if err != nil {
			fmt.Println(err)
			return
		}
		str := string(recv[:])
		if str == "exit" {
			break
		}
		fmt.Println(str)
	}
}
