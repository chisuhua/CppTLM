# SystemC Headers

Place SystemC header files here for local (bundled) SystemC support.

## Required headers

At minimum, the following headers (or a symlink to a SystemC installation) are needed:

```
systemc.h
tlm.h
tlm_core/
tlm_utils/
sysc/
```

## Setup options

### Option 1: Symlink to system installation
```bash
ln -s /usr/include/systemc/* .
```

### Option 2: Copy headers
Copy the contents of your SystemC installation's `include/` directory here.

### Option 3: Use Accellera reference implementation
Download from https://www.accellera.org/downloads/standards/systemc

## Enabling

```bash
cmake -S . -B build -DUSE_SYSTEMC=ON
```
