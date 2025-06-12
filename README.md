# Boreas

Implementation of PIC/MPM from Stomakhin et al. 2013 paper "A material point method for snow simulation".

## Compilation

Compiles with local libraries using cmake's findpackage.

From the source folder, run the following:

```
mkdir build
cd build
cmake ..
cmake --build .
```

If packages aren't found and compilation fails, check your PATH.

## Controls

**WASD** for moving the camera, **shift** to go down, **space** to go up. **Mouse wheel** or **Page up/down** to zoom. **Q** releases/captures the mouse from the window.
