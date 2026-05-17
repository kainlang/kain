package main

import (
	"io"
	"net"
	"net/http"
	"os"
	"strings"
	"time"
)

const (
	rounds   = 320
	modulus  = int64(1000000007)
	expected = int64(7019)
)

const requestBody = "framework-ping"
const responseBody = "stack-ok-2026"

var sinkHTTPServerFrameworks int64

func sendRequest(port string) (string, error) {
	stream, err := net.Dial("tcp", "127.0.0.1:"+port)
	if err != nil {
		return "", err
	}
	defer stream.Close()
	request := "POST /bench HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: 14\r\nConnection: close\r\n\r\nframework-ping"
	if _, err = io.WriteString(stream, request); err != nil {
		return "", err
	}
	if tcp, ok := stream.(*net.TCPConn); ok {
		_ = tcp.CloseWrite()
	}
	responseBytes, err := io.ReadAll(stream)
	if err != nil {
		return "", err
	}
	responseText := string(responseBytes)
	if position := strings.Index(responseText, "\r\n\r\n"); position >= 0 {
		return responseText[position+4:], nil
	}
	return "", nil
}

func main() {
	mux := http.NewServeMux()
	mux.HandleFunc("/bench", func(writer http.ResponseWriter, request *http.Request) {
		bodyBytes, err := io.ReadAll(request.Body)
		if err != nil || string(bodyBytes) != requestBody {
			writer.WriteHeader(http.StatusBadRequest)
			return
		}
		_, _ = io.WriteString(writer, responseBody)
	})

	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		os.Exit(1)
	}
	server := &http.Server{Handler: mux}
	done := make(chan struct{})
	go func() {
		_ = server.Serve(listener)
		close(done)
	}()
	time.Sleep(25 * time.Millisecond)

	port := listener.Addr().(*net.TCPAddr).Port
	var acc int64
	for index := 0; index < rounds; index++ {
		body, requestErr := sendRequest(strconvItoa(port))
		if requestErr != nil || body != responseBody {
			_ = server.Close()
			<-done
			os.Exit(1)
		}
		acc = (acc + int64(len(requestBody)) + int64(index%17)) % modulus
	}

	_ = server.Close()
	<-done

	sinkHTTPServerFrameworks = acc
	if sinkHTTPServerFrameworks != expected {
		os.Exit(1)
	}
}

func strconvItoa(value int) string {
	if value == 0 {
		return "0"
	}
	var digits [16]byte
	index := len(digits)
	current := value
	for current > 0 {
		index--
		digits[index] = byte('0' + current%10)
		current /= 10
	}
	return string(digits[index:])
}
