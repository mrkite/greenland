/** @copyright 2025 Sean Kasun */
package main

import (
	"image"
	"image/png"
	"io"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/canvas"
	"fyne.io/fyne/v2/container"
)

type PLT struct {
	view  fyne.CanvasObject
	image image.Image
	file  *File
}

func NewPLT(res *File) ResView {
	key := res.R4()
	if key != "PLT " {
		return NewErr("Invalid PLT")
	}
	res.Seek(16)
	w := res.R32()
	h := res.R32()
	img := image.NewRGBA(image.Rect(0, 0, w, h))

	for y := h - 1; y >= 0; y-- {
		out := y * img.Stride
		for range w {
			c := byte(res.R8())
			a := byte(res.R8())
			img.Pix[out] = c
			img.Pix[out+1] = c
			img.Pix[out+2] = c
			img.Pix[out+3] = a
			out += 4
		}
	}
	i := canvas.NewImageFromImage(img)
	i.FillMode = canvas.ImageFillOriginal
	i.ScaleMode = canvas.ImageScalePixels
	return &PLT{container.NewCenter(i), img, res}
}

func (p *PLT) Gestalt() Gestalt {
	return CanSave | CanExport
}

func (p *PLT) View() fyne.CanvasObject {
	return p.view
}

func (p *PLT) Save(w io.Writer) {
	w.Write(p.file.data)
}

func (p *PLT) Export(w io.Writer) {
	png.Encode(w, p.image)
}
