#!/bin/bash

pandoc  -N --filter pandoc-crossref \
	intro.md \
        rewriting-math-equations-into-basic-ures.md \
	references.md \
       	-o programming-guide.html

