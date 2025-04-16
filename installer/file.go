/** @copyright 2025 Sean Kasun */

package main

import (
	"os"
	"unicode/utf16"
)

type File struct {
	data []byte
	pos  int
}

func NewFile(filename string) (*File, error) {
	data, err := os.ReadFile(filename)
	if err != nil {
		return nil, err
	}
	return &File{data: data, pos: 0}, nil
}

func NewFileFromArray(data []byte) *File {
	return &File{data: data, pos: 0}
}

func (f *File) Length() int {
	return len(f.data)
}

func (f *File) Tell() int {
	return f.pos
}

func (f *File) EOF() bool {
	return f.pos >= len(f.data)
}

func (f *File) Seek(pos int) {
	f.pos = pos
}

func (f *File) Skip(length int) {
	f.pos += length
}

func (f *File) R64() int {
	pos := f.pos
	f.pos += 8
	return int(f.data[pos]) |
		(int(f.data[pos+1]) << 8) |
		(int(f.data[pos+2]) << 16) |
		(int(f.data[pos+3]) << 24) |
		(int(f.data[pos+4]) << 32) |
		(int(f.data[pos+5]) << 40) |
		(int(f.data[pos+6]) << 48) |
		(int(f.data[pos+7]) << 56)
}

func (f *File) R32() int {
	pos := f.pos
	f.pos += 4
	return int(f.data[pos]) |
		(int(f.data[pos+1]) << 8) |
		(int(f.data[pos+2]) << 16) |
		(int(f.data[pos+3]) << 24)
}

func (f *File) R16() int {
	pos := f.pos
	f.pos += 2
	return int(f.data[pos]) |
		(int(f.data[pos+1]) << 8)
}

func (f *File) R16be() int {
	pos := f.pos
	f.pos += 2
	return (int(f.data[pos]) << 8) |
		int(f.data[pos+1])
}

func (f *File) R8() int {
	pos := f.pos
	f.pos++
	return int(f.data[pos])
}

func (f *File) R4() string {
	s := []rune{}
	for range 4 {
		s = append(s, rune(f.data[f.pos]))
		f.pos++
	}
	return string(s)
}

func (f *File) Read(length int) []byte {
	pos := f.pos
	f.pos += length
	return f.data[pos:f.pos]
}

func (f *File) Utf8() string {
	from := f.pos
	for f.pos < len(f.data) && f.data[f.pos] != 0 {
		f.pos++
	}
	bytes := f.data[from:f.pos]
	if f.pos < len(f.data) {
		f.pos++
	}
	return string(bytes)
}

func (f *File) Utf16() string {
	words := []uint16{}
	for !f.EOF() {
		ch := f.R16be()
		if ch != 0 {
			words = append(words, uint16(ch))
		} else {
			break
		}
	}
	return string(utf16.Decode(words))
}
