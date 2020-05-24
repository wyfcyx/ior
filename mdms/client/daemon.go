package main

import (
    "net"
    "fmt"
    "strconv"
    "net/rpc"
	"os"
	"strings"
)

type Operation struct {
	id int
	//path []byte
	path string
}

func main() {
	local_rank, err := strconv.Atoi(os.Args[1]) 
	if err != nil {
		fmt.Println(err)
		return
	}
	//fmt.Println(os.Args[1])
    l, err := net.Listen("tcp4", "localhost:" + strconv.Itoa(6789 + local_rank))
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

	/*
	resultFile := "result." + os.Args[1]
	if _, err := os.Stat(resultFile); err != nil {
	   os.Create(resultFile)
    }
    file, err := os.OpenFile(resultFile, os.O_RDWR | os.O_APPEND, os.ModeAppend)
	if err != nil {
		fmt.Println(err)
		return
	}
	defer file.Close()
	*/

    client, err := rpc.DialHTTP("tcp", "10.1.0.20:1234")
    if err != nil {
        fmt.Println(err)
        return
    }

    //count := 0
    for {
        recv := make([]byte, 1000)
        _, err := c.Read(recv)    
        if err != nil {
            fmt.Println(err)
            return
        }
        //fmt.Printf("Received message = " + string(recv[:]) + "\n")
		/*
        count = count + 1 
        temp := make([]byte, 100)
        err = client.Call("LevelDB.Query", &count, &temp)
        if err != nil {
            fmt.Println(err)
            return
        }
        //fmt.Println("received from rpc = ", string(temp[:]))
		*/
		e := 0
		for i := 0; i < len(recv); i++ {
			if recv[i] == 0 {
				e = i
				break
			}
		}
		command := string(recv[:e])
		split := strings.Split(command, " ")
		c_type := split[0]
		/*
		path := ""
		if len(split) > 1 {
			path = split[1]
		}
		*/
		var reply int
		//fmt.Println("c_type = ", c_type)
		if c_type == "mkdir" {
			//fmt.Println("ready!!!")
			//op := Operation{id: 0, path: path}
			/*
			var op Operation
			op.id = 0
			op.path = path
			*/
			err = client.Call("LevelDB.Query", split, &reply)
			//fmt.Println("path = ", path)
			/*
			test := 0
			err = client.Call("LevelDB.Test", &test, &reply)
			*/
		}
        if _, err := c.Write([]byte("OK")); err != nil {
            fmt.Println(err)
            return
        }
    } 
}
