package main

import (
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"
)

func w2u(part string) string {
	return strings.Replace(part, "\\", "/", -1)
}

func pathFix(targetDir, comp, group, dir, name string) (string, bool) {
	clean := strings.ToLower(filepath.Join(w2u(comp), w2u(group), w2u(dir), w2u(name)))
	if strings.Contains(clean, "<targetdir>") && len(name) > 0 {
		return strings.Replace(clean, "<targetdir>", targetDir, 1), true
	}
	return clean, false
}

func copyFile(from, to string) error {
	r, err := os.Open(from)
	if err != nil {
		return err
	}
	defer r.Close()
	w, err := os.Create(to)
	if err != nil {
		return err
	}

	_, err = io.Copy(w, r)
	return err
}

func main() {
	if len(os.Args) != 3 {
		fmt.Fprintf(os.Stderr, "Usage: %s <path/to/data1.hdr> <destdir>\n", os.Args[0])
		return
	}
	cab, err := NewCab(os.Args[1])
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: %s\n", err.Error())
		return
	}

	targetDir := os.Args[2]

	for _, comp := range cab.Components {
		fmt.Printf("Extracting component: %s\n", comp.Name)
		for _, groupName := range comp.Groups {
			if group, ok := cab.Groups[groupName]; ok {
				for i := group.First; i <= group.Last && i < len(cab.Files); i++ {
					file := cab.Files[i]
					if dest, ok := pathFix(targetDir, comp.Dest, group.Dest, file.Directory, file.Name); ok {
						// make all dirs
						os.MkdirAll(filepath.Dir(dest), 0750)
						if cab.Files[i].Offset == 0 {
							src := filepath.Join(os.Args[1], w2u(group.Source), file.Name)
							fmt.Printf("Copying %s\n", file.Name)
							if err := copyFile(src, dest); err != nil {
								fmt.Fprintf(os.Stderr, "Failed: %s\n", err.Error())
								return
							}
						} else {
							fmt.Printf("Extracting %s\n", file.Name)
							if err := cab.Extract(i, dest); err != nil {
								fmt.Fprintf(os.Stderr, "Failed: %s\n", err.Error())
								return
							}
						}
					}
				}
			}
		}
	}
}
