#!/bin/bash


pyocd erase -t mimxrt1021dag5a --chip

pyocd flash build/MIMXRT1020_SDRAM --format elf -t mimxrt1021dag5a


