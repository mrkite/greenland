/** @copyright 2025 Sean Kasun */
package main

import (
	"bytes"
	"compress/zlib"
	"errors"
	"fmt"
	"image"
	"io"
	"sync"
	"time"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/canvas"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/theme"
	"fyne.io/fyne/v2/widget"
)

type Cycle struct {
	frames []*image.RGBA
}

type BAM struct {
	widget.BaseWidget
	min       fyne.Size
	cycles    []Cycle
	dest      *canvas.Image
	infoCycle *widget.Label
	infoFrame *widget.Label
	play      *widget.Button
	curCycle  int
	curFrame  int
	playing   bool
	stopping  bool
	lock      sync.RWMutex
}

type BAMPlayer struct {
	view fyne.CanvasObject
	bam  *BAM
}

func NewBAM(res *File) ResView {
	player := &BAMPlayer{}
	player.bam = &BAM{}
	player.bam.ExtendBaseWidget(player.bam)
	player.bam.dest = &canvas.Image{}
	player.bam.dest.FillMode = canvas.ImageFillOriginal

	err := player.bam.load(res)
	if err != nil {
		return NewErr(err.Error())
	}

	player.bam.infoCycle = widget.NewLabel("")
	player.bam.infoFrame = widget.NewLabel("")
	player.bam.play = widget.NewButtonWithIcon("", theme.MediaPlayIcon(), func() {})

	player.bam.play.OnTapped = func() {
		if player.bam.isPlaying() {
			player.bam.Stop()
		} else {
			player.bam.Play()
		}
	}

	prev := widget.NewButtonWithIcon("", theme.MediaSkipPreviousIcon(), func() {
		player.bam.changeCycle(-1)
	})

	next := widget.NewButtonWithIcon("", theme.MediaSkipNextIcon(), func() {
		player.bam.changeCycle(1)
	})

	prevFrame := widget.NewButtonWithIcon("", theme.MediaFastRewindIcon(), func() {
		player.bam.changeFrame(-1)
	})

	nextFrame := widget.NewButtonWithIcon("", theme.MediaFastForwardIcon(), func() {
		player.bam.changeFrame(1)
	})

	controller := container.NewHBox(prev, prevFrame, player.bam.play, nextFrame, next, widget.NewLabel("Cycle:"), player.bam.infoCycle, widget.NewLabel("Frame:"), player.bam.infoFrame)

	player.bam.changeCycle(0)

	player.view = container.NewBorder(nil, controller, nil, nil, container.NewCenter(player.bam))
	return player
}

func (p *BAMPlayer) Gestalt() Gestalt {
	return 0
}

func (p *BAMPlayer) View() fyne.CanvasObject {
	return p.view
}

func (p *BAMPlayer) Save(_ io.Writer) {
}

func (p *BAMPlayer) Export(_ io.Writer) {
}

func (b *BAM) Play() {
	if b.isPlaying() {
		return
	}
	b.lock.Lock()
	b.playing = true
	b.play.Icon = theme.MediaPauseIcon()
	b.play.Refresh()
	b.lock.Unlock()

	go func() {
		for !b.isStopping() {
			b.curFrame++
			if b.curFrame >= len(b.cycles[b.curCycle].frames) {
				b.curFrame = 0
			}
			b.updateFrame()
			time.Sleep(time.Millisecond * time.Duration(80))
		}
		b.lock.Lock()
		b.playing = false
		b.stopping = false
		b.play.Icon = theme.MediaPlayIcon()
		b.play.Refresh()
		b.lock.Unlock()
	}()
}

func (b *BAM) Stop() {
	if !b.isPlaying() {
		return
	}
	b.lock.Lock()
	b.stopping = true
	b.lock.Unlock()
}

func (b *BAM) changeCycle(dir int) {
	b.lock.Lock()
	prev := b.curCycle
	b.curCycle += dir
	if b.curCycle < 0 {
		b.curCycle = len(b.cycles) - 1
	}
	if b.curCycle >= len(b.cycles) {
		b.curCycle = 0
	}
	if prev != b.curCycle {
		b.curFrame = 0
	}
	b.updateCycle()
	b.updateFrame()
	b.lock.Unlock()
}

func (b *BAM) changeFrame(dir int) {
	b.lock.Lock()
	b.curFrame += dir
	if b.curFrame < 0 {
		b.curFrame = len(b.cycles[b.curCycle].frames) - 1
	}
	if b.curFrame >= len(b.cycles[b.curCycle].frames) {
		b.curFrame = 0
	}
	b.updateFrame()
	b.lock.Unlock()
}

func (b *BAM) updateCycle() {
	b.infoCycle.Text = fmt.Sprintf("%d/%d", b.curCycle+1, len(b.cycles))
	b.infoCycle.Refresh()
}

func (b *BAM) updateFrame() {
	b.infoFrame.Text = fmt.Sprintf("%d/%d", b.curFrame+1, len(b.cycles[b.curCycle].frames))
	b.infoFrame.Refresh()
	b.dest.Image = b.cycles[b.curCycle].frames[b.curFrame]
	b.dest.Refresh()
}

func (b *BAM) load(res *File) error {
	key := res.R4()
	if key == "BAMC" {
		res.Skip(4) // "V1  "
		length := res.R32()
		in := bytes.NewBuffer(res.Read(res.Length() - res.Tell()))
		reader, err := zlib.NewReader(in)
		if err != nil {
			return err
		}
		arr, err := io.ReadAll(reader)
		if err != nil {
			return err
		}
		if len(arr) != length {
			return errors.New("Invalid size")
		}
		res = NewFile(arr)
		key = res.R4()
	}
	if key != "BAM " {
		return errors.New("Not a BAM file")
	}

	res.Skip(4) // "V1  "
	numFrames := res.R16()
	numCycles := res.R8()
	clone := res.R8()
	frameOfs := res.R32()
	palOfs := res.R32()
	idxOfs := res.R32()

	res.Seek(palOfs)
	palette := res.Read(256 * 4)
	res.Seek(frameOfs + numFrames*12)

	frameCounts := []int{}
	frameIds := []int{}
	for range numCycles {
		cnt := res.R16()
		if cnt == 0 {
			cnt = 1
		}
		frameCounts = append(frameCounts, cnt)
		frameIds = append(frameIds, res.R16())
	}

	for frame, cnt := range frameCounts {
		cycle := Cycle{}

		bounds := image.Rect(0, 0, 0, 0)
		cx := 0
		cy := 0
		for f := range cnt {
			res.Seek(idxOfs + frameIds[frame]*2 + f*2)
			num := res.R16()
			if num != 0xffff {
				res.Seek(frameOfs + num*12)
				w := res.R16()
				h := res.R16()
				x := int(int16(res.R16()))
				y := int(int16(res.R16()))
				if f == 0 {
					cx = x
					cy = y
					bounds = image.Rect(0, 0, w, h)
				}
				x = cx - x
				y = cy - y
				bnd := image.Rect(x, y, x+w, y+h)
				bounds = bounds.Union(bnd)
			}
		}

		for f := range cnt {
			res.Seek(idxOfs + frameIds[frame]*2 + f*2)
			num := res.R16()
			if num == 0xffff {
				cycle.frames = append(cycle.frames, nil)
			} else {
				res.Seek(frameOfs + num*12)
				w := res.R16()
				h := res.R16()
				x := cx - int(int16(res.R16())) - bounds.Min.X
				y := cy - int(int16(res.R16())) - bounds.Min.Y
				fofs := res.R32()
				img := image.NewRGBA(bounds)
				if (fofs & 0x80000000) != 0 {
					res.Seek(fofs & 0x7fffffff)
					for py := range h {
						out := (py+y)*img.Stride + x*4
						for range w {
							c := res.R8()
							r := palette[c*4+2]
							g := palette[c*4+1]
							b := palette[c*4]
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
				} else {
					res.Seek(fofs)
					for py := 0; py < h; py++ {
						out := (py+y)*img.Stride + x*4
						px := 0
						for px < w && py < h {
							c := res.R8()
							r := palette[c*4+2]
							g := palette[c*4+1]
							b := palette[c*4]

							if c == clone {
								n := res.R8() + 1
								for range n {
									img.Pix[out] = r
									img.Pix[out+1] = g
									img.Pix[out+2] = b
									if r == 0 && g == 0xff && b == 0 {
										img.Pix[out+3] = 0
									} else {
										img.Pix[out+3] = 0xff
									}
									out += 4
									px++
									if px == w {
										px = 0
										py++
										out = (py+y)*img.Stride + x*4
									}
								}
							} else {
								img.Pix[out] = r
								img.Pix[out+1] = g
								img.Pix[out+2] = b
								if r == 0 && g == 0xff && b == 0 {
									img.Pix[out+3] = 0
								} else {
									img.Pix[out+3] = 0xff
								}
								out += 4
								px++
							}
						}
					}
				}
				cycle.frames = append(cycle.frames, img)
			}
		}
		b.min.Width = max(b.min.Width, float32(bounds.Dx()))
		b.min.Height = max(b.min.Height, float32(bounds.Dy()))
		b.cycles = append(b.cycles, cycle)
	}
	return nil
}

func (b *BAM) CreateRenderer() fyne.WidgetRenderer {
	return &bamRenderer{bam: b}
}

func (b *BAM) MinSize() fyne.Size {
	return b.min
}

func (b *BAM) SetMinSize(min fyne.Size) {
	b.min = min
}

func (b *BAM) isPlaying() bool {
	b.lock.RLock()
	defer b.lock.RUnlock()
	return b.playing
}

func (b *BAM) isStopping() bool {
	b.lock.RLock()
	defer b.lock.RUnlock()
	return b.stopping
}

type bamRenderer struct {
	bam *BAM
}

func (b *bamRenderer) Destroy() {
	b.bam.Stop()
}

func (b *bamRenderer) Layout(size fyne.Size) {
	b.bam.dest.Resize(size)
}

func (b *bamRenderer) MinSize() fyne.Size {
	return b.bam.MinSize()
}

func (b *bamRenderer) Objects() []fyne.CanvasObject {
	return []fyne.CanvasObject{b.bam.dest}
}

func (b *bamRenderer) Refresh() {
	b.bam.dest.Refresh()
}
