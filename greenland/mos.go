/** @copyright 2025 Sean Kasun */
package main

import (
	"bytes"
	"compress/zlib"
	"image"
	"image/png"
	"io"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/canvas"
	"fyne.io/fyne/v2/container"
)

type MOS struct {
	view  fyne.CanvasObject
	image image.Image
	file  *File
}

func NewMOS(res *File) ResView {
	key := res.R4()
	if key == "MOSC" {
		res.Skip(4) // "V1  "
		length := res.R32()
		in := bytes.NewBuffer(res.Read(res.Length() - res.Tell()))
		reader, err := zlib.NewReader(in)
		if err != nil {
			return NewErr(err.Error())
		}
		arr, err := io.ReadAll(reader)
		if err != nil {
			return NewErr(err.Error())
		}
		if len(arr) != length {
			return NewErr("Invalid size")
		}
		res = NewFile(arr)
		key = res.R4()
	}

	if key != "MOS " {
		return NewErr("Not a MOS file")
	}

	res.Skip(4) // "V1  "
	w := res.R16()
	h := res.R16()
	cols := res.R16()
	rows := res.R16()
	res.Skip(4) // bsize
	offset := res.R32()

	img := image.NewRGBA(image.Rect(0, 0, w, h))

	res.Seek(offset)
	palette := res.Read(256 * 4 * cols * rows)
	offsets := make([]int, cols*rows)
	for i := range cols * rows {
		offsets[i] = res.R32()
	}
	tiledata := res.Tell()
	off := 0
	for ty := range rows {
		th := 64
		if ty == rows-1 {
			th = h & 63
		}
		if th == 0 {
			th = 64
		}
		for tx := range cols {
			tw := 64
			if tx == cols-1 {
				tw = w & 63
			}
			if tw == 0 {
				tw = 64
			}
			res.Seek(tiledata + offsets[off])

			for y := range th {
				out := (y+ty*64)*img.Stride + tx*64*4
				for range tw {
					c := res.R8()
					r := palette[off*256*4+c*4+2]
					g := palette[off*256*4+c*4+1]
					b := palette[off*256*4+c*4]
					img.Pix[out] = r
					img.Pix[out+1] = g
					img.Pix[out+2] = b
					if r == 0 && g == 0xff && b == 0 {
						img.Pix[out+3] = 0
					} else {
						img.Pix[out+3] = 0xff
					}
					out += 4
				}
			}
			off++
		}
	}

	i := canvas.NewImageFromImage(img)
	i.FillMode = canvas.ImageFillOriginal
	i.ScaleMode = canvas.ImageScalePixels
	return &MOS{container.NewCenter(i), img, res}
}

func (m *MOS) Gestalt() Gestalt {
	return CanSave | CanExport
}

func (m *MOS) View() fyne.CanvasObject {
	return m.view
}

func (m *MOS) Save(w io.Writer) {
	w.Write(m.file.data)
}

func (m *MOS) Export(w io.Writer) {
	png.Encode(w, m.image)
}
