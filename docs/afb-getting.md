# Getting the binder

The binder is installed by default on all redpesk distributions.

It is valuable to install the binder *afb-binder*, its simple client *afb-client*
and the headers for developing bindings *afb-binding* on its development environment,
because it allows two important things:

- communication with redpesk services
- first step debugging within a rich environment

## Installing from packages

You can run the same command on a target runing a redpesk OS or in the [SDK container]({% chapter_link sdk-container-doc.overview %}) (development mode).

```bash
dnf install afb-binder afb-client afb-binding-devel
```

## Installing from rebuild

For older distributions that are not supported by IoT.bzh,
or for less commonly used distributions (gentoo, arch, ...)
it is possible to build from sources.

Building from sources is done in 4 steps:

- install required dependencies
- clone source repositories
- setup environment
- compile and install sources

### Install dependencies

When developing inside the SDK container, to install the build dependencies, run the following command:

```bash
dnf builddep afb-binder
```

### Get the sources

Building the binder and the client from the sources requires cloning of the following repositories:

- afb-binding: https://github.com/redpesk-core/afb-binding
- afb-libafb:  https://github.com/redpesk-core/afb-libafb
- afb-binder:  https://github.com/redpesk-core/afb-binder
- afb-client:  https://github.com/redpesk-core/afb-client

It can be achieved as below:

    git clone https://github.com/redpesk-core/afb-binding.git
    git clone https://github.com/redpesk-core/afb-libafb.git
    git clone https://github.com/redpesk-core/afb-binder.git
    git clone https://github.com/redpesk-core/afb-client.git

### Compile and install

Before compiling you have to choose the base directory where you want to
install the programs and development files. That directory is noted PREFIX.
The common values of PREFIX are:

- /usr
- /usr/local
- $HOME/.local

The two first require to have privilege (as user root), the latter doesn't.

Then to compile the whole set, enter the following commands:

    export PREFIX=$HOME/.local
    export CPATH=$PREFIX/include:$CPATH
    export LD_LIBRARY_PATH=$PREFIX/lib64:$PREFIX/lib:$LD_LIBRARY_PATH
    export LD_RUN_PATH=$PREFIX/lib64:$PREFIX/lib:$LD_RUN_PATH
    export LIBRARY_PATH=$PREFIX/lib64:$PREFIX/lib:$LIBRARY_PATH
    export PATH=$PREFIX/bin:$PATH
    export PKG_CONFIG_PATH=$PREFIX/lib64/pkgconfig:$PREFIX/lib/pkgconfig:$PKG_CONFIG_PATH

    afb-binding/mkbuild.sh install
    WITHOUT_CYNAGORA=ON afb-libafb/mkbuild.sh install
    afb-binder/mkbuild.sh install
    afb-client/mkbuild.sh install

It is done!

