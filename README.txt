--- WintabEmulator ---

This is a "quick hack" which uses the Windows 8 pointer API to
simulate a tablet accessible via the Wintab API.
(http://www.wacomeng.com/windows/docs/Wintab_v140.htm)

Specifically this was created to allow PaintToolSai to access
pen pressure information on a Microsoft Surface Pro tablet.
That said it may work (or be further developed) to support
other programs which use the Wintab API.

- Carl Ritson <critson@perlfu.co.uk>


-- Install --

1. (optional) Build the program in Visual Studio 2012.
2. Rename WintabEmulator.dll to wintab32.dll.
 If you built the DLL from source this will be in the Release
 directory, otherwise use the copy in the same directory as
 this README file.
3. Copy wintab32.dll to the same directory as the program executable.
4. (optional) Copy wintab.ini to the directory in step 3.
5. (optional) Edit wintab.ini.
 This file allows configuration of a limit set of options.
 None of which need to be changed.
6. Run your progam as normal.


-- Uninstall --

1. Delete the wintab32.dll from the program directory.
2. (optional) Delete the wintab.ini file if used.


-- Caveats --

When emulation is enabled the system Wintab driver is overridden
and normal tablets cannot be accessed via it.  That said Wacom's
Windows 8 drivers provide pointer information in addition to
Wintab API access.

As the focus was on PaintToolSai which makes very limited use of
the Wintab API, and a Microsoft Surface Pro which only provides
pressure (not tilt or rotation), many advanced features of the 
Wintab interface are not implemented or certainly not implemented 
correctly.  However, I'm sure with a bit of time and appropriate
hardware this could easily be fixed.

