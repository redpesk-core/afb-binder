# Getting the binder

The binder if installed by default on all redpesk distributions.

It is valuable to install the binder *afb-binder*, its simple client *afb-client*
and the headers for developing bindings *afb-binding* on its development environment,
because it allows two important things:

- communication with redpesk services
- first step debugging within a rich environment

## Installing from packages

IoT.bzh provides an SDK for some common linux distributions at the download URL

    https://download.redpesk.bzh/redpesk-lts/{{ site.redpesk-os.latest }}/sdk/

You must first add the repository to your list of repositories.

Then you have to install the packages.

### Installing for Fedora:

Add the repository to your list of repositories.

    sudo dnf config-manager --add-repo https://download.redpesk.bzh/redpesk-lts/{{ site.redpesk-os.latest }}/sdk/redpesk-sdk_fedora.repo

Install the packages

    sudo dnf install afb-binder afb-client afb-binding-devel

### Installing for open SUSE:

Add the repository to your list of repositories.

    sudo zypper ar -f -r https://download.redpesk.bzh/redpesk-lts/{{ site.redpesk-os.latest }}/sdk/redpesk-sdk_suse.repo redpesk-sdk
    sudo zypper --gpg-auto-import-keys ref
    sudo zypper dup --from redpesk-sdk

Install the packages

    sudo zypper in afb-binder afb-client afb-binding-devel

### Installing on Debian 10

Add the repository to your list of repositories.

    wget -O - https://download.redpesk.bzh/redpesk-lts/{{ site.redpesk-os.latest }}/sdk/Debian_10/Release.key | apt-key add -
    sudo apt-add-repository 'deb https://download.redpesk.bzh/redpesk-lts/{{ site.redpesk-os.latest }}/sdk/xUbuntu_20.04/ ./'

Install the packages

    sudo apt update
    sudo apt install afb-binder afb-client afb-binding-dev

### Installing on Ubuntu 20

Add the repository to your list of repositories.

    wget -O - https://download.redpesk.bzh/redpesk-lts/{{ site.redpesk-os.latest }}/sdk/xUbuntu_20.04/Release.key | apt-key add -
    sudo apt-add-repository 'deb https://download.redpesk.bzh/redpesk-lts/{{ site.redpesk-os.latest }}/sdk/xUbuntu_20.04/ ./'

Install the packages

    sudo apt update
    sudo apt install afb-binder afb-client afb-binding-dev

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

The exact workflow for installing dependencies required for the build depends on
your distribution. The procedure below applies on widely used distributions.

- For Fedora or open SUSE

    dnf install json-c-devel systemd-devel readline-devel libmicrohttpd-devel file-devel cmake make gcc

- For Debian or Ubuntu

    apt install libjson-c-dev libsystemd-dev libreadline-dev libmicrohttpd-dev libmagic-dev cmake make gcc

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

