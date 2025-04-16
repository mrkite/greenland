GOFILES = $(shell find . -iname '*.go')

all: $(GOFILES)
	mkdir -p bin
	go mod tidy
	go build -o bin ./...

check:
	gocritic check ./...
	go vet ./...
