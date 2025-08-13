#!/bin/bash
phpize
./configure --enable-xdebug-dev $@
cat Makefile
make all

