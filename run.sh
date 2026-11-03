#!/bin/bash
case "$OSTYPE" in
    darwin*)
        echo "mac"
        make mac
    ;;
    linux*)
        echo "linux"
        make linux
    ;;
esac
./bin/serve