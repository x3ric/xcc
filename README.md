<div align="center">

### XCC

XCC is a simple C compiler based on chibic, featuring support for including files from websites using curl and shebang support.

#### Installation

<pre>
git clone https://github.com/x3ric/xcc
cd xcc
sudo make install
</pre>

#### Usage

<pre>
xc
</pre>

#### Features

**Chibic Based:** Built upon chibic, XCC supports most C11 features.

**GCC Backend:** Runs gcc with Web Includes & Shebang Support through XCC.

**Web Includes:** Includes files from websites using curl.

**Shebang Support:** Supports executing C scripts directly.

#### For examples:

<pre>
./examples/x11window
</pre>

#### Internals

XCC comprises the following stages:

**Tokenize:** Breaks input into tokens.

**Preprocess:** Expands macros and interprets preprocessor directives.

**Parse:** Constructs abstract syntax trees and assigns types.

**Codegen:** Generates assembly text for AST nodes.


<br>
</p><a href="https://archlinux.org"><img alt="Arch Linux" src="https://img.shields.io/badge/Arch_Linux-1793D1?style=for-the-badge&logo=arch-linux&logoColor=D9E0EE&color=000000&labelColor=97A4E2"/></a><br>
