#!/bin/bash

zip -r tests.notazip tests/
zip -r group15.zip main.c bun_parse.c bun.h Makefile setup.sh tests.notazip
python3 ./submission_sanity_checker.py ./group15.zip
