package main

import (
	"bytes"
	"compress/flate"
	"crypto/md5"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
)

type CabFile struct {
	Name       string
	Directory  string
	Flags      int
	Size       int
	Compressed int
	Offset     int
	MD5        []byte
	Previous   int
	Next       int
	Link       int
	Volume     int
}

type CabGroup struct {
	Name   string
	Source string
	Dest   string
	First  int
	Last   int
}

type CabComponent struct {
	Name   string
	Dest   string
	Groups []string
}

type Cabinet struct {
	Version    int
	Components []CabComponent
	Groups     map[string]CabGroup
	Files      []CabFile

	base            string
	curVolume       int
	obfuscated      bool
	handle          *File
	obOffs          int
	curIndex        int
	firstIndex      int
	lastIndex       int
	firstOffset     int
	lastOffset      int
	firstSize       int
	lastSize        int
	firstCompressed int
	lastCompressed  int
	end             int
}

func NewCab(basePath string) (*Cabinet, error) {
	hdrName := filepath.Join(basePath, "data1.hdr")
	f, err := NewFile(hdrName)
	if err != nil {
		return nil, err
	}

	if f.R4() != "ISc(" {
		return nil, errors.New("Not a valid Cabinet header")
	}
	v := f.R32()
	f.Skip(4) // volume info
	base := f.R32()

	cab := &Cabinet{}
	cab.base = basePath
	cab.curVolume = 0
	cab.Version = 0
	cab.Groups = make(map[string]CabGroup)

	switch v >> 24 {
	case 1:
		cab.Version = (v >> 12) & 0xf
	default:
		cab.Version = (v & 0xffff) / 100
	}

	f.Seek(base + 0xc)
	ftOffset := f.R32()
	f.Skip(4) // unknown
	f.Skip(8) // ftsize and duplicate
	numDirs := f.R32()
	f.Skip(8) // unknown
	numFiles := f.R32()
	ftOffset2 := f.R32()
	f.Skip(0xe) // unknown

	groupOffsets := []int{}
	for range 71 {
		groupOffsets = append(groupOffsets, f.R32())
	}
	compOffsets := []int{}
	for range 71 {
		compOffsets = append(compOffsets, f.R32())
	}

	for _, off := range groupOffsets {
		for off != 0 {
			f.Seek(base + off + 4) // skip name
			desc := f.R32()
			off = f.R32() // linked list 0

			f.Seek(base + desc)
			name := f.R32()
			f.Skip(0x12)

			if cab.Version == 0 || cab.Version == 5 {
				f.Skip(0x36)
			}
			group := CabGroup{}
			group.First = f.R32()
			group.Last = f.R32()

			sourcefolder := 0
			destfolder := 0
			if cab.Version != 5 {
				sourcefolder = f.R32()
				f.Skip(0x18) // unused strings
				destfolder = f.R32()
			}

			f.Seek(base + name)

			if cab.Version >= 17 {
				group.Name = f.Utf16()
			} else {
				group.Name = f.Utf8()
			}

			if sourcefolder != 0 {
				f.Seek(base + sourcefolder)
				if cab.Version >= 17 {
					group.Source = f.Utf16()
				} else {
					group.Source = f.Utf8()
				}
			}
			if destfolder != 0 {
				f.Seek(base + destfolder)
				if cab.Version >= 17 {
					group.Dest = f.Utf16()
				} else {
					group.Dest = f.Utf8()
				}
			}
			cab.Groups[group.Name] = group
		}
	}

	for _, off := range compOffsets {
		for off != 0 {
			f.Seek(base + off + 4) // skip name
			desc := f.R32()
			off = f.R32() // linked list

			f.Seek(base + desc)
			name := f.R32()
			destfolder := 0

			if cab.Version == 0 || cab.Version == 5 {
				f.Skip(0x18)
				destfolder = f.R32()
				f.Skip(0x50)
			} else {
				f.Skip(0x6b)
			}
			numGroups := f.R16()
			gOff := f.R32()
			comp := CabComponent{}
			f.Seek(base + name)
			if cab.Version >= 17 {
				comp.Name = f.Utf16()
			} else {
				comp.Name = f.Utf8()
			}

			if destfolder != 0 {
				f.Seek(base + destfolder)
				if cab.Version >= 17 {
					comp.Dest = f.Utf16()
				} else {
					comp.Dest = f.Utf8()
				}
			}

			for i := range numGroups {
				f.Seek(base + gOff + i*4)
				f.Seek(base + f.R32())
				if cab.Version >= 17 {
					comp.Groups = append(comp.Groups, f.Utf16())
				} else {
					comp.Groups = append(comp.Groups, f.Utf8())
				}
			}
			cab.Components = append(cab.Components, comp)
		}
	}

	fileTable := []int{}
	f.Seek(base + ftOffset)
	for range numDirs + numFiles {
		fileTable = append(fileTable, f.R32())
	}

	dirNames := []string{}
	for i, ft := range fileTable {
		if i < numDirs {
			if ft != 0 {
				f.Seek(base + ftOffset + ft)
				if cab.Version >= 17 {
					dirNames = append(dirNames, f.Utf16())
				} else {
					dirNames = append(dirNames, f.Utf8())
				}
			}
		} else {
			file := CabFile{}
			file.Name = ""
			file.Directory = ""
			name := 0
			dir := 0
			if cab.Version == 0 || cab.Version == 5 {
				f.Seek(base + ftOffset + ft)
				name = f.R32()
				dir = f.R32()
				file.Flags = f.R16()
				file.Size = f.R32()
				file.Compressed = f.R32()
				f.Skip(0x14)
				file.Offset = f.R32()
				file.MD5 = f.Read(16)
				file.Link = 0
				file.Volume = 1
			} else {
				f.Seek(base + ftOffset + ftOffset2 + (i-numDirs)*0x57)
				file.Flags = f.R16()
				file.Size = f.R64()
				file.Compressed = f.R64()
				file.Offset = f.R64()
				file.MD5 = f.Read(16)
				f.Skip(0x10)
				name = f.R32()
				dir = f.R16()
				f.Skip(0xc)
				file.Previous = f.R32()
				file.Next = f.R32()
				file.Link = f.R8()
				file.Volume = f.R16()
			}

			if name != 0 {
				f.Seek(base + ftOffset + name)
				if cab.Version >= 17 {
					file.Name = f.Utf16()
				} else {
					file.Name = f.Utf8()
				}
			}

			if dir != 0 {
				file.Directory = dirNames[dir]
			}
			cab.Files = append(cab.Files, file)
		}
	}
	return cab, nil
}

func (cab *Cabinet) Extract(index int, dest string) error {
	if index >= len(cab.Files) {
		return errors.New("Invalid file index")
	}
	file := cab.Files[index]
	if (file.Flags & 8) != 0 {
		return errors.New("Invalid file flags")
	}
	if (file.Link & 1) != 0 {
		return cab.Extract(file.Previous, dest)
	}

	f, err := os.Create(dest)
	if err != nil {
		return err
	}

	cab.seek(file.Volume, index, file.Offset, (file.Flags&2) != 0)
	bytesLeft := file.Size
	if (file.Flags & 4) != 0 { // compressed
		bytesLeft = file.Compressed
	}
	hash := md5.New()
	for bytesLeft > 0 {
		if (file.Flags & 4) != 0 { // compressed
			lenword, err := cab.read(2)
			if err != nil {
				return err
			}
			bytesLeft -= 2
			chunklen := int(lenword[0]) | (int(lenword[1]) << 8)
			raw, err := cab.read(chunklen)
			if err != nil {
				return err
			}
			bytesLeft -= chunklen
			chunk, err := io.ReadAll(flate.NewReader(bytes.NewReader(raw)))
			f.Write(chunk)
			hash.Write(chunk)
		} else {
			raw, err := cab.read(bytesLeft)
			if err != nil {
				return err
			}
			bytesLeft = 0
			f.Write(raw)
			hash.Write(raw)
		}
	}
	if !bytes.Equal(hash.Sum(nil), file.MD5) {
		return fmt.Errorf("Hash failure!")
	}
	return nil
}

func (cab *Cabinet) open() error {
	for {
		path := filepath.Join(cab.base, fmt.Sprintf("data%d.cab", cab.curVolume))
		handle, err := NewFile(path)
		if err != nil {
			return err
		}
		if handle.R4() != "ISc(" {
			return fmt.Errorf("%s is not a valid cab", path)
		}
		v := handle.R32()
		handle.Skip(0xc) // volume info, base, size

		version := 0
		switch v >> 24 {
		case 1:
			version = (v >> 12) & 0xf
		default:
			version = (v & 0xffff) / 100
		}
		handle.Skip(8) // offset
		switch version {
		case 0, 5:
			cab.firstIndex = handle.R32()
			cab.lastIndex = handle.R32()
			cab.firstOffset = handle.R32()
			cab.firstSize = handle.R32()
			cab.firstCompressed = handle.R32()
			cab.lastOffset = handle.R32()
			cab.lastSize = handle.R32()
			cab.lastCompressed = handle.R32()
		default:
			cab.firstIndex = handle.R32()
			cab.lastIndex = handle.R32()
			cab.firstOffset = handle.R64()
			cab.firstSize = handle.R64()
			cab.firstCompressed = handle.R64()
			cab.lastOffset = handle.R64()
			cab.lastSize = handle.R64()
			cab.lastCompressed = handle.R64()
		}
		if version == 5 && cab.curIndex > cab.lastIndex {
			cab.curVolume++
		} else {
			cab.handle = handle
			return nil
		}
	}
}

func (cab *Cabinet) read(length int) ([]byte, error) {
	result := []byte{}
	for length > 0 {
		toread := min(length, cab.end)
		result = append(result, cab.handle.Read(toread)...)
		length -= toread
		cab.end -= toread
		if length > 0 {
			cab.curVolume++
			if err := cab.open(); err != nil {
				return nil, err
			}
			if cab.curIndex == cab.firstIndex {
				cab.handle.Seek(int(cab.firstOffset))
			} else if cab.curIndex == cab.lastIndex {
				cab.handle.Seek(int(cab.lastOffset))
			}
		}
	}
	if cab.obfuscated {
		for i, ch := range result {
			ch ^= 0xd5
			result[i] = ((ch >> 2) | (ch << 6)) - byte(cab.obOffs)
			cab.obOffs = (cab.obOffs + 1) % 0x47
		}
	}
	return result, nil
}

func (cab *Cabinet) seek(volume int, index int, offset int, obfuscated bool) error {
	cab.curIndex = index
	cab.obfuscated = obfuscated
	if cab.curVolume != volume {
		cab.curVolume = volume
		if err := cab.open(); err != nil {
			return err
		}
	}
	if index == cab.lastIndex && cab.lastCompressed != 0 {
		cab.end = cab.lastCompressed
		cab.handle.Seek(cab.lastOffset)
	} else {
		cab.end = cab.handle.Length() - offset
		cab.handle.Seek(offset)
	}
	cab.obOffs = 0
	return nil
}
