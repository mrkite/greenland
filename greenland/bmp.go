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

type BMP struct {
	view  fyne.CanvasObject
	image image.Image
	file  *File
}

func NewBMP(res *File) ResView {
	key := res.Rs(2)
	if key != "BM" {
		return NewErr("Invalid BMP")
	}
	res.Seek(10)
	ofs := res.R32()
	res.Skip(4) // unknown
	w := res.R32()
	h := res.R32()
	res.Skip(2) // planes
	bpp := res.R16()
	res.Skip(4) // compression

	colors := make([]byte, 4*(1<<bpp))
	if bpp == 4 || bpp == 8 {
		res.Seek(54)
		for i := range 1 << bpp {
			colors[i*4+2] = byte(res.R8()) // b
			colors[i*4+1] = byte(res.R8()) // g
			colors[i*4+0] = byte(res.R8()) // r
			colors[i*4+3] = byte(res.R8()) // a
		}
	}
	img := image.NewRGBA(image.Rect(0, 0, w, h))
	res.Seek(ofs)

	c := 0
	for y := h - 1; y >= 0; y-- {
		out := y * img.Stride
		px := 0
		for x := range w {
			switch bpp {
			case 4:
				if (x & 1) == 0 {
					px++
					c = res.R8()
					img.Pix[out] = colors[(c>>4)*4]
					img.Pix[out+1] = colors[(c>>4)*4+1]
					img.Pix[out+2] = colors[(c>>4)*4+2]
					img.Pix[out+3] = colors[(c>>4)*4+3]
				} else {
					img.Pix[out] = colors[(c&0xf)*4]
					img.Pix[out+1] = colors[(c&0xf)*4+1]
					img.Pix[out+2] = colors[(c&0xf)*4+2]
					img.Pix[out+3] = colors[(c&0xf)*4+3]
				}
				out += 4
			case 8:
				px++
				c = res.R8()
				img.Pix[out] = colors[c*4]
				img.Pix[out+1] = colors[c*4+1]
				img.Pix[out+2] = colors[c*4+2]
				img.Pix[out+3] = colors[c*4+3]
				out += 4
			default:
				px += 3
				img.Pix[out+3] = 0xff
				img.Pix[out+2] = byte(res.R8())
				img.Pix[out+1] = byte(res.R8())
				img.Pix[out] = byte(res.R8())
				out += 4
			}
		}
		for (px & 3) != 0 {
			res.Skip(1) // pad
			px++
		}
	}
	i := canvas.NewImageFromImage(img)
	i.FillMode = canvas.ImageFillOriginal
	i.ScaleMode = canvas.ImageScalePixels
	return &BMP{container.NewCenter(i), img, res}
}

func (b *BMP) Gestalt() Gestalt {
	return CanSave | CanExport
}

func (b *BMP) View() fyne.CanvasObject {
	return b.view
}

func (b *BMP) Save(w io.Writer) {
	w.Write(b.file.data)
}

func (b *BMP) Export(w io.Writer) {
	png.Encode(w, b.image)
}
