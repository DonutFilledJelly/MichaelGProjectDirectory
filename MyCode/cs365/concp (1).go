package main

import (
    "fmt"
    "os"
    "log"
    "io"
    "sync"
)

var wg sync.WaitGroup

func setnames(n []string) ([]string, string){//sets the names of the files in a 
                                             //slice and the destination folder in its own string
    lengthofcom := len(n)
    filenames := n[1:lengthofcom-1]
    destifolder := n[lengthofcom-1]
    return filenames, destifolder
}

func makeCopy(n string, d string, c chan error){//copys a file into a new file
    f, err := os.Open(n)
    if err != nil{//error if cant open og file
        c <- err
    }
    b1:= make([]byte, 1024)//buffered by 1024 bytes
    newfiletime := d+"/"+n//new file path
    f2, errc := os.Create(newfiletime)
    if errc != nil{//error with creation of new file
        c <- errc
    }
    for{//copies over and over til done with whole file
        n1, err2 := f.Read(b1)
        if err2 == io.EOF{//ends if at the end of og file
            break
        }
        if err2 != nil{//error if problem reading file
            c <- err2
        }
        _, err3 := f2.Write(b1[:n1])
        if err3 != nil{//error if problem writing onto new file
            c <- err3
        }

    }
    wg.Done()//tells others it is done

}

func checkforerror(c chan error){//checks to see if an error came in
    for{
        msg, open := <- c//constantly reads chan c
        if !open{//if it closes, then stop the cycle
            break;
        }
        if msg != nil {//if there is an error, stop the program entirely
            log.Fatal(msg)
            os.Exit(3)
        }
    }

}

func main() {
    filenames, destifolder := setnames(os.Args)

    _, err := os.Open(destifolder)
    if err != nil {
        log.Fatal(err)//if no folder, than error
    }
    for _, v := range filenames {
        fileinfo, err2 := os.Stat(v)
        if err2 != nil {
            log.Fatal(err2)//if incorrect files, error
        }
        if fileinfo.IsDir(){
            fmt.Println(v, "needs to be a normal file!")//if a file is a directory, incorrect usage
            os.Exit(3)
        }
    }
    c := make(chan error)
    for i, _ := range filenames { //does concurrency for each copy
        wg.Add(1)
        go makeCopy(filenames[i], destifolder, c)
    }
    go checkforerror(c)//starts the error check
    wg.Wait()//waits for all files to copy
    close(c)//stops the error check, not really needed 
}