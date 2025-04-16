/** @copyright 2025 Sean Kasun */
package main

import (
	"bytes"
	"compress/zlib"
	"errors"
	"io"
	"os"
)

type Bif struct {
	data *File
}

func NewBif(filename string) (*Bif, error) {
	data, err := os.ReadFile(filename)
	if err != nil {
		return nil, err
	}

	return &Bif{data: NewFile(data)}, nil
}

const BifMagic = "BIFF"
const BifCompressed = "BIFC"
const BifV1 = "V1  "
const BifV10 = "V1.0"

func (b *Bif) Get(tileset int, index int) (*File, error) {
	b.data.Seek(0)
	key := b.data.R4()
	if key != BifMagic && key != BifCompressed {
		return nil, errors.New("Not a BIF file")
	}
	compressed := key == BifCompressed

	ver := b.data.R4()
	if ver != BifV1 && ver != BifV10 {
		return nil, errors.New("Invalid BIF version")
	}
	if tileset == 0 {
		if compressed {
			h, err := b.getBlock(16, 4)
			if err != nil {
				return nil, err
			}
			fileOfs := h.R32() + index*16
			h, err = b.getBlock(fileOfs, 16)
			if err != nil {
				return nil, err
			}
			h.Skip(4) // locator
			offset := h.R32()
			size := h.R32()
			return b.getBlock(offset, size)
		}
		b.data.Skip(8) // num files and num tiles
		fileOfs := b.data.R32() + index*16
		b.data.Seek(fileOfs)
		b.data.Skip(4) // locator
		offset := b.data.R32()
		size := b.data.R32()
		b.data.Seek(offset)
		return NewFile(b.data.Read(size)), nil
	}
	if compressed {
		h, err := b.getBlock(8, 4)
		if err != nil {
			return nil, err
		}
		numFiles := h.R32()
		h, err = b.getBlock(16, 4)
		if err != nil {
			return nil, err
		}
		fileOfs := h.R32() + numFiles*16 + tileset*20
		h, err = b.getBlock(fileOfs, 20)
		if err != nil {
			return nil, err
		}
		h.Skip(4) // locator
		offset := h.R32()
		numTiles := h.R32()
		size := h.R32()
		return b.getBlock(offset, size*numTiles)
	}
	numFiles := b.data.R32()
	b.data.Skip(4) // num tiles
	fileOfs := b.data.R32() + numFiles*16 + tileset*20
	b.data.Seek(fileOfs)
	b.data.Skip(4) // locator
	offset := b.data.R32()
	numTiles := b.data.R32()
	size := b.data.R32()
	b.data.Seek(offset)
	return NewFile(b.data.Read(size * numTiles)), nil
}

func (b *Bif) getBlock(offset int, length int) (*File, error) {
	b.data.Seek(12)
	cur := 0
	cLen := 0
	uLen := 0
	for cur <= offset && !b.data.EOF() {
		b.data.Skip(cLen)
		if !b.data.EOF() {
			uLen = b.data.R32()
			cLen = b.data.R32()
			cur += uLen
		}
	}

	if b.data.EOF() {
		return nil, errors.New("Block not found")
	}
	delta := offset - (cur - uLen)
	dLen := 0
	arr := make([]byte, length)
	for dLen < length {
		in := bytes.NewBuffer(b.data.Read(cLen))
		reader, err := zlib.NewReader(in)
		if err != nil {
			return nil, err
		}
		unpack, err := io.ReadAll(reader)
		if err != nil {
			return nil, err
		}
		uLen -= delta
		if uLen+dLen > length {
			uLen = length - dLen
		}
		copy(arr[dLen:dLen+uLen], unpack[delta:delta+uLen])
		dLen += uLen
		delta = 0
		if dLen < length {
			uLen = b.data.R32()
			cLen = b.data.R32()
		}
	}
	return NewFile(arr), nil
}
