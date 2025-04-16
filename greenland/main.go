/** @copyright 2025 Sean Kasun */
package main

import (
	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
)

func main() {
	a := app.NewWithID("com.seancode.greenland")
	w := a.NewWindow("Greenland")

	greenland := Greenland{A: a, W: w}
	greenland.W.SetMainMenu(greenland.makeMenu())
	greenland.W.SetMaster() // kill app if window is closed
	greenland.W.SetContent(greenland.formatRecents())
	greenland.W.Resize(fyne.NewSize(800, 600))
	greenland.W.ShowAndRun()
}
