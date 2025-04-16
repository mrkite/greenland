/** @copyright 2025 Sean Kasun */
package main

import (
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"slices"
	"strconv"
	"strings"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/widget"
)

type Gestalt byte

const (
	CanSave Gestalt = 1 << iota
	CanExport
)

type ResView interface {
	View() fyne.CanvasObject
	Gestalt() Gestalt
	Save(io.Writer)
	Export(io.Writer)
}

type Resource struct {
	Name    string
	Type    int
	Bif     int
	Tileset int
	Index   int
}

type Chitin struct {
	bifs  []string
	types map[string][]Resource
	cur   ResView
}

func (c *Chitin) Load(data []byte, root string) error {
	file := NewFile(data)
	if file.R4() != "KEY " {
		return errors.New("Not a valid key file")
	}
	if file.R4() != "V1  " {
		return errors.New("Unsupported key version")
	}

	numBifs := file.R32()
	numResources := file.R32()
	bifOfs := file.R32()
	resOfs := file.R32()

	file.Seek(bifOfs)
	for i := range numBifs {
		file.Seek(bifOfs + i*12 + 4) // skip length
		nameOfs := file.R32()
		nameLen := file.R16()
		location := file.R16()
		file.Seek(nameOfs)
		path := ""
		if (location & 0x20) != 0 {
			path = "cd4"
		}
		if (location & 0x10) != 0 {
			path = "cd3"
		}
		if (location & 0x8) != 0 {
			path = "cd2"
		}
		name := strings.Replace(file.Rs(nameLen-1), "\\", "/", -1)
		path = strings.ToLower(filepath.Join(root, path, name))
		// check if cd-based path exists:
		if _, err := os.Stat(path); os.IsNotExist(err) {
			path = strings.ToLower(filepath.Join(root, name))
		}
		c.bifs = append(c.bifs, path)
	}

	c.types = make(map[string][]Resource)
	file.Seek(resOfs)
	for range numResources {
		res := Resource{}
		res.Name = file.Rs(8)
		res.Type = file.R16()
		locator := file.R32()
		res.Bif = locator >> 20
		res.Tileset = (locator >> 14) & 0x3f
		res.Index = locator & 0x3fff
		typ := "unknown"
		for _, cat := range Categories {
			if cat.Type == res.Type {
				typ = cat.Name
				break
			}
		}
		c.types[typ] = append(c.types[typ], res)
	}
	for k := range c.types {
		slices.SortFunc(c.types[k], func(a, b Resource) int {
			return strings.Compare(a.Name, b.Name)
		})
	}

	return nil
}

func (c *Chitin) Save(writer io.Writer) {
	if c.cur != nil {
		c.cur.Save(writer)
	}
}

func (c *Chitin) Export(writer io.Writer) {
	if c.cur != nil {
		c.cur.Export(writer)
	}
}

func (c *Chitin) Main(saveItem *fyne.MenuItem, exportItem *fyne.MenuItem) fyne.CanvasObject {
	content := container.NewStack(widget.NewLabel("Select a resource"))
	sidebar := c.listCategories()
	split := container.NewHSplit(
		sidebar,
		content,
	)
	sidebar.OnSelected = func(uid widget.TreeNodeID) {
		saveItem.Disabled = true
		exportItem.Disabled = true
		if uid[0] == '>' {
			sidebar.ToggleBranch(uid)
		} else {
			c.cur = c.showResource(uid)
			gestalt := c.cur.Gestalt()
			saveItem.Disabled = (gestalt & CanSave) == 0
			exportItem.Disabled = (gestalt & CanExport) == 0
			content.Objects[0] = c.cur.View()
			content.Refresh()
		}
	}
	split.Offset = 0.3
	return split
}

type Category struct {
	Type      int
	Extension string
	Name      string
	View      func(*File) ResView
}

func Invalid(_ *File) ResView {
	return NewErr("Invalid Type")
}

var Categories = []Category{
	{1, "bmp", "Bitmaps", NewBMP},
	{2, "mve", "Movies", NewMVE},
	{4, "wav", "Audio", Invalid},
	{5, "wfx", "Wave FX", Invalid},
	{6, "plt", "Paper Dolls", NewPLT},
	{1000, "bam", "Animations", NewBAM},
	{1001, "wed", "Maps", NewWED},
	{1002, "chu", "Controls", Invalid},
	{1003, "tis", "Tiles", NewTIS},
	{1004, "mos", "GUIs", NewMOS},
	{1005, "itm", "Items", Invalid},
	{1006, "spl", "Spells", Invalid},
	{1007, "bcs", "Compiled Scripts", Invalid},
	{1008, "ids", "IDs", Invalid},
	{1009, "cre", "Creatures", Invalid},
	{1010, "are", "Areas", Invalid},
	{1011, "dlg", "Dialogs", Invalid},
	{1012, "2da", "Rulesets", Invalid},
	{1013, "gam", "Save games", Invalid},
	{1014, "sto", "Stores", Invalid},
	{1015, "wmp", "World maps", Invalid},
	{1016, "eff", "Effects", Invalid},
	{1017, "vvc", "Spell Effects", Invalid},
	{1018, "pro", "Projectiles", Invalid},
}

func (c *Chitin) listCategories() *widget.Tree {
	return widget.NewTree(
		func(tni widget.TreeNodeID) []widget.TreeNodeID {
			ids := []widget.TreeNodeID{}
			if tni == "" {
				for _, cat := range Categories {
					ids = append(ids, fmt.Sprintf(">%s", cat.Name))
				}
			} else {
				cat := tni[1:]
				for i := range c.types[cat] {
					ids = append(ids, fmt.Sprintf("%s|%d", cat, i))
				}
			}
			return ids
		}, func(tni widget.TreeNodeID) bool {
			return tni == "" || tni[0] == '>'
		}, func(b bool) fyne.CanvasObject {
			return widget.NewLabel("template")
		}, func(tni widget.TreeNodeID, b bool, co fyne.CanvasObject) {
			text := tni
			if tni[0] == '>' {
				text = tni[1:]
			} else {
				r := c.findResource(tni)
				if r != nil {
					text = r.Name
				}
			}
			co.(*widget.Label).SetText(text)
		})
}

func (c *Chitin) findResource(t string) *Resource {
	parts := strings.Split(t, "|")
	if len(parts) != 2 {
		return nil
	}
	if idx, err := strconv.Atoi(parts[1]); err == nil {
		return &c.types[parts[0]][idx]
	}
	return nil
}

func (c *Chitin) showResource(t string) ResView {
	r := c.findResource(t)
	if r == nil {
		return NewErr("Failed")
	}

	bif, err := NewBif(c.bifs[r.Bif])
	if err != nil {
		return NewErr(err.Error())
	}
	res, err := bif.Get(r.Tileset, r.Index)
	if err != nil {
		return NewErr(err.Error())
	}
	for _, cat := range Categories {
		if cat.Type == r.Type {
			return cat.View(res)
		}
	}
	return NewErr("Invalid type")
}

type ErrWidget struct {
	label *widget.Label
}

func NewErr(s string) *ErrWidget {
	return &ErrWidget{widget.NewLabel(s)}
}

func (e *ErrWidget) View() fyne.CanvasObject {
	return e.label
}

func (e *ErrWidget) Gestalt() Gestalt {
	return 0
}

func (e *ErrWidget) Save(_ io.Writer) {
}

func (e *ErrWidget) Export(_ io.Writer) {
}
