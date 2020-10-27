# Aegisub CLI

Standalone binary for running Aegisub automations from the command line.

## Usage

```
aegisub-cli [options] <input file> <output file> <script file> <macro>
Options:
  --help                  produce help message
  --video arg             video to load
  --timecodes arg         timecodes to load
  --keyframes arg         keyframes to load
  --active-line arg (=-1) the active line
  --selected-lines arg    the selected lines
  --dialog arg            response to a dialog, in JSON
  --file arg              filename to supply to an open/save call
```

Examples:
```
aegisub-cli --dialog '{"button": 0, "values": {"stripComments": true}}' --video premux.mkv script.ass script_out.ass l0.ASSWipe.moon ASSWipe

aegisub-cli --selected-lines 0-5,10,15-20 script.ass script_out.ass lyger.GradientByChar.lua "Gradient by characte/Apply Gradient"
```

### Dialogs

You can navigate automations that show dialogs using the `--dialog` option.
`--dialog` takes a JSON object with two keys: `button` and `values`.
`button` is an integer specifying which button in the dialog to push, starting at 0.
`values` is a JSON object consisting of control name/value pairs, for specifying values of text fields, checkboxes, and so on.

Valid values include:

* Float controls: A float such as `0.1`
* Integer controls: An integer such as `42`
* Color controls: An integer array of length 3 or 4, such as `[50, 100, 250, 20]`, in the order R, G, B, A
* Checkbox controls: A boolean value, either `true` or `false`
* Others: A string such as `"hello"`

The control names are the names supplied to `aegisub.dialog`.
Refer to the source code of the automation or the debug output printed by `aegisub-cli` to find the correct names.

## Compiling

### Linux

Install Meson and Ninja (e.g. using pip for Python 3), as well as development libraries for ICU, Boost and FFMS2, and run

```
$ meson --prefix=/usr --buildtype=release builddir
$ ninja -C builddir src/aegisub-cli
```

The prefix should be set to the same prefix as your main Aegisub installation, as this is where Aegisub CLI will search for automation modules.
Note however that you do not need to install the actual binary itself to the same directory.

### Windows

Install Visual Studio 2019, Python 3, Meson and Ninja, open the `x64 Native Tools Command Prompt for VS 2019`, run

```
$ meson -Ddefault_library=static -Dlocal_boost=true --buildtype=release builddir
$ ninja -C builddir src/aegisub-cli.exe
```

and sit tight.
Meson will download and build the required dependencies, including ICU, Boost and FFmpeg, so compilation will take a while.

### Development on Windows

You can generate a Visual Studio 2019 project using
```
$ meson -Ddefault_library=static -Dlocal_boost=true --buildtype=debugoptimized --backend vs2019 builddir
```

You can then import the solution generated in `builddir`.
Note that you cannot use `--buildtype=debug` as FFmpeg will fail to compile without optimizations.

## Windows binaries

There are pre-built binaries for Windows available from the releases section.
Simply place aegisub-cli.exe in the same directory as your main Aegisub executable and add the directory to your PATH.
