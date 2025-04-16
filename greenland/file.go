/** @copyright 2025 Sean Kasun */

package main

type File struct {
	data []byte
	pos  int
}

func NewFile(data []byte) *File {
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

func (f *File) Rs(length int) string {
	s := []rune{}
	for range length {
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
