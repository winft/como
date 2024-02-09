<!--
SPDX-FileCopyrightText: 2024 Roman Gilg <subdiff@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
-->

<div align="center">
<p>
    <picture>
      <source media="(prefers-color-scheme: dark)" srcset="docs/assets/logo-dark-wide.png">
      <source media="(prefers-color-scheme: light)" srcset="docs/assets/logo-bright-wide.png">
      <img alt="The Compositor Modules wide logo" src="docs/assets/logo-bright-wide.png" width="600">
    </picture>
</p>

<p>
<i>The Compositor Modules (COMO)</i> are a robust and versatile set of libraries
<br>
to create compositors for the Wayland and X11 windowing systems on Linux.
</p>
</div>

# Features

**Compatibility**
<br>
The Compositor Modules currently integrate primarily with
KDE's Plasma Desktop but can be used with other desktop environments as well.
This cross-desktop interoperability will be expanded upon in the future.

**Ease of Use**
<br>
With the Compositor Modules a Wayland compositor can be created with a handful of lines only.
See our MVP [Minico](tests/minico) for an example of that.

**Customizability**
<br>
The Compositor Modules make heavy use of C++ templates. This allows consumers to replace
many functions and types with customized versions when required.

# Values

**Stability and robustness**

This is achieved through upholding strict development standards
and deploying modern development methods to prevent regressions and code smell.

**Collaboration with competitors and upstream partners**

We want to overcome antiquated notions on community divisions
and work together on the best possible Linux graphics platform.

**Value the knowledge of experts but also the curiosity of beginners**

Well defined and transparent decision processes enable expert knowledge to proliferate
and new contributors to easily find help on their first steps.

# Installation
The Compositor Modules can be compiled from source.
If you do that manually you have to check for your specific distribution
how to best get the required dependencies.

You can also make use of the FDBuild tool to automate that process as described
[here](CONTRIBUTING.md#compiling).

# Usage
It's easiest to link via CMake to the Compositor Modules libraries that you want to use.
As an example check out the test code of [Minico](tests/minico) and the [Plasma binaries](tests/plasma).

# Development
The [CONTRIBUTING.md](CONTRIBUTING.md) document contains all information
on how to get started with:
* providing useful debug information,
* building the Compositor Modules
* and doing your first code submission to the project.
