<!--
SPDX-FileCopyrightText: 2024 Roman Gilg <subdiff@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
-->

# Contributing to the Compositor Modules

- [Contributing to the Compositor Modules](#contributing-to-the-compositor-modules)
  - [Logging and Debugging](#logging-and-debugging)
    - [General information about the running instance](#general-information-about-the-running-instance)
    - [Debug console](#debug-console)
    - [Runtime logging](#runtime-logging)
      - [Preparations](#preparations)
      - [DRM logging](#drm-logging)
  - [Developing](#developing)
    - [Compiling](#compiling)
      - [Using FDBuild](#using-fdbuild)
    - [Running Tests](#running-tests)
      - [Local Build](#local-build)
    - [Learning Material](#learning-material)
  - [Submission Guideline](#submission-guideline)
  - [Commit Message Guideline](#commit-message-guideline)
    - [Example](#example)
    - [Tooling](#tooling)

## Logging and Debugging
The first step in contributing to the project by either providing meaningful feedback or by directly
sending in patches is always the analysis of runtime.
For any compositor build with COMO that means
querying general information about its internal state and studying
its debug log while running and afterwards.

### General information about the running instance
Some general information about the running compositor instance can be queried via D-Bus by the following
command (the qdbus tool must be installed):

    qdbus org.kde.KWin /KWin supportInformation

### Debug console
Compositors based on COMO usually come with an integrated debug console. You can launch it with:

    qdbus org.kde.KWin /KWin org.kde.KWin.showDebugConsole

Note that the debug console provides much more information when run for a Wayland compositor.

### Runtime logging
#### Preparations
To show more debug information in the log as a first step the following lines should be added to the
file
`$HOME/.config/QtProject/qtlogging.ini`
(create the file if it does not exist already):

    [Rules]
    kwin_core*=true
    kwin_platform*=true
    kwineffects*=true
    kwin_wayland*=true
    kwin_decorations*=true
    org.kde.kwindowsystem*=true
    kwin_tabbox*=true
    kwin_qpa*=true
    kwin_wl*=true
    kwin_xwl*=true
    kwin_perf*=true
    wrapland*=true
    kwin_libinput.info=true
    kwin_libinput.warning=true
    kwin_libinput.critical=true
    kwin_libinput.debug=false

The above list specifies `kwin_libinput.debug=false` because otherwise the log gets spammed with
lines whenever a mouse button is pressed.
In the same way other logging categories above can be switched on and off by changing the respective
boolean value in this file. The change will become active after a restart of the compositor.

#### DRM logging
In a Wayland session we talk through wlroots directly to the
[Direct Rendering Manager (DRM)](https://en.wikipedia.org/wiki/Direct_Rendering_Manager)
subsystem of the Linux kernel
for showing graphical buffers and configuring outputs.

Debugging issues with it directly can be difficult.
A first step is to priunt out the DRM logs to dmesg what usually isn't done by default.
How to enable such DRM logging is described in the
[wlroots wiki](https://gitlab.freedesktop.org/wlroots/wlroots/-/wikis/DRM-Debugging).

You can also use the following script
to have a convenient way of enabling it temporarily
from the command line:

    #!/usr/bin/env bash

    # Enable verbose DRM logging
    echo 0xFE | sudo tee /sys/module/drm/parameters/debug > /dev/null
    # Clear kernel logs
    sudo dmesg -C
    # Continuously write DRM logs to a file, in the background
    sudo dmesg -w > $HOME/dmesg.log &

    echo "DRM logging activated. Waiting for Ctrl+C..."
    ( trap exit SIGINT ; read -r -d '' _ </dev/tty )

    # Disable DRM logging
    echo 0x00 | sudo tee /sys/module/drm/parameters/debug > /dev/null
    echo
    echo "Ctrl+C received. Disabled DRM logging and exit."

Note that the DRM log output is very verbose.
So only enable it shortly before triggering the faulty behavior
and disable it directly afterwards again.
You then find the dmesg log in `$HOME/dmesg.log`.

## Developing

### Compiling
To start writing code for the Compositor modules first the project needs to be compiled.
You usually want to compile it from its
[master branch](https://github.com/winft/como/commits/master/)
as it reflects the most recent state of development.

#### Using FDBuild
Since some of COMO's dependencies are moving targets in KDE
that do not offer backwards compatibility guarantees,
it is often required to build these KDE dependencies also from their master branches
and rebuild them regularly from the most recent state of the master branch.
The most convenient way for that is to use the
[FDBuild](https://gitlab.com/kwinft/fdbuild)
tool.
It comes with a template mechanism
that creates a subdirectory structure with all required KWinFT and KDE projects to build.
For that issue the command:
```
fdbuild --init-with-template kwinft-plasma-meta
```
After the project templating has finished,
go into the toplevel directory of the just created subdirectory structure.
FDBuild uses fdbuild.yaml files in directories it is supposed to work on
to remember settings about the projects inside these directories.

Important is the setting specifying the installation location of the projects.
This is set in the fdbuild.yaml file inside the toplevel directory.
Adjust the setting to your liking. Recommended is setting it to a subdirectory inside `/opt`,
for example `/opt/como`.

Then simply run FDBuild without any arguments from the toplevel directory
and FDBuild will try to compile and install all projects one after the other.

Note that this will likely fail for several projects on the first run
since you require additional dependencies.
Check the FDBuild log output to find out what dependencies are missing.
A complete list of required dependencies with drifting correctness is also listed
[in the KDE Community Wiki](https://community.kde.org/Guidelines_and_HOWTOs/Build_from_source/Install_the_dependencies).

Once you have installed additional dependencies and want to continue building the projects
from where it failed command:
```
fdbuild --resume-from <project-that-failed>
```

### Running Tests
The Compositor Modules come with over 100 integration tests
which check the expected behavior of different parts of the application.

#### Local Build
To run all relevant tests go to the build directory and issue:
```
dbus-run-session ctest -E 'lockscreen|modifier only shortcut'
```

This command is composited from two commands. Let's quickly explain the different parts:
* `dbus-run-session`: starts a new DBus session for the tests, so your current session is unimpaired.
* `ctest`: the CMake testing utility running binaries, that have been marked as tests in the CMake files.
* `-E 'lockscreen|modifier only shortcut'`: exclude two tests that are currently also not run on the CI.

You can also run a single test.
All tests are separate binaries in the `bin` directory inside the build directory.
That means in order to test e.g. "maximize animation" run from the build directory:
```
dbus-run-session bin/tests "maximize animation"
```

### Learning Material
The Compositor Modules source code is vast and complex.
Understanding it requires time and practice.
For the beginning there are still few available resources to get an overview:
* [Xplain](https://magcius.github.io/xplain/article/), introduction and explanations for X11.
* [How X Window Managers Work](https://jichu4n.com/posts/how-x-window-managers-work-and-how-to-write-one-part-i/),
  series on how to write an X window manager.
* [The Wayland Book](https://wayland-book.com/), explains fundamental concepts of Wayland.
* [KWin now and tomorrow at XDC 2019](https://www.youtube.com/watch?v=vj70xmG_5Bs),
  gives an overview about the internal structure of the Compositor Modules.

## Submission Guideline
Code contributions to the Compositor Modules are very welcome but follow a strict process that is laid out in
detail in Wrapland's [contributing document][wrapland-submissions].

*Summarizing the main points:*

* Use [pull requests][pull-requests] directly for smaller contributions, but create
  [issue tickets][issue] *beforehand* for [larger changes][wrapland-large-changes].
* Adhere to the [KDE Frameworks Coding Style][frameworks-style].
* Merge requests have to be posted against master or a feature branch. Commits to the stable branch
  are only cherry-picked from the master branch after some testing on the master branch.

Also make sure to increase the default pipeline timeout to 2h in `Settings > CI/CD > General Pipelines > Timeout`.

## Commit Message Guideline
The [Conventional Commits 1.0.0][conventional-commits] specification is applied with the following
amendments:

* Only the following types are allowed:
  * build: changes to the CMake build system, dependencies or other build-related tooling
  * ci: changes to CI configuration files and scripts
  * docs: documentation only changes to overall project or code
  * feat: a new feature is added or a previously provided one explicitly removed
  * fix: bug fix
  * perf: performance improvement
  * refactor: rewrite of code logic that neither fixes a bug nor adds a feature
  * style: improvements to code style without logic change
  * test: addition of a new test or correction of an existing one
* Only the following optional scopes are allowed:
  * input: library input module
  * plugin: any plugin
  * render: library render module
  * win: library win module
  * wl: Wayland windowing system related
  * x11: X11 windowing system related
* Any line of the message must be 90 characters or shorter.
* Angular's [Revert][angular-revert] and [Subject][angular-subject] policies are applied.

### Example

    fix(input): provide correct return value

    For function exampleFunction the return value was incorrect.
    Instead provide the correct value A by changing B to C.

### Tooling
See [Wrapland's documentation][wrapland-tooling] for available tooling.

[angular-revert]: https://github.com/angular/angular/blob/3cf2005a936bec2058610b0786dd0671dae3d358/CONTRIBUTING.md#revert
[angular-subject]: https://github.com/angular/angular/blob/3cf2005a936bec2058610b0786dd0671dae3d358/CONTRIBUTING.md#subject
[conventional-commits]: https://www.conventionalcommits.org/en/v1.0.0/#specification
[frameworks-style]: https://community.kde.org/Policies/Frameworks_Coding_Style
[issue]: https://github.com/winft/como/issues
[pull-requests]: https://github.com/winft/como/pulls
[wrapland-large-changes]: https://github.com/winft/wrapland/blob/master/CONTRIBUTING.md#issues-for-large-changes
[wrapland-submissions]: https://github.com/winft/wrapland/blob/master/CONTRIBUTING.md#submission-guideline
[wrapland-tooling]: https://github.com/winft/wrapland/blob/master/CONTRIBUTING.md#tooling
