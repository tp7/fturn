## fturn. ##

### What ###
Fast implementation of TurnLeft(), TurnRight() and Turn180() AviSynth functions.

### How ###
```
FTurnLeft(chroma=true, mt=true)
FTurnRight(chroma=true, mt=true)
FTurn180(chroma=true)
```

### Why ###
Great performance improvements for antialiasing scripts like maa. You can disable chroma processing in case you don't need it (almost often in antialiasing scripts).

### Requirements ###
SSSE3 is required. This means you need to have at least Core 2 Duo.

### License ###
This project is licensed under the [MIT license](http://opensource.org/licenses/MIT). Binaries are [GPL v2](http://www.gnu.org/licenses/gpl-2.0.html) because if I understand licensing stuff right (please tell me if I don't) they must be.
