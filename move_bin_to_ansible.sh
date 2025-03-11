#!/bin/bash

mkdir -p $HOME/nii-sakura-cluster-configs/monitoring/ansible/roles/infiniband_exporter/files/

cp ./infiniband/infiniband-exporter \
 $HOME/nii-sakura-cluster-configs/monitoring/ansible/roles/infiniband_exporter/files/infiniband-exporter

cp ./pcm//bin/pcm-memory-exporter.out \
 $HOME/nii-sakura-cluster-configs/monitoring/ansible/roles/pcm_memory_exporter/files/pcm-memory-exporter

cp ./pcm//bin/pcm-pcie-exporter.out \
 $HOME/nii-sakura-cluster-configs/monitoring/ansible/roles/pcm_pcie_exporter/files/pcm-pcie-exporter
