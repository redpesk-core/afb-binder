# Application Framework Binder

This project provides the binder component of the microservice architecture
for Application Framework.

This project is available here <https://github.com/redpesk/afb-binder>.


## License and copying

This software if available in dual licensing. See file LICENSE.txt for detail

## Building

### Requirements

Building the Application framework binder has been tested successfully
on most Linux distributions.

It requires the following libraries

* libafb (<https://github.com/redpesk/afb-libafb>)
* json-c

### Simple compilation

The following commands will install the binder in your subdirectory
**$HOME/.local** (instead of **/usr/local** the default when 
*CMAKE_INSTALL_PREFIX* isn't set).

```sh
git clone https://github.com/redpesk/afb-binder
cd afb-binder
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=$HOME/.local ..
make install
```

or

```sh
git clone https://github.com/redpesk/afb-binder
cd afb-binder
./mkbuild.sh -p $HOME/.local install
```

***** TO BE COMPLETED *****
