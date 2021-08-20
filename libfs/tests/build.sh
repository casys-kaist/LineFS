#!/bin/bash
(cd .. && make clean ;make -j20) && make clean; make -j20
