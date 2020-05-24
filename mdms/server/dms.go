package main

import (
    "github.com/syndtr/goleveldb/leveldb"
    //"strconv"
    "fmt"
    "net"
    "net/http"
    "net/rpc"
    "log"
	//"sync"
	"strings"
)

type Operation struct {
	id int
	//path []byte
	path string
}

type LevelDB struct {
	//m sync.Mutex
	db *leveldb.DB
}

type DirMetedata struct {
	uid uint16
	gid uint16
	ctime uint64
}
var levelDB *LevelDB

func (t *LevelDB) Test(args *int, reply *int) error {
	fmt.Println("in Test!")
	return nil
}

func (t *LevelDB) Query(args []string, reply *int) error {
	//fmt.Println("in Query!", args[0])
	//*reply = 0
	switch args[0] {
		case "mkdir":
			//fmt.Println("in dms, begin mkdir " + args[1])
			split := strings.Split(args[1], "/")
			temp := ""
			count := 0
			for _, v := range split {
				if v != "" {
					temp += v + "/"
					count += 1
				}
			}
			split = strings.Split(temp, "/")
			//fmt.Println("in dms, end mkdir " + temp)
			for i := 4; i < len(split) - 1; i++ {
				chk := ""
				for j := 0; j < i; j++ {
					chk += split[j] + "/"
				}
				//fmt.Println("check ", chk)
				_, err := t.db.Get([]byte(chk), nil)
				if err != nil {
					fmt.Println(err)
				}
			}
			//fmt.Println("in dms, mkdir " + temp)
			/*
			for i := 0; i < 10; i++ {
				t.db.Put([]byte(args[1]), []byte("66666666"), nil)
				t.db.Delete([]byte(args[1]), nil)
			}
			*/
			err := t.db.Put([]byte(temp), []byte("66666666"), nil)
			
			if err != nil {
				fmt.Println(err)
			}
		default:
	}
	return nil
}

func main() {
	db, err := leveldb.OpenFile("db", nil)
	if err != nil {
		fmt.Println(err)
		return
	}

	levelDB = &LevelDB{db: db}
	rpc.Register(levelDB)
	rpc.HandleHTTP()

	l, e := net.Listen("tcp", "10.1.0.20:1234")
	if e != nil {
		log.Fatal("listen error : ", e)
	}
	fmt.Println("ready serve!")
	http.Serve(l, nil)
	/*
	conn, err := l.Accept()
	if err != nil {
		log.Fatal("Accept error : ", err)
	}
	fmt.Println("new conn")
	server.ServeConn(conn)
	*/
}
