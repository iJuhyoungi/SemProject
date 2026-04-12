#!/bin/bash


pyocd erase -t mimxrt1021dag5a --chip

pyocd flash build/MIMXRT1020_BareMetal --format elf -t mimxrt1021dag5a


