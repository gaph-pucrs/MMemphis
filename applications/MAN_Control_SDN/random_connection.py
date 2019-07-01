#!/usr/bin/python
import sys
import math
import os
import commands
import yaml
import random





def search_value(word):
    with open("noc_manager.h") as search:
        for line in search:
            line = line.rstrip()  # remove '\n' at end of line
            if word in line:
                return int(line.split(" ")[2])


def conv_id_to_addr(id, x_dim, y_dim):
    y_addr = int(id / x_dim)
    x_addr = id - (y_addr*x_dim)
    
    return x_addr << 8 | y_addr

def search_cluster(source_x, source_y, X_CLUSTER, Y_CLUSTER):

    nc_x = source_x / X_CLUSTER;
    nc_y = source_y / Y_CLUSTER;

    return (nc_x << 8 | nc_y);

def main():  
    
    
    X_DIMENSION      = search_value("#define XDIMENSION")
    Y_DIMENSION      = search_value("#define YDIMENSION")
    X_CLUSTER        = search_value("#define XCLUSTER")
    Y_CLUSTER        = search_value("#define YCLUSTER")  
    SUBNET_NUMBER    = search_value("#define SUBNETS_NUMBER")
    #SPLIT            = search_value("#define SPLIT")
    
    cluster_X_number = X_DIMENSION / X_CLUSTER
    cluster_Y_number = Y_DIMENSION / Y_CLUSTER
    cluster_number = ((X_DIMENSION*Y_DIMENSION)/(X_CLUSTER*Y_CLUSTER))

    total_conn_number = X_DIMENSION*Y_DIMENSION;
    
    
    try:
        pro_arg = int(sys.argv[1])
        #Proportion of local connection 1.0 == 100% of connection made locally at each controller
        LOCAL_PROPORTION = pro_arg / 100.0
        if (LOCAL_PROPORTION > 0.0) and (LOCAL_PROPORTION < 1.0):
            print "LOCAL_PROPORTION:", LOCAL_PROPORTION
    except:
        print "Bad arguments for local proportion!"
        exit()
    
    cluster_conn_number = int(total_conn_number * LOCAL_PROPORTION)
    global_conn_number = total_conn_number - cluster_conn_number

    
    print total_conn_number
    print global_conn_number
    print cluster_conn_number
    print cluster_conn_number * cluster_number
    
    local_counter = 0
    global_counter = 0
    
    CONNECTIONS = []
    
    for s in range(0,SUBNET_NUMBER):
        
        source_list = []
        target_list = []
        
        #Inicializa a lista com todos os PEs
        for x in range(0, X_DIMENSION):
            for y in range(0, Y_DIMENSION):
                source_list.append(x << 8 | y)
                target_list.append(x << 8 | y)
                #print "Added ", x,"x",y
                
                
                
                
        #CONEXOES LOCAIS ----------------------------------------
        cluster_id = 0;
        #Para cada cluster
        for j in range(0, cluster_conn_number):                

            cluster_per_layer = (X_DIMENSION / X_CLUSTER);
            layers_above = cluster_id / cluster_per_layer;
            y_offset = layers_above * Y_CLUSTER;
            x_offset = (cluster_id - (cluster_per_layer * layers_above)) * X_CLUSTER;
            
            cluster_id = cluster_id + 1
            
            if (cluster_id == cluster_number):
                cluster_id = 0
            
            print "Cluster "+str(x_offset/X_CLUSTER)+"x"+str(y_offset/Y_CLUSTER)
            
            #Remove um src e um target que estao dentro do offset
            src_addr = 1;
            tgt_addr = 1;
            
            #Selects a valid communication flow
            while(True):
                src_index = random.randint(0,len(source_list)-1);
                tgt_index = random.randint(0,len(target_list)-1);
                #print "src_index: ", src_index, "tgt_index: ", tgt_index
                
                src_addr = source_list[src_index];
                tgt_addr = target_list[tgt_index];
                
                sx = src_addr >> 8
                sy = src_addr & 0xFF
                tx = tgt_addr >> 8
                ty = tgt_addr & 0xFF
                
                print "Trying local" + str(sx)+"x"+str(sy)+"->"+str(tx)+"x"+ str(ty)
                print j
                
                if (src_addr == tgt_addr):
                    continue
                
                if (sx < x_offset or sx >= x_offset + X_CLUSTER):
                    continue
                
                if (sy < y_offset or sy >= y_offset + Y_CLUSTER):
                    continue;
                
                if (tx < x_offset or tx >= x_offset + X_CLUSTER):
                    continue;
                
                if (ty < y_offset or ty >= y_offset + Y_CLUSTER):
                    continue;
                
                #print "Add local " + str(sx)+"x"+str(sy)+"->"+str(tx)+"x"+ str(ty)
            
                break;
            
            #Remove elements from main lists
            source_list.remove(src_addr)
            target_list.remove(tgt_addr)
            
            #Adds to the final list
            CONNECTIONS.append(src_addr<<16|tgt_addr)
            local_counter = local_counter + 1;
                    
        print "GLOBAIS"
        #CONEXOES GLOBAIS ----------------------------------------
        for c in range(0,global_conn_number):
            
            #Selects a valid communication flow
            while(True):
                src_index = random.randint(0,len(source_list)-1);
                tgt_index = random.randint(0,len(target_list)-1);
                #print "src_index: ", src_index, "tgt_index: ", tgt_index
                
                src_addr = source_list[src_index];
                tgt_addr = target_list[tgt_index];
                
                sx = src_addr >> 8
                sy = src_addr & 0xFF
                tx = tgt_addr >> 8
                ty = tgt_addr & 0xFF
                
                print "Trying global" + str(sx)+"x"+str(sy)+"->"+str(tx)+"x"+ str(ty)
                print c, global_conn_number
                
                #print "source x " + str(sx)+"x"+str(sy)
                cluster_addr = search_cluster(sx, sy, X_CLUSTER, Y_CLUSTER)
                        
                x_offset = (cluster_addr >> 8) * X_CLUSTER;
                y_offset = (cluster_addr & 0xFF) * Y_CLUSTER;
                
                
                if (src_addr == tgt_addr):
                    print "same address"
                    continue
                
                if (tx >= x_offset and tx < x_offset + X_CLUSTER and ty >= y_offset and ty < y_offset + Y_CLUSTER):
                    print "Same cluster"
                    if (c+1 < global_conn_number):
                        continue;
                
                #print "Add global " + str(sx)+"x"+str(sy)+"->"+str(tx)+"x"+ str(ty)
                break;
            
            #Remove elements from main lists
            source_list.remove(src_addr)
            target_list.remove(tgt_addr)
            
            #Adds to the final list
            CONNECTIONS.append(src_addr<<16|tgt_addr)  
            global_counter = global_counter + 1;
            
    print "LOCAL CONNECTIONS ", local_counter, (local_counter*100.0/(total_conn_number*SUBNET_NUMBER)),"%"        
    print "GLOBAL CONNECTIONS ", global_counter, (global_counter*100.0/(total_conn_number*SUBNET_NUMBER)),"%"  
       
                    #print local_counter,":",str(src>>8)+"x"+str(src&0xff)+"->"+str(tgt>>8)+"x"+str(tgt&0xff)+"["+str(s)+"]"
            
                    #final_string = final_string + str(src<<16|tgt) + ","
    
    #final_string = final_string[:-1] + "};"
    
    random.shuffle(CONNECTIONS)
    CONNECTIONS_NR = total_conn_number*SUBNET_NUMBER
    CONNECTION_PER_REQUESTER = CONNECTIONS_NR/cluster_number

    if (CONNECTIONS_NR % cluster_number) != 0:
        sys.exit("ERROR CONNECTION CANNOT BE DIVIDED BY CLUSTER NUMBER")
    

    #if SPLIT == 0:
    final_string = "unsigned int connections["+str(total_conn_number*SUBNET_NUMBER)+"] = {"
    for conn in CONNECTIONS:
        final_string = final_string + str(conn) + ","
    final_string = final_string[:-1] + "};"

    '''else:
        
        final_string = "unsigned int connections["+str(cluster_number)+"]["+str(CONNECTION_PER_REQUESTER)+"] = {"
        conn_index = 0
        for i in range(0, cluster_number):
            final_string = final_string + "{"
            for k in range(0, CONNECTION_PER_REQUESTER):
                final_string = final_string + str(CONNECTIONS[conn_index]) + ","
                conn_index = conn_index + 1
            final_string = final_string[:-1] + "},"
        final_string = final_string[:-1]
        final_string = final_string + "};"
    '''
    
    local_counter = (local_counter*100.0/CONNECTIONS_NR)
    global_counter = (global_counter*100.0/CONNECTIONS_NR)
    
    os.system("reset")
    
    subnet_repo = "path_repository/"+str(SUBNET_NUMBER)+"NETS"
    if not os.path.isdir(subnet_repo):
        os.makedirs(subnet_repo)
    final_directory = subnet_repo+"/local"+str(int(LOCAL_PROPORTION*100))
    if not os.path.isdir(final_directory):
        os.makedirs(final_directory)
    
    final_file_path = final_directory + "/"+str(X_DIMENSION)+"x"+str(Y_DIMENSION)+"-"+str(X_CLUSTER)+"x"+str(Y_CLUSTER)+".txt"
    
    line1 = "\n\n//"+str(X_DIMENSION)+"x"+str(Y_DIMENSION)+"-"+str(X_CLUSTER)+"x"+str(Y_CLUSTER)+"-"+str(SUBNET_NUMBER)+"NETS    Local = "+str(local_counter)+"%   Global = "+str(global_counter)+"%\n"
    line2 = "#define CONNECTIONS_NR    "+str(CONNECTIONS_NR)+"\n"
    line3 = final_string+"\n\n"
    
    
    try:
        path_file  = open(final_file_path, "w")
        path_file.write(line1)
        path_file.write(line2)
        path_file.write(line3)
    except:
        print "DEU PAU NA HORA DE CRIAR O ARQUIVO\n"
        sys.exit()
    
    
    print line1, line2, line3
    
    print final_file_path
    print "File created succesifully!!!\n"
    
    
main()
