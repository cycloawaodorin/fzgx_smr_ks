#!/bin/bash

VERSION=$(grep '#define VERSION' version.hpp | sed -E 's/.*"([^"]+)".*/\1/')
cd ../
zip "./$1.${VERSION}.zip" $1.auo2
cd -
