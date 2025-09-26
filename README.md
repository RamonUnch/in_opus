# in_opus
Winamp 2.x and 5.x input plugin for Opus file


== PURPOSE ==

Opus is a quite new audio file format, very good for generic lossy
compression especially at low bitrates including voice.

I developed this plugin because I wanted to be able to play OPUS files
with Winamp 2, both on my Windows XP machine and an old Windows 98 SE,
That is still used for playing music and for retro gaming at my home.

I had to build myself the libraries because they include dependencies
that are not present on this old OS.

The plugin was tested and works fine on Windows 95/98SE/NT4/2000/XP/2003/7.

There is a very good plugin that was already written by thinktink but
it only works for Winamp 5 and recent windows versions.

I provide this plugin for compatibility with old platforms. to my knowledge,
this is the only Opus player for Windows 9x. and some of you may still
use Winamp 2 for some reasons.


== THIS PROGRAM IS BASED ON ==

libopus 1.5.2, * opusfile 0.12 (http://opus-codec.org/)
libogg 1.3.2 (http://xiph.org/ogg/)
Compiled using gcc 13.2, (MinGW64 / winlibs.com) on Windows Server 2003

== FEATURES ==

* Opens *.opus and *.opu files contained in an ogg stream (this is what opus
  files are supposed to be anyway) the extension has to be .opu or .opus.

* Opens HTTP OPUS Radio streams since v0.610, displays Shoutcast/Icecast
  server info on Alt+3 since v0.888.

* Displays most standard Tags values (Title, Artist, Date... on Alt+3)
  In addition there is a panel at the bottom of the Tag dialog box,
  displaying the content of the "DESCRIPTION" tag, used by youtube-dl
  for the video comment when using the --add-metadata switch.

* Full replaygain support.

* Output at any rate from 1-192kHz, since v0.777,
  and using 8, 16, 24 or 32 bits per samples wince v0.892.

* Runs on Windows 95/98SE/NT4/2000/XP/2003/7/8/10, with Winamp 2.x, 5.x,
  WACUP and XMPlay 2.1+. You need XMPlay3.7+ to enable UNICODE_FILE.
  Finally the plugin was tested with MediaMonkey 3.2.5 and 4.1.29

* UNICODE filename support (v0.666+) under Windows NT4+ as well as UNICODE
  tags support (v0.555+).


== LIMITATIONS ==

I did not want to lose time so there are some limitations,
they may change in the future if I continue the project.

* No way to edit tags.

* UTF-8 is not really supported under Win9x; all tags are converted
  to Windows-1252 for display  Thus you will not have all the Unicode
  characters. Not a big deal if you are from a European country.
  Under NT the UTF-8 strings are converted to UTF-16 since 0.555.

* In the Winamp playlist only the filename/URL will be visible.
  Since v0.911 proper format will be shown in the PL on Winamp 5.x
