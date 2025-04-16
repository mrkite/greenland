/** @copyright 2025 Sean Kasun */
package main

import (
	"io"
	"os"
	"path/filepath"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/dialog"
	"fyne.io/fyne/v2/driver/desktop"
	"fyne.io/fyne/v2/storage"
	"fyne.io/fyne/v2/widget"
)

type Greenland struct {
	A          fyne.App
	W          fyne.Window
	chitin     Chitin
	saveItem   *fyne.MenuItem
	exportItem *fyne.MenuItem
}

const recentKey = "recent"

func (g *Greenland) makeMenu() *fyne.MainMenu {
	openItem := fyne.NewMenuItem("Open Chitin.key...", func() {
		if err := g.OpenFile(); err != nil {
			dialog.ShowError(err, g.W)
		}
	})
	openItem.Shortcut = &desktop.CustomShortcut{KeyName: fyne.KeyO, Modifier: fyne.KeyModifierShortcutDefault}
	g.W.Canvas().AddShortcut(openItem.Shortcut, func(shortcut fyne.Shortcut) {
		if err := g.OpenFile(); err != nil {
			dialog.ShowError(err, g.W)
		}
	})
	g.saveItem = fyne.NewMenuItem("Save...", func() {
		if err := g.SaveFile(); err != nil {
			dialog.ShowError(err, g.W)
		}
	})
	g.saveItem.Shortcut = &desktop.CustomShortcut{KeyName: fyne.KeyS, Modifier: fyne.KeyModifierShortcutDefault}
	g.W.Canvas().AddShortcut(g.saveItem.Shortcut, func(shortcut fyne.Shortcut) {
		if err := g.SaveFile(); err != nil {
			dialog.ShowError(err, g.W)
		}
	})
	g.exportItem = fyne.NewMenuItem("Export...", func() {
		if err := g.ExportFile(); err != nil {
			dialog.ShowError(err, g.W)
		}
	})
	g.exportItem.Shortcut = &desktop.CustomShortcut{KeyName: fyne.KeyE, Modifier: fyne.KeyModifierShortcutDefault}
	g.W.Canvas().AddShortcut(g.exportItem.Shortcut, func(shortcut fyne.Shortcut) {
		if err := g.ExportFile(); err != nil {
			dialog.ShowError(err, g.W)
		}
	})
	fileItems := []*fyne.MenuItem{openItem, fyne.NewMenuItemSeparator()}
	keys := []fyne.KeyName{fyne.Key1, fyne.Key2, fyne.Key3, fyne.Key4, fyne.Key5, fyne.Key6, fyne.Key7, fyne.Key8, fyne.Key9}
	for i, recent := range g.A.Preferences().StringList(recentKey) {
		item := fyne.NewMenuItem(recent, func() { g.OpenRecent(recent) })
		if i < len(keys) {
			item.Shortcut = &desktop.CustomShortcut{KeyName: keys[i], Modifier: fyne.KeyModifierShortcutDefault}
			g.W.Canvas().AddShortcut(item.Shortcut, func(shortcut fyne.Shortcut) { g.OpenRecent(recent) })
		}
		fileItems = append(fileItems, item)
	}
	fileItems = append(fileItems, fyne.NewMenuItemSeparator(), g.saveItem, g.exportItem)

	file := fyne.NewMenu("File", fileItems...)
	return fyne.NewMainMenu(file)
}

func (g *Greenland) OpenFile() error {
	fd := dialog.NewFileOpen(func(reader fyne.URIReadCloser, err error) {
		if reader == nil {
			return
		}
		if err != nil {
			dialog.ShowError(err, g.W)
			return
		}
		g.open(reader, reader.URI().Path())
	}, g.W)
	curLoc, err := defaultFolder()
	if err != nil {
		return err
	}
	fd.SetLocation(curLoc)
	fd.SetFilter(storage.NewExtensionFileFilter([]string{".key"}))
	fd.Show()
	return nil
}

func (g *Greenland) SaveFile() error {
	fd := dialog.NewFileSave(func(writer fyne.URIWriteCloser, err error) {
		if writer == nil {
			return
		}
		if err != nil {
			dialog.ShowError(err, g.W)
			return
		}
		g.chitin.Save(writer)
	}, g.W)
	curLoc, err := defaultFolder()
	if err != nil {
		return err
	}
	fd.SetLocation(curLoc)
	fd.Show()
	return nil
}

func (g *Greenland) ExportFile() error {
	fd := dialog.NewFileSave(func(writer fyne.URIWriteCloser, err error) {
		if writer == nil {
			return
		}
		if err != nil {
			dialog.ShowError(err, g.W)
			return
		}
		g.chitin.Export(writer)
	}, g.W)
	curLoc, err := defaultFolder()
	if err != nil {
		return err
	}
	fd.SetLocation(curLoc)
	fd.Show()
	return nil
}

func defaultFolder() (fyne.ListableURI, error) {
	ex, err := os.Executable()
	if err != nil {
		return nil, err
	}
	return storage.ListerForURI(storage.NewFileURI(filepath.Dir(ex)))
}

func (g *Greenland) OpenRecent(name string) {
	f, err := os.Open(name)
	if err != nil {
		dialog.ShowError(err, g.W)
		return
	}
	defer f.Close()
	g.open(f, name)
}

func (g *Greenland) open(r io.Reader, name string) {
	data, err := io.ReadAll(r)
	if err != nil {
		dialog.ShowError(err, g.W)
		return
	}
	if err = g.chitin.Load(data, filepath.Dir(name)); err != nil {
		dialog.ShowError(err, g.W)
		return
	}
	g.addToRecent(name)
	g.W.SetContent(g.chitin.Main(g.saveItem, g.exportItem))
}

func (g *Greenland) addToRecent(name string) {
	prevRecents := g.A.Preferences().StringList(recentKey)
	// remove duplicates
	recents := []string{name}
	seen := map[string]bool{name: true}
	count := 0
	for _, item := range prevRecents {
		if !seen[item] && count < 5 {
			recents = append(recents, item)
			count++
		}
		seen[item] = true
	}

	g.A.Preferences().SetStringList(recentKey, recents)
	g.W.SetContent(g.formatRecents())
	g.W.SetMainMenu(g.makeMenu())
}

func (g *Greenland) formatRecents() fyne.CanvasObject {
	recents := g.A.Preferences().StringList(recentKey)
	list := widget.NewList(
		func() int {
			return len(recents)
		},
		func() fyne.CanvasObject {
			return widget.NewLabel("template")
		},
		func(i widget.ListItemID, obj fyne.CanvasObject) {
			obj.(*widget.Label).SetText(recents[i])
		})
	list.OnSelected = func(id widget.ListItemID) {
		g.OpenRecent(recents[id])
	}
	return list
}
