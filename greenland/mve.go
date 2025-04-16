/** @copyright 2025 Sean Kasun */
package main

import (
	"fmt"
	"image"
	"io"
	"os"
	"sync"
	"time"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/canvas"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/theme"
	"fyne.io/fyne/v2/widget"
)

type MVE struct {
	widget.BaseWidget
	f           *File
	min         fyne.Size
	src         *image.RGBA
	dest        *canvas.Image
	palette     []byte
	surface     []byte
	back        []byte
	control     []byte
	msec        int
	realX       int
	realY       int
	realW       int
	realH       int
	width       int
	height      int
	frameWidth  int
	frameHeight int
	playing     bool
	stopping    bool
	lock        sync.RWMutex
}

type MVEPlayer struct {
	view fyne.CanvasObject
	mve  *MVE
}

func NewMVE(res *File) ResView {
	player := &MVEPlayer{}
	player.mve = &MVE{}
	player.mve.ExtendBaseWidget(player.mve)
	player.mve.palette = make([]byte, 256*3)
	player.mve.dest = &canvas.Image{}
	player.mve.dest.FillMode = canvas.ImageFillOriginal
	player.mve.f = res
	player.mve.f.Seek(0x1a)

	play := widget.NewButtonWithIcon("Play", theme.MediaPlayIcon(), func() {
		player.mve.Play()
	})

	player.view = container.NewBorder(nil, play, nil, nil, player.mve)
	return player
}

func (p *MVEPlayer) Gestalt() Gestalt {
	return 0
}

func (p *MVEPlayer) View() fyne.CanvasObject {
	return p.view
}

func (p *MVEPlayer) Save(_ io.Writer) {
}

func (p *MVEPlayer) Export(_ io.Writer) {
}

func (m *MVE) Play() {
	if m.isPlaying() {
		return
	}
	m.lock.Lock()
	m.playing = true
	m.lock.Unlock()

	go func() {
		for {
			size := m.f.R32()
			if size&0xffff == 0 || m.isStopping() {
				break
			}
			ofs := m.f.Tell()
			m.handleChunk(size)
			m.f.Seek(ofs + (size & 0xffff))
		}
		m.lock.Lock()
		m.playing = false
		m.stopping = false
		m.lock.Unlock()
	}()
}

func (m *MVE) Stop() {
	if !m.isPlaying() {
		return
	}
	m.lock.Lock()
	m.stopping = true
	m.lock.Unlock()
}

func (m *MVE) handleChunk(size int) {
	offset := 0
	for offset < size {
		packetSize := m.f.R16()
		opcode := m.f.R8()
		param := m.f.R8()
		packetStart := m.f.Tell()
		switch opcode {
		case 0, 1:
			return
		case 2: // timing
			{
				delay := m.f.R32()
				div := 1000 / m.f.R16()
				m.msec = delay / div
			}
		case 3: // init sound
		case 4: // play sound
		case 5: // init video
			m.frameWidth = m.f.R16() * 8
			m.frameHeight = m.f.R16() * 8
			m.surface = make([]byte, m.frameWidth*m.frameHeight)
			m.back = make([]byte, m.frameWidth*m.frameHeight)
			m.src = image.NewRGBA(image.Rect(0, 0, m.frameWidth, m.frameHeight))
		case 7: // show frame
			m.f.Skip(4) // wa,wb
			if param > 0 {
				m.f.Skip(2)
			}
			m.saveFrame()
			time.Sleep(time.Millisecond * time.Duration(m.msec))
		case 8: // sound data
		case 9: // silence
		case 10: // init movie
			m.width = m.f.R16()
			m.height = m.f.R16()
		case 12: // set palette
			{
				first := m.f.R16()
				num := m.f.R16()
				for i := range num * 3 {
					m.palette[first*3+i] = byte(m.f.R8())
				}
			}
		case 15: // decoder control
			m.control = m.f.Read(packetSize)
		case 17: // decode blocks
			{
				m.f.Skip(4) // unknown
				rx := m.f.R16()
				ry := m.f.R16()
				w := m.f.R16()
				h := m.f.R16()
				if m.f.R16()&1 != 0 { // swap
					m.surface, m.back = m.back, m.surface
				}
				m.decodeBlocks(rx, ry, w, h)
			}
		case 19: // unknown
		case 21: // unknown
		default:
			fmt.Fprintf(os.Stderr, "Unknown opcode: %x", opcode)
			return
		}
		m.f.Seek(packetStart + packetSize)
		offset += packetSize
	}
}

func (m *MVE) saveFrame() {
	stride := m.src.Stride

	for y := range m.realH {
		dstOfs := (m.realY+y)*stride + m.realX
		srcOfs := (m.realY+y)*m.frameWidth + m.realX
		for range m.realW {
			c := int(m.surface[srcOfs])
			m.src.Pix[dstOfs] = m.palette[c*3] * 4
			m.src.Pix[dstOfs+1] = m.palette[c*3+1] * 4
			m.src.Pix[dstOfs+2] = m.palette[c*3+2] * 4
			m.src.Pix[dstOfs+3] = 0xff
			srcOfs++
			dstOfs += 4
		}
	}

	m.dest.Image = m.src
	m.dest.Refresh()
}

func (m *MVE) decodeBlocks(rx, ry, w, h int) {
	m.realX = rx * 8
	m.realY = ry * 8
	m.realW = w * 8
	m.realH = h * 8
	destLine := (m.realY * m.frameWidth) + m.realX
	key := byte(0)
	curCtl := 0
	for range h {
		dest := destLine
		for x := range w {
			if x&1 == 0 {
				key = m.control[curCtl]
				curCtl++
			} else {
				key >>= 4
			}
			switch key & 0xf {
			case 0:
				m.sameBlock(dest)
			case 1: // block is unchanged
			case 2:
				m.copyPreBlock(dest)
			case 3:
				m.copyBlock(dest)
			case 4:
				m.copyBackBlock(dest)
			case 5:
				m.shiftBackBlock(dest)
			case 7:
				m.draw2ColBitBlock(dest)
			case 8:
				m.draw8ColBitBlock(dest)
			case 9:
				m.draw4ColBitBlock(dest)
			case 10:
				m.draw16ColBitBlock(dest)
			case 11:
				m.drawFullBlock(dest)
			case 12:
				m.fill2x2CheckedBlock(dest)
			case 13:
				m.fill4x4CheckedBlock(dest)
			case 14:
				m.fillSolidBlock(dest)
			case 15:
				m.fill1x1CheckedBlock(dest)
			}
			dest += 8
		}
		destLine += m.frameWidth * 8
	}
}

func (m *MVE) sameBlock(dest int) {
	for range 8 {
		copy(m.surface[dest:dest+8], m.back[dest:dest+8])
		dest += m.frameWidth
	}
}

func (m *MVE) copyPreBlock(dest int) {
	val := m.f.R8()
	x := 0
	y := 0
	if val >= 56 {
		val -= 56
		y = (val / 29) + 8
		x = (val % 29) - 14
	} else {
		y = val / 7
		x = (val % 7) + 8
	}
	src := dest + y*m.frameWidth + x
	for range 8 {
		copy(m.surface[dest:dest+8], m.surface[src:src+8])
		dest += m.frameWidth
		src += m.frameWidth
	}
}

func (m *MVE) copyBlock(dest int) {
	val := m.f.R8()
	x := 0
	y := 0
	if val >= 56 {
		val -= 56
		y = -((val / 29) + 8)
		x = -((val % 29) - 14)
	} else {
		y = -(val / 7)
		x = -((val % 7) + 8)
	}

	src := dest + y*m.frameWidth + x
	for range 8 {
		copy(m.surface[dest:dest+8], m.surface[src:src+8])
		dest += m.frameWidth
		src += m.frameWidth
	}
}

func (m *MVE) copyBackBlock(dest int) {
	val := m.f.R8()
	x := (val & 15) - 8
	y := (val >> 4) - 8
	src := dest + y*m.frameWidth + x
	for range 8 {
		copy(m.surface[dest:dest+8], m.back[src:src+8])
		dest += m.frameWidth
		src += m.frameWidth
	}
}

func (m *MVE) shiftBackBlock(dest int) {
	x := int(int8(m.f.R8()))
	y := int(int8(m.f.R8()))
	src := dest + y*m.frameWidth + x
	for range 8 {
		copy(m.surface[dest:dest+8], m.back[src:src+8])
		dest += m.frameWidth
		src += m.frameWidth
	}
}

func (m *MVE) draw2ColBitBlock(dest int) {
	lo := byte(m.f.R8())
	hi := byte(m.f.R8())

	if lo <= hi {
		for range 8 {
			val := m.f.R8()
			for x := range 8 {
				if val&1 == 0 {
					m.surface[dest+x] = lo
				} else {
					m.surface[dest+x] = hi
				}
				val >>= 1
			}
			dest += m.frameWidth
		}
	} else {
		val := 0
		for y := range 4 {
			if y&1 == 0 {
				val = m.f.R8()
			}
			for x := range 4 {
				pix := hi
				if val&1 == 0 {
					pix = lo
				}
				m.surface[dest+x*2] = pix
				m.surface[dest+x*2+1] = pix
				m.surface[dest+x*2+m.frameWidth] = pix
				m.surface[dest+x*2+1+m.frameWidth] = pix
				val >>= 1
			}
			dest += m.frameWidth * 2
		}
	}
}

func (m *MVE) draw8ColBitBlock(dest int) {
	source := m.f.Read(16)
	lo := source[0]
	hi := source[1]
	lo2 := source[6]
	hi2 := source[7]

	if lo <= hi {
		for i := range 2 {
			lo = source[i*4]
			hi = source[i*4+1]
			lo2 = source[i*4+8]
			hi2 = source[i*4+9]
			key := byte(0)
			key2 := byte(0)
			for y := range 4 {
				if y&1 == 0 {
					key = source[i*4+y/2+2]
				}
				for x := range 4 {
					if key&1 == 0 {
						m.surface[dest+x] = lo
					} else {
						m.surface[dest+x] = hi
					}
					key >>= 1
				}
				if y&1 == 0 {
					key2 = source[i*4+y/2+10]
				}
				for x := range 4 {
					if key2&1 == 0 {
						m.surface[dest+x+4] = lo2
					} else {
						m.surface[dest+x+4] = hi2
					}
					key2 >>= 1
				}
				dest += m.frameWidth
			}
		}
	} else if lo2 <= hi2 {
		key := byte(0)
		key2 := byte(0)
		for y := range 8 {
			if y&1 == 0 {
				key = source[y/2+2]
			}
			for x := range 4 {
				if key&1 == 0 {
					m.surface[dest+x] = lo
				} else {
					m.surface[dest+x] = hi
				}
				key >>= 1
			}
			if y&1 == 0 {
				key2 = source[y/2+8]
			}
			for x := range 4 {
				if key2&1 == 0 {
					m.surface[dest+x+4] = lo2
				} else {
					m.surface[dest+x+4] = hi2
				}
				key2 >>= 1
			}
			dest += m.frameWidth
		}
		m.f.Skip(-4) // 12
	} else {
		for i := range 2 {
			lo = source[i*6]
			hi = source[i*6+1]
			for y := range 4 {
				key := source[i*6+y/2+2]
				for x := range 8 {
					if key&1 == 0 {
						m.surface[dest+x] = lo
					} else {
						m.surface[dest+x] = hi
					}
					key >>= 1
				}
				dest += m.frameWidth
			}
		}
		m.f.Skip(-4) // 12
	}
}

func (m *MVE) draw4ColBitBlock(dest int) {
	cmd := m.f.Read(4)
	if cmd[0] <= cmd[1] {
		if cmd[2] <= cmd[3] {
			for range 8 {
				val := m.f.R16()
				for x := range 8 {
					m.surface[dest+x] = cmd[val&3]
					val >>= 2
				}
				dest += m.frameWidth
			}
		} else {
			for range 4 {
				val := m.f.R8()
				for x := range 4 {
					pix := cmd[val&3]
					val >>= 2
					m.surface[dest+x*2] = pix
					m.surface[dest+x*2+1] = pix
					m.surface[dest+m.frameWidth+x*2] = pix
					m.surface[dest+m.frameWidth+x*2+1] = pix
				}
				dest += m.frameWidth * 2
			}
		}
	} else if cmd[2] <= cmd[3] {
		for range 8 {
			val := m.f.R8()
			for x := range 4 {
				pix := cmd[val&3]
				val >>= 2
				m.surface[dest+x*2] = pix
				m.surface[dest+x*2+1] = pix
			}
			dest += m.frameWidth
		}
	} else {
		for range 4 {
			val := m.f.R16()
			for x := range 8 {
				pix := cmd[val&3]
				m.surface[dest+x] = pix
				m.surface[dest+x+m.frameWidth] = pix
				val >>= 2
			}
			dest += m.frameWidth * 2
		}
	}
}

func (m *MVE) draw16ColBitBlock(dest int) {
	cmd := make([]byte, 4)
	cmd2 := make([]byte, 4)
	source := m.f.Read(32)
	lo := source[0]
	hi := source[1]
	lo2 := source[12]
	hi2 := source[13]

	if lo <= hi {
		for i := range 2 {
			for y := range 4 {
				cmd[y] = source[i*8+y]
				cmd2[y] = source[i*8+16+y]
			}
			for y := range 4 {
				key := source[i*8+y+4]
				for x := range 4 {
					m.surface[dest+x] = cmd[key&3]
					key >>= 2
				}
				key = source[i*8+y+20]
				for x := range 4 {
					m.surface[dest+x+4] = cmd2[key&3]
					key >>= 2
				}
				dest += m.frameWidth
			}
		}
	} else if lo2 < hi2 {
		for y := range 4 {
			cmd[y] = source[y]
			cmd2[y] = source[y+12]
		}
		for y := range 8 {
			key := source[y+4]
			for x := range 4 {
				m.surface[dest+x] = cmd[key&3]
				key >>= 2
			}
			key = source[y+16]
			for x := range 4 {
				m.surface[dest+x+4] = cmd2[key&3]
				key >>= 2
			}
			dest += m.frameWidth
		}
		m.f.Skip(-8) // 24
	} else {
		for i := range 2 {
			for y := range 4 {
				cmd[y] = source[i*12+y]
			}
			for y := range 8 {
				key := source[i*12+y+4]
				for x := range 4 {
					m.surface[dest+x+(y&1)*4] = cmd[key&3]
					key >>= 2
				}
				if y&1 != 0 {
					dest += m.frameWidth
				}
			}
		}
		m.f.Skip(-8) // 24
	}
}

func (m *MVE) drawFullBlock(dest int) {
	source := m.f.Read(64)
	for y := range 8 {
		copy(m.surface[dest:dest+8], source[y*8:y*8+8])
		dest += m.frameWidth
	}
}

func (m *MVE) fill2x2CheckedBlock(dest int) {
	for range 4 {
		cmd := m.f.Read(4)
		for range 2 {
			for x := range 4 {
				m.surface[dest+x*2] = cmd[x]
				m.surface[dest+x*2+1] = cmd[x]
			}
			dest += m.frameWidth
		}
	}
}

func (m *MVE) fill4x4CheckedBlock(dest int) {
	for range 2 {
		lo := byte(m.f.R8())
		hi := byte(m.f.R8())
		for x := range 4 {
			for i := range 4 {
				m.surface[dest+i*m.frameWidth+x] = lo
				m.surface[dest+i*m.frameWidth+x+4] = hi
			}
		}
		dest += m.frameWidth * 4
	}
}

func (m *MVE) fillSolidBlock(dest int) {
	val := byte(m.f.R8())
	for range 8 {
		for i := range 8 {
			m.surface[dest+i] = val
		}
		dest += m.frameWidth
	}
}

func (m *MVE) fill1x1CheckedBlock(dest int) {
	val := m.f.Read(2)
	for y := range 8 {
		for x := range 8 {
			m.surface[dest+x] = val[(x+y)&1]
		}
		dest += m.frameWidth
	}
}

func (m *MVE) CreateRenderer() fyne.WidgetRenderer {
	return &mveRenderer{mve: m}
}

func (m *MVE) MinSize() fyne.Size {
	return m.min
}

func (m *MVE) SetMinSize(min fyne.Size) {
	m.min = min
}

func (m *MVE) isPlaying() bool {
	m.lock.RLock()
	defer m.lock.RUnlock()
	return m.playing
}

func (m *MVE) isStopping() bool {
	m.lock.RLock()
	defer m.lock.RUnlock()
	return m.stopping
}

type mveRenderer struct {
	mve *MVE
}

func (m *mveRenderer) Destroy() {
	m.mve.Stop()
}

func (m *mveRenderer) Layout(size fyne.Size) {
	m.mve.dest.Resize(size)
}

func (m *mveRenderer) MinSize() fyne.Size {
	return m.mve.MinSize()
}

func (m *mveRenderer) Objects() []fyne.CanvasObject {
	return []fyne.CanvasObject{m.mve.dest}
}

func (m *mveRenderer) Refresh() {
	m.mve.dest.Refresh()
}
