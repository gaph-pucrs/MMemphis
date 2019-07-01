#!/usr/bin/python
import sys
import math
import os
import commands
import subprocess

def search_value(word):
    with open("noc_manager.h") as search:
        for line in search:
            line = line.rstrip()  # remove '\n' at end of line
            if word in line:
                return int(line.split(" ")[2])



XDIMENSION      = search_value("#define XDIMENSION")
YDIMENSION      = search_value("#define YDIMENSION")
XCLUSTER        = search_value("#define XCLUSTER")
YCLUSTER        = search_value("#define YCLUSTER")  
SPLIT           = search_value("#define SPLIT")


CLUSTER_X_NUMBER =   (XDIMENSION/XCLUSTER)
CLUSTER_Y_NUMBER =   (YDIMENSION/YCLUSTER)

os.system("rm *noc_manager*.c")
os.system("rm *path_requester*.c")

requester_id = 0

for x in range(0, CLUSTER_X_NUMBER):
    for y in range(0, CLUSTER_Y_NUMBER):
        cluster_id = y*CLUSTER_X_NUMBER + x
        manager_name = "noc_manager"+str(x)+"x"+str(y)+".c"
        os.system("cp manager_base "+manager_name)
        os.system("sed -i '/#define CLUSTER_ID/c\#define CLUSTER_ID "+str(cluster_id)+"' "+manager_name)
        '''
        Path requester comentado pq agora estou avaliando app reais
        if SPLIT or (x == 0 and y == 0):
            requester_name = "path_requester"+str(x)+"x"+str(y)+".c"
            os.system("cp requester_base "+requester_name)
            os.system("sed -i '/#define my_id/c\#define my_id "+str(requester_id)+"' "+requester_name)
            requester_id = requester_id + 1
        '''
