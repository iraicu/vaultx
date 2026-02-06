#!/bin/bash

ssh fpganode2 "/home/iraicu/vaultx/vaultx -P -k 31 -n 2 -F /ssd-raid0/iraicu/tmp -T /ceph/iraicu/vaults -t 16 -b true"
ssh torus "/home/iraicu/vaultx-integration/vaultx -k 32 -f /ceph/iraicu/tmp -t 32 -b true"
ssh athena "/home/iraicu/vaultx/vaultx -k 32 -f /ceph/iraicu/tmp -t 48 -b true"
ssh nvmebox "/home/iraicu/vaultx-integration/vaultx -k 32 -f /ceph/iraicu/tmp -t 64 -b true"
ssh gpubox "/home/iraicu/vaultx/vaultx -k 32 -f /ceph/iraicu/tmp -t 96 -b true"
ssh s8 "/home/iraicu/vaultx-integration/vaultx -k 32 -f /ceph/iraicu/tmp -t 384 -b true"
