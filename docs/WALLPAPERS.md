# Wallpapers

An image of your own behind the launcher's screens. **Settings > Appearance > Wallpaper**
lists the images in the wallpapers folder. The choice is kept in the settings file as
`"launcher": { "wallpaper": "beach.png" }`; with no wallpaper (or one this build cannot
read) the theme's own art shows, as before.

## The folder

The images are read from the wallpapers folder on the console:

```
/data/prosperoeden/wallpapers
```

The app creates the folder when it starts. Put images in it over FTP while the app is
running (the dialog shows the same path under its list), then leave and reopen **Settings >
Appearance > Wallpaper**: the list is read each time the dialog opens. An
image already in use is loaded again when the settings are saved, so a replaced file takes
effect without restarting.

### USB / portable folders

To manage wallpapers on a computer without FTP, put a `wallpapers/` folder in the
**Settings > Game files** folder (beside `keys/`, `firmware/` and `roms/`). If it exists
when the app starts, it is used instead of `/data/prosperoeden/wallpapers`. The dialog
shows the folder actually in use. Files are read in place, not copied to the console.

For another location, set `"wallpapers_folder"` in
`/data/prosperoeden/config/prosperoeden.json` and reopen the app:

```json
{ "game_files": "/data/prosperoeden", "wallpapers_folder": "/mnt/usb0/assets/wallpapers" }
```

This setting is shared by all profiles; each profile still chooses its own image.
An explicit folder takes priority, even if its drive is unavailable (the theme's art
then shows). Remove the setting to return to automatic selection. Relative paths and
paths containing `..` are ignored. Without a portable folder or an explicit setting,
the existing console folder remains in use.

## Formats

PNG and JPEG are what a photo or a screenshot usually is; BMP, GIF (its first picture), TGA
and BC7 DDS are read too. An image over 8192 pixels a side is halved until it fits, and the
whole picture is resized to the screen: an image of another shape keeps its own proportions,
with the theme showing behind what it leaves over, instead of being stretched or cut.

## What shows where

The Home screen and the Library draw one background image, in this order:

1. a game's own cover, where the theme shows one (Home and the selected game in the Library);
2. the wallpaper, when one is chosen;
3. the theme's own artwork.

Panels and cards sample the blurred copy of whichever of them is in use.
