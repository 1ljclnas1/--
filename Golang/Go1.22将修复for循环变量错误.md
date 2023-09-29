根据Go开发团队技术leader Russ Cox(rsc)的介绍，Go开发者经常会犯的错误是在循环迭代结束后，保留对循环变量的引用，此时他会采用预期之外的新值

```go
func main(){
    done := make(chan bool)

    values := []string{"a", "b", "c"}
    for _, v := range values {
        go func(){
            fmt.Println(v)
            done <- true;
        }（）
    }

    // wait for all goroutines to complete before exiting
    for _ = range values {
        <- done
    }
}
```

其创建的三个goroutine都用于打印相同的变量 v，因此他们只会打出"c","c","c", 而不是按顺序的"a", "b", "c"

从Go1.21开始，开发者可以启用GOEXPRIMENT=loopvar来构建Go程序，已解决上文提到的for循环变量问题

构建命令：

```shell
GOEXPRIMENT=loopvar go install my/program
GOEXPRIMENT=loopvar go build my/program
GOEXPRIMENT=loopvar go test my/program
GOEXPRIMENT=loopvar go test my/program -benth=.
```

**现在，Go的开发团队表示，在1.22开始，新的for循环语义将会在go.mod文件中的Go版本大于等于Go1.22下默认启用**
