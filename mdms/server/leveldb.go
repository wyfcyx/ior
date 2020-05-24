package main

import (
    "github.com/syndtr/goleveldb/leveldb"
    "strconv"
    "fmt"
    "net"
    "net/http"
    "net/rpc"
    "log"
)

type LevelDB int

var db *leveldb.DB
func (t *LevelDB) Query(args *int, reply *[]byte) error {
    //fmt.Println("Query args = ", strconv.Itoa(*args))
    //*reply = []byte("answer")

    //fmt.Println("in Query, args = ", strconv.Itoa(*args))
    temp := make([]byte, 100)
    temp, err := db.Get([]byte(strconv.Itoa(*args)), nil)
    //fmt.Println("Quest result = ", string(temp[:]));
    *reply = temp 
    if err != nil {
        fmt.Println(err)
        return err
    }
    return nil
}

//var db *leveldb.DB

func main() {
    _db, err := leveldb.OpenFile("db", nil)
    db = _db
    if err != nil {
        fmt.Println(err)
        return
    }
    
    for i := 1; i <= 100000; i++ {
        key := strconv.Itoa(i)
        value := strconv.Itoa(i + 1)
        if err := db.Put([]byte(key), []byte(value), nil); err != nil {
            fmt.Println(err)
            return
        }
    }

    levelDB := new(LevelDB)
    rpc.Register(levelDB)
    rpc.HandleHTTP()
    l, e := net.Listen("tcp", "127.0.0.1:1234")
    if e != nil {
        log.Fatal("listen error : ", e)
    }
    http.Serve(l, nil)
}
