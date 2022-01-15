#!/bin/bash

pandoc intro.md \
       rewriting-math-equations-into-basic-ures.md \
       references.md \
       --toc -N \
       --include-in-header style.tex \
       --highlight-style tango \
       -V linkcolor:blue \
       -V geometry:letterpaper \
       -V geometry:margin=2cm \
       -V mainfont="DejaVu Serif" \
       -V monofont="DejaVu Sans Mono" \
       -V fontsize=12pt \
       -o programming-guide.pdf

