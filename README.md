# wicc 
Windows Imaging Component(WIC) CLI test tool

## Usage
    Syntax:  wicc [options or files ...]

    Options: begins with [- or /]
     l, L    List installed Components
     c, C    Show Componentinfo
     m, M    Preferred Microsoft Component
     oldpng  Use Old(Windows7) PNG Decoder

    >wicc -c a.png
    a.png
    Author: Microsoft
    Name: PNG Decoder
    Version: 2.0.0.0
    CLSID: {E018945B-AA86-4008-9BD4-6777A1E40C11}
    Frame: 0 Width: 240 Height: 240

    >wicc -c -oldpng a.png
    a.png
    Author: Microsoft
    Name: PNG Decoder
    Version: 1.0.0.0
    CLSID: {389EA17B-5078-4CDE-B6EF-25C15175C751}
    Frame: 0 Width: 240 Height: 240

## Licence
MIT License