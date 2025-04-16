/** @copyright 2025 Sean Kasun */
package main

import (
	"image"
	"io"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/canvas"
	"fyne.io/fyne/v2/container"
)

type WED struct {
	view  fyne.CanvasObject
	image image.Image
	file  *File
}

func NewWED(res *File) ResView {
	if res.R4() != "WED " {
		return NewErr("Not a WED")
	}
	if res.R4() != "V1.3" {
		return NewErr("Invalid version")
	}

	res.R32() // numOverlays
	numDoors := res.R32()
	res.Skip(4) // unknown
	walkOffset := res.R32()
	doorOfs := res.R32()
	res.Seek(walkOffset)
	numPolys := res.R32()
	polyOfs := res.R32()
	vertOfs := res.R32()
	//	wallOfs := res.R32()
	//	polyIdxOfs := res.R32()

	w := 0
	h := 0
	for p := range numPolys {
		res.Seek(polyOfs + p*18)
		start := res.R32()
		count := res.R32()
		for i := range count {
			res.Seek(vertOfs + (start+i)*4)
			w = max(w, res.R16())
			h = max(h, res.R16())
		}
	}

	maxx := 0
	maxy := 0
	if w > 1000 {
		maxx = w
		w = 1000
	}
	if h > 1000 {
		maxy = h
		h = 1000
	}

	img := image.NewRGBA(image.Rect(0, 0, w+1, h+1))
	for p := range numPolys {
		res.Seek(polyOfs + p*18)
		start := res.R32()
		count := res.R32()
		px := 0
		py := 0
		sx := 0
		sy := 0
		for i := range count {
			res.Seek(vertOfs + (start+i)*4)
			x := res.R16()
			y := res.R16()
			if maxx != 0 {
				x = x * 1000 / maxx
			}
			if maxy != 0 {
				y = y * 1000 / maxy
			}
			if i != 0 {
				drawLine(img, px, py, x, y, 255, 255, 255)
			} else {
				sx = x
				sy = y
			}
			px = x
			py = y
		}
		drawLine(img, sx, sy, px, py, 255, 255, 255)
	}

	for d := range numDoors {
		res.Seek(doorOfs + d*26 + 16)
		count := res.R16()
		res.Skip(4) // unknown
		polyOfs := res.R32()
		sx := 0
		sy := 0
		px := 0
		py := 0
		for i := range count {
			res.Seek(polyOfs + i*18)
			start := res.R32()
			vcnt := res.R32()
			for j := range vcnt {
				res.Seek(vertOfs + (start+j)*4)
				x := res.R16()
				y := res.R16()
				if maxx != 0 {
					x = x * 1000 / maxx
				}
				if maxy != 0 {
					y = y * 1000 / maxy
				}
				if j != 0 {
					drawLine(img, px, py, x, y, 0, 0, 255)
				} else {
					sx = x
					sy = y
				}
				px = x
				py = y
			}
			drawLine(img, sx, sy, px, py, 0, 0, 255)
		}
	}

	i := canvas.NewImageFromImage(img)
	i.FillMode = canvas.ImageFillOriginal
	i.ScaleMode = canvas.ImageScalePixels
	return &WED{container.NewCenter(i), img, res}
}

func drawLine(img *image.RGBA, x1, y1, x2, y2 int, r, g, b byte) {
	dx := x2 - x1
	if dx < 0 {
		dx = -dx
	}
	dy := y2 - y1
	if dy < 0 {
		dy = -dy
	}
	ptr := y1*img.Stride + x1*4
	xincr := 4
	if x1 > x2 {
		xincr = -xincr
	}
	yincr := img.Stride
	if y1 > y2 {
		yincr = -yincr
	}
	if dx >= dy {
		dpr := dy * 2
		dpru := dpr - (dx * 2)
		p := dpr - dx

		for dx >= 0 {
			img.Pix[ptr] = r
			img.Pix[ptr+1] = g
			img.Pix[ptr+2] = b
			img.Pix[ptr+3] = 0xff
			if p > 0 {
				ptr += xincr
				ptr += yincr
				p += dpru
			} else {
				ptr += xincr
				p += dpr
			}
			dx--
		}
	} else {
		dpr := dx * 2
		dpru := dpr - (dy * 2)
		p := dpr - dy

		for dy >= 0 {
			img.Pix[ptr] = r
			img.Pix[ptr+1] = g
			img.Pix[ptr+2] = b
			img.Pix[ptr+3] = 0xff
			if p > 0 {
				ptr += xincr
				ptr += yincr
				p += dpru
			} else {
				ptr += yincr
				p += dpr
			}
			dy--
		}
	}

}

func (w *WED) Gestalt() Gestalt {
	return 0
}

func (w *WED) View() fyne.CanvasObject {
	return w.view
}

func (w *WED) Save(_ io.Writer) {
}

func (w *WED) Export(_ io.Writer) {
}
