% PMS(1) Practical Music Search
% Kim Tore Jensen <kimtjen@gmail.com>
% August 29, 2015

# NAME

pms − Practical Music Search, a Vim-like MPD client based on ncurses

# SYNOPSIS

pms [*−d*] [*−H hostname*] [*−c configfile*] [*−p port*] [*−P password*]

pms [*−h*|*-v*]

# DESCRIPTION

Practical Music Search aims to be a highly accessible and configurable client
to the Music Player Daemon. It features key mapping, customized colors, a
command-line mode, several modes of play, and an easy but powerful interface.

# OPTIONS

-h
:   Show command-line options and exit

-v
:   Print version and exit

-d
:   Turn on debugging output to standard error. If you use this command-line
    option, you should redirect output to a log file lest you clutter up the
    screen.

-H *hostname*
:   Connect to MPD server hostname

-c *configfile*
:   Use configfile as an additional configuration file

-p *port*
:   Connect to port on MPD server

-P *password*
:   Specify password for the MPD server

# CONFIGURATION

Configuration is done in three parts: options, key bindings, and colors.

Options, bindings and colors can be set at runtime by entering command mode
(default binding *:*) or can be preset in a configuration file.

Configuration files are loaded in a specific order in line (almost) with the
XDG Base Directory Specification
<http://standards.freedesktop.org/basedir-spec/basedir-spec-0.6.html>. Possible
directories are collected from the environment variables *XDG_CONFIG_DIRS* and
*XDG_CONFIG_HOME* (the former defaults in PMS to /usr/local/etc/xdg:/etc/xdg
rather than just /etc/xdg as in the spec, the latter defaults to $HOME/.config
as in the spec).

Each path in order from lowest priority (the last entry in *XDG_CONFIG_DIRS*) to
highest (*XDG_CONFIG_HOME*) is suffixed with `/pms/rc` and if this file exists
it is loaded. Finally, if a configuration file was specified on the commandline
this file is loaded.

## Configuring options

String, integer and enumerated options are set with

    set option=value

Boolean options are set with

    set option

and reset with

    set nooption

They can be toggled with

    set option!

or

    set invoption

Values can be retrieved with

    set option?

*se* is an alias for *set* and *:* can be used in place of *=*.

## Configuring keybindings

Key bindings are set with

    bind key command

and removed with

    unbind key

key can be any single character, in addition to these special characters: *up*,
*down*, *left*, *right*, *pageup*, *pagedown*, *home*, *end*, *space*,
*delete*, *insert*, *backspace*, *return*, *kpenter*, *tab* and *F1* through
*F63*.

When unbinding, you can specify *all* as a parameter to remove all bindings.

*map* is an alias for *bind*, while *unmap* and *unm* are aliases for *unbind*.

## Configuring colors

Colors are defined with

    color item foreground [background]

*colour* is an alias for *color*.

# OPTIONS

## Configuration options

host=*string*
:   The hostname to connect to. Default: localhost

port=*integer*
:   The port on the MPD server to connect to. Default: 6600

password=*string*
:   The password to give to the MPD server. Default: none

libraryroot=*string*
:   Optional path to the library’s root. See !string below. If used, it should have a trailing slash. Default: empty string (nothing is prepended)

xtermtitle=*string*
:   If set, the XTerm window title will be set to the specified string. Default: PMS: %playstate%%ifcursong% %artist% – %title%%endif%

reconnectdelay=*integer*
:   If a connection is lost, this is how many seconds between each connection retry. Default: 30

mouse (*boolean*)
:   If set, the mouse is active. Mouse support is incomplete and the slightly hacky mousewheel support available with standard versions of ncurses is less than ideal, but the actions below have been implemented.  Default: unset

msg_buffer_size=*integer*
:   How many log lines to keep in the console. A value of 0 keeps all lines.  Default: 1024

mpd_timeout=*integer*
:   Sets MPD connection timeout in seconds. Leave this lower than your MPD setting.  Default: 30

debug (*boolean*)
:   Turn debugging mode on or off. Default: set

stopdelay=*integer*
:   MPD does not support manual progression within the playlist. A hack is neccessary to stop when the song is done. If manual song progression is not working correctly, increase this value. Default: 1

nextinterval=*integer*
:   This setting controls how many seconds from the current song ends until PMS automatically adds the next song to MPD’s playlist. Default: 5

crossfade=*integer*
:   Set crossfade time in seconds. 0 turns crossfade off completely. Default: MPD’s settings

onplaylistfinish=*string*
:   Specify a shell command to run when the playlist finishes and playback stops.  Default: none

addtoreturns (*boolean*)
:   If set, the add-to command will return focus to the original window. Else, the destination will be focused. Default: unset

followplayback (*boolean*)
:   If set, the cursor will go to the currently playing song and playlist when it changes. Default: unset

followwindow (*boolean*)
:   If set, playback will continue in the active window. Default: unset

followcursor (*boolean*)
:   If set, playback will follow cursor position. Default: unset

nextafteraction (*boolean*)
:   Move cursor to next item after the song is selected, unselected or added to a playlist. Default: set

startuplist=*"playlist|library|..."*
:   The list which is activated and shown at program startup. This can be playlist, library or an arbitrary name of an existing playlist. Default: playlist

regexsearch (*boolean*)
:   Use regular expressions for search terms. Default: set

ignorecase (*boolean*)
:   Ignore case when sorting and searching. The alias ic can also be used. Default: set

scroll=*SCROLL MODE*
:   Set scroll mode; see *SCROLL MODES* below. Default: normal

scrolloff=*integer*
:   When scroll is set to normal, try to keep this many songs above and below the cursor at all times. The alias so can also be used. Default: 0

resetstatus=*integer*
:   Set how many seconds before resetting the statusbar text. Default: 3

sort=tag [tag [...]]
:   Tags by which to sort the library. You can specify multiple tags, separated by whitespace; see *TAGS*. Default: track disc album albumartistsort

columns=column [column [...]]
:   Columns to show in the list; see *TAGS*. Default: artist track title album length

status_unknown=*string*
:   Topbar status string when status is unknown. Default: ??

status_play=*string*
:   Topbar status string when playing. Default: |>

status_pause=*string*
:   Topbar status string when paused. Default: ||

status_stop=*string*
:   Topbar status string when stopped. Default: []

topbarspace (*boolean*)
:   Whether or not to leave an extra space at the end of fixed-width columns. Default: set

topbarvisible (*boolean*)
:   If set, the topbar is visible. Default: set

topbarborders (*boolean*)
:   Whether or not to draw borders on the topbar window. Default: unset

columnspace (*boolean*)
:   Whether or not to leave a blank row between the topbar and the playlist windows. Default: set

columnborders (*boolean*)
:   If set, draw borders between columns. Default: unset

topbarclear (*boolean*)
:   Start out with an empty topbar. Default: unset

topbar[1-99].(left|center|right)=*string*
:   Modify what is displayed in the topbar. The spelling *centre* is also accepted.  See *Configuring the topbar* for format syntax, available tags and defaults.

## Configuring the topbar

Available variables to put in the topbar:

    bitrate, bits, channels, librarysize, listsize, livequeuesize, manual,
    manualshort, mute, muteshort, playstate, progressbar, progresspercentage,
    queuesize, random, randomshort, repeat, repeatshort, samplerate,
    time_elapsed, time_remaining, volume

All *TAGS* can also be used.

Conditionals (if-else) are supported. Currently available conditionals:

    ifcursong, ifplaying, ifpaused, ifstopped

See the defaults below for how conditionals are used.

The topbar syntax allows free customization of the top area of the display. A good way to understand the syntax is to take a look at the defaults:

    set topbar1.left=%time_elapsed% %playstate% %time%%ifcursong% (%progresspercentage%%%)%endif%
    set topbar1.center=%ifcursong%%artist%%endif%
    set topbar1.right=Vol: %volume%%% Mode: %muteshort%%repeatshort%%randomshort%%manualshort%
    set topbar2.center=%ifcursong%==> %title% <==%else%No current song%endif%
    set topbar3.left=%listsize%
    set topbar3.center=%ifcursong%%album% (%year%)%endif%
    set topbar3.right=Q: %livequeuesize%

# COMMANDS

## Playback

play
:   Play the song under the cursor

add
:   Add the selected song(s) to the playlist. If a playlist is selected in windowlist mode, that list is appended to the playlist.

add-to
:   Add the selected song(s) to a chosen playlist

next
:   Play the next song from the playlist or library based on current play mode

really-next
:   Play the next song from playlist or library, regardless of play mode.

prev
:   Play previous song

pause
:   Pause playback or play if playback was paused; do nothing if playback is stopped

stop
:   Stop playback

toggle-play
:   Toggle playback. Play from the current song if playback was stopped or paused, otherwise pause.

volume [string]
:   Set volume. *string* can be delta (+/-value, for instance +4) or absolute value (0~100). If run without any parameters, return the current volume.

mute
:   Toggle mute

crossfade [integer]
:   Set crossfade time in seconds. If no integer is given, or integer is 0, toggle crossfade. If set to a negative value, turn crossfade off.

seek integer
:   Seek integer seconds (can be negative) in the playing song.

repeat
:   Rotate repeat mode through none, repeat one and repeat list

## Adding and playing

play-album
:   Play all songs from the album of the song under the cursor

play-artist
:   Play all songs from the selected artist

play-random [n]
:   Play one or n random songs from the library or visible list.

add-album
:   Add all songs from the selected album to playlist

add-all
:   Add all songs from the currently visible list to playlist. If part of the album already is at the end of the playlist, the remainder is added.

add-artist
:   Add all songs from the selected artist to the playlist

add-random [n]
:   Add one or n random songs from the visible list to the playlist.

remove
:   Remove selected song from playlist

## Playlist management

create string
:   Create a new empty playlist with given name

save string
:   Saves the current list view into a new playlist file with given filename

delete-list [string]
:   Permanently delete the named playlist if given or else the current playlist

activate-list
:   Activate currently viewed list for playback

crop
:   Crop the current playlist to the currently playing song

cropsel
:   Crop the current playlist to the selected songs, or song under cursor

clear
:   Clear the playlist

shuffle
:   Shuffle the playlist

move integer
:   Move the selected songs by the given offset. A positive offset moves songs down; a negative offset moves songs up.

update [string]
:   Ask MPD to update the music library. string can be a file in the music library, or one of this, thisdir, current or currentdir.

select [string]
:   Select songs matching a search term.  If no parameter is given, the song under the cursor is affected.  

unselect [string]
:   Unselect songs matching a search term.  If no parameter is given, the song under the cursor is affected.

toggle-select [string]
:   Toggle selection of songs matching a search term.  If no parameter is given, the song under the cursor is affected.

clear-selection
:   Unselect all songs in the playlist

## Application

info
:   Show info in the status bar about the current song

help
:   Show current key bindings

command-mode
:   Enter command mode, where you can enter configuration options or perform other commands (including those which are not mapped to any key)

change-window *playlist|library|windowlist*
:   Change the active window to playlist, library or windowlist

next-window
:   Move to the next window

prev-window
:   Move to the previous window

last-window
:   Switch to the previously viewed window

redraw
:   Force screen redraw

rehash
:   Reload the configuration file

version
:   Show version information

clear-topbar [integer]
:   Clear out all contents of the topbar or, if a parameter is given, only that line

!string
:   Run a shell command

    Some vim-like placeholders are available:

        %   The current song’s file path, not escaped in any way

        #   The currently highlighted song’s file path, not escaped in any way

        ##  The file path of each of the songs in the current selection or, if there is no selection, each song in the currently visible list. Each path is enclosed in double quotes.

    Examples:

        !echo "%" | xclip

    Copy the current song’s file path to the X clipboard

        !rox-filer "$(dirname "#")"

    Browse the directory containing the currently highlighted song with Rox-filer

        !transcribe ##

    Open the selected songs (or, with no selection, all songs on the playlist) in Transcribe

        !cp ## /media/removabledrive

    Copy the selected songs (or, with no selection, all songs on the playlist) to a USB stick or portable media player

    All paths are prefixed with the string in the config variable libraryroot.

quit, q
:   Exit PMS

## Movement and search

move-up
:   Move the cursor up. In command or quick-find mode move to the previous item in command or search history.

move-down
:   Move the cursor down. In command or quick-find mode move to the next item in command or search history.

move-halfpgup
:   Move the cursor one half screen up

move-halfpgdn
:   Move the cursor one half screen down

move-pgup
:   Move the cursor one screen up

move-pgdn
:   Move the cursor one screen down

move-home
:   Move the cursor to the start of the list

move-end
:   Move the cursor to the end of the list

scroll-up
:   Scroll the list up one line (only acts differently from move-up if scroll is set to normal)

scroll-down
:   Scroll the list down one line (only acts differently from move-up if scroll is set to normal)

center-cursor
:   Scroll the list such that the cursor is centered (only has an effect when scroll is set to normal)

filter
:   Enter filter mode: type to filter the current view for songs. Songs that don’t match are removed from the view. Use the clear-filters command to return to the original view.  

clear-filters
:   Clear all filters from the current playlist.

quick-find
:   Enter quick-find mode: type to jump to next matched song

next-result
:   Find the next search result from the last quick-find

prev-result
:   Find the previous search result from the last quick-find

next-of string
:   Parameter should be a field name (see *TAGS*) – jump to the next track in the list for which the field differs

prev-of string
:   Parameter should be a field name (see *TAGS*) – jump up the list to the first (topmost) of a set of tracks which have in common the next differing value of the given field.  To put that another way, the cursor moves up until the given field changes, then keeps going until just before it would change again.

goto-current
:   Jumps to the current playing song, if any

goto-random
:   Jump to a random song in the playlist

# TAGS

Tags are used for sorting, columns, topbar, and several other things.

The following tags can be used everywhere *TAGS* are supported:

    album, albumartist, albumartistsort, artist, artistsort, comment, composer,
    date, disc, file, genre, name, num, performer, time, title, track,
    trackshort, year

# COLORS

The following colors can be configured:

    background, borders, current, cursor, error, fields.*, foreground, headers,
    lastlist, playinglist, position, selection, statusbar, title,
    topbar.fields.*, topbar.foreground, topbar.librarysize, topbar.listsize,
    topbar.livequeuesize, topbar.mute, topbar.muteshort, topbar.playstate,
    topbar.progressbar, topbar.progresspercentage, topbar.queuesize,
    topbar.random, topbar.randomshort, topbar.repeat, topbar.repeatshort,
    topbar.time_elapsed, topbar.time_remaining

Replace the wildcard * with any of the *TAGS* described above.

The following colors can be used only as foreground colors:

    gray, brightred, brightgreen, yellow, brightblue, brightmagenta, brightcyan

The following colors can be used either for background or foreground colors:

    black, red, green, brown, blue, magenta, cyan, brightgray

The special color *trans* can only be used as a background color, and provides a transparent background.

The alternative spelling *grey* can be used in the place of *gray*, and *light* can be used in place of *bright*.

# MOUSE ACTIONS

topbar
:   Click to toggle play/pause, doubleclick to stop, mousewheel down to skip to the next track, mousewheel up to skip to the previous track, control-mousewheel to turn volume up or down

header
:   Click or scroll mousewheel down on the window title to switch to the next window. Doubleclick or scroll mousewheel up to switch to the previous window.

playlist
:   Click to place the cursor, control-click or click right button to place cursor and toggle selection, doubleclick to place cursor and play, tripleclick to place cursor and add to playlist (if there is a selection, the selection will be added and the song clicked will just be selected), scroll mousewheel to scroll the list up and down.

statusbar
:   Click to enter command mode

# SCROLL MODES

normal
:   The list only scrolls when the cursor is about to go off the top or bottom of the window. See the *scrolloff* option.

centered
:   The cursor is always in the middle of the window except when it is near the top or bottom of the list. The spelling centred is also accepted.

relative
:   The position of the cursor in the confines of the window is proportional to the position of the visible songs relative to the whole song list. Try it out to get a better idea of how it works.

# FILES

* /etc/xdg/pms/rc
* /usr/local/xdg/pms/rc
* ~/.config/pms/rc

Default paths to configuration files, loaded in this order (see the configuration section above).

# ENVIRONMENT

HOME
:   Used to generate the default path to the configuration file if *XDG_CONFIG_HOME* is not set or empty

XDG_CONFIG_HOME
:   The prefix for the user-specific configuration file

XDG_CONFIG_DIRS
:   Prefixes for system-wide configuration files

MPD_HOST
:   Specifies the host which MPD runs on

MPD_PORT
:   Specifies the port on which MPD listens

MPD_PASSWORD
:   Specifies a password to send to MPD on connection

# AUTHORS

Copyright (c) 2006-2015 Kim Tore Jensen <kimtjen@gmail.com>.

Written by Kim Tore Jensen <kimtjen@gmail.com> and Bart Nagel <bart@tremby.net>.

The newest version can be obtained at <https://ambientsound.github.io/pms/>.

# SEE ALSO

mpd(1)
