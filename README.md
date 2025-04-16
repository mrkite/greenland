# greenland
Eventually this will be an inspector for infinity engine games (baldur's gate etc)

Right now it's just an installer that will extract the cab files from the original CDs (for non-windows platforms).


To use the installer:
Copy all the files from the CDs into a single folder.
Then run `bin/installer path/to/cd destfolder`

It will extract all the game files from the cd into destfolder.  It will skip
any support files and registry files.


Run `bin/greenland` and open the chitin.key of the installed files.
