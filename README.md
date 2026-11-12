# bg-window-autoclicker

Lists your open windows, lets you pick one, and spams the `Enter` keystroke to it until the window closes (or you press Ctrl+C).

# Quickstart

Clone the repo and `cd` into it:

```sh
git clone https://github.com/DeGatchi/bg-window-autoclicker.git
cd bg-window-autoclicker
```

To run the script, run either:

- `make mac 200`
- `make win11 200`

where `200` is the ms delay between the keystroke `Enter` sent to the selected window (optional — defaults to 200).

**Prerequisites**
- **macOS:** Xcode Command Line Tools (`xcode-select --install`). The first run will ask for Accessibility permission for your terminal (System Settings → Privacy & Security → Accessibility).
- **Windows 11:** `make` and MinGW `gcc`, e.g. via [MSYS2](https://www.msys2.org/) or Git Bash with mingw-w64.

```
./build/autoclicker

Active windows:
  [ 0] Code  (pid 62662)
       title: (no title)
       proc:  /Applications/Visual Studio Code.app/Contents/MacOS/Code
  [ 1] Discord  (pid 58713)
       title: (no title)
       proc:  /Applications/Discord.app/Contents/MacOS/Discord
  [ 2] GitHub Desktop  (pid 68689)
       title: (no title)
       proc:  /Applications/GitHub Desktop.app/Contents/MacOS/GitHub Desktop
  [ 3] Brave Browser  (pid 1041)
       title: (no title)
       proc:  /Applications/Brave Browser.app/Contents/MacOS/Brave Browser
  [ 4] Wow  (pid 58900)
       title: (no title)
       proc:  /Applications/World of Warcraft/_forever_/World of Warcraft.app/Contents/MacOS/World of Warcraft
  [ 5] Battle.net  (pid 58586)
       title: (no title)
       proc:  /Applications/Battle.net.app/Contents/MacOS/Battle.net
  [ 6] Stickies  (pid 53286)
       title: (no title)
       proc:  /System/Applications/Stickies.app/Contents/MacOS/Stickies
  [ 7] BetterDisplay  (pid 15795)
       title: (no title)
       proc:  /Applications/BetterDisplay.app/Contents/MacOS/BetterDisplay
```

Then after selecting a window from the active ones in the list:

```
Select a window number: 4

Spamming Enter to:
  app:   Wow
  title: (no title)
  proc:  /Applications/World of Warcraft/_forever_/World of Warcraft.app/Contents/MacOS/World of Warcraft
  pid:   58900   interval: 200 ms

Press Ctrl+C to stop; stops automatically when the window closes.

Enter sent: 56
```

Tada!