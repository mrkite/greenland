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

type TIS struct {
	view  fyne.CanvasObject
	image image.Image
	file  *File
}

func NewTIS(res *File) ResView {
	palette := res.Read(256 * 4)
	img := image.NewRGBA(image.Rect(0, 0, 64, 64))
	for y := range 64 {
		src := y * img.Stride
		for range 64 {
			c := res.R8()
			img.Pix[src] = palette[c*4]
			img.Pix[src+1] = palette[c*4+1]
			img.Pix[src+2] = palette[c*4+2]
			img.Pix[src+3] = 0xff
			src += 4
		}
	}

	i := canvas.NewImageFromImage(img)
	i.FillMode = canvas.ImageFillOriginal
	i.ScaleMode = canvas.ImageScalePixels
	return &TIS{container.NewCenter(i), img, res}
}

func (t *TIS) Gestalt() Gestalt {
	return CanSave | CanExport
}

func (t *TIS) View() fyne.CanvasObject {
	return t.view
}

func (t *TIS) Save(w io.Writer) {
	w.Write(t.file.data)
}

func (t *TIS) Export(w io.Writer) {
	png.Encode(w, t.image)
}
