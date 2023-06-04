#!/bin/bash

for FILE in *.png ; do
    OUTNAME=`basename "$FILE" .png`.h
    OUTNAMEC=`basename "$FILE" .png`
    gdk-pixbuf-csource --raw --name=$OUTNAMEC $FILE > $OUTNAME
done
