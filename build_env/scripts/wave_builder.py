#!/usr/bin/env python
import sys
import os
from yaml_intf import *

## @package wave_generator
#This scripts generates the wave.do inside the testcase dir. Modify this scripts as you need to improve debuggability in your project

ENABLE_CPU              = True
ENABLE_CPU_ROUTER_CFG   = False
ENABLE_DMNI             = True
ENABLE_CS               = True
ENABLE_PS               = False

def generate_wave(testcase_path, yaml_reader):
    
    system_model = get_model_description(yaml_reader)
    
    if system_model == "scmod" or system_model == "vhdl":
    
        wave_lines = generate_wave_generic(yaml_reader, system_model)
    
        wave_path = testcase_path + "/base_scenario/wave.do"
        try:
            if not os.path.isfile(wave_path):
                f = open(wave_path, "w+")
                f.writelines(wave_lines)
                f.close()
        except:
            sys.exit("\n[WARN] wave.do not created, testcase directory not created yet!!\n")
            pass

def generate_wave_generic(yaml_r, system_model):
    
    mpsoc_x_dim =       get_mpsoc_x_dim(yaml_r)
    mpsoc_y_dim =       get_mpsoc_y_dim(yaml_r)
    subnet_number =     get_subnet_number(yaml_r)
    
    wave_lines = []
    wave_lines.append("onerror {resume}\n")
    wave_lines.append("quietly WaveActivateNextPane {} 0\n") 
    
    wave_lines.append("add wave -noupdate /test_bench/Memphis/clock\n")
    
    
    app_injector_send = "add wave -noupdate -group {App Injector} -group send -radix hexadecimal /test_bench/App_Injector/"
    app_injector_receive = "add wave -noupdate -group {App Injector} -group receive -radix hexadecimal /test_bench/App_Injector/"
    app_injector_monitor = "add wave -noupdate -group {App Injector} -group monitor -radix hexadecimal /test_bench/App_Injector/"
    
    wave_lines.append(app_injector_monitor + "EA_new_app_monitor\n")
    wave_lines.append(app_injector_monitor + "app_cluster_id\n")
    wave_lines.append(app_injector_monitor + "app_task_number\n")
    wave_lines.append(app_injector_monitor + "app_start_time\n")
    wave_lines.append(app_injector_monitor + "app_name\n")
    wave_lines.append(app_injector_send + "tx\n")
    wave_lines.append(app_injector_send + "data_out\n")
    wave_lines.append(app_injector_send + "credit_in\n")
    wave_lines.append(app_injector_send + "EA_send_packet\n")
    wave_lines.append(app_injector_send + "packet_size\n")
    wave_lines.append(app_injector_receive + "EA_receive_packet\n")
    wave_lines.append(app_injector_receive + "rx\n")
    wave_lines.append(app_injector_receive + "data_in\n")
    wave_lines.append(app_injector_receive + "credit_out\n")
                    
    pe_index = 0
    
    for x in range(0, mpsoc_x_dim):
        for y in range(0, mpsoc_y_dim):
            
            pe_index = (y * mpsoc_x_dim) + x
            
            if system_model == "scmod":
                pe = str(x) + "x" + str(y)
                pe_group = "add wave -noupdate -group {PE "+pe+"} "
                signal_path = "-radix hexadecimal /test_bench/Memphis/PE"+pe
                PS_router_name = "ps_router"
            else: #vhdl
                pe = str(pe_index)
                pe_group = "add wave -noupdate -group {PE"+str(x) + "x" + str(y)+"} "
                signal_path = "-radix hexadecimal /test_bench/Memphis/proc("+pe+")/PE"
                PS_router_name = "PS_router"
            
            cpu_group =  "-group CPU_"+pe+" " 
            
            if ENABLE_CPU:
            
                wave_lines.append(pe_group + cpu_group + signal_path + "/current_page\n")
                wave_lines.append(pe_group + cpu_group + signal_path + "/irq_mask_reg\n")
                wave_lines.append(pe_group + cpu_group + signal_path + "/irq_status\n") 
                wave_lines.append(pe_group + cpu_group + signal_path + "/irq\n") 
                wave_lines.append(pe_group + cpu_group + signal_path + "/cpu_mem_address_reg\n") 
                wave_lines.append(pe_group + cpu_group + signal_path + "/cpu_mem_data_write_reg\n") 
                wave_lines.append(pe_group + cpu_group + signal_path + "/cpu_mem_data_read\n") 
                wave_lines.append(pe_group + cpu_group + signal_path + "/cpu_mem_write_byte_enable\n") 
                
                if ENABLE_CPU_ROUTER_CFG:
                    router_config = "-group router_config_"+pe+" "
                    
                    wave_lines.append(pe_group + cpu_group + router_config + signal_path + "/config_r_cpu_inport\n") 
                    wave_lines.append(pe_group + cpu_group + router_config + signal_path + "/config_r_cpu_outport\n") 
                    wave_lines.append(pe_group + cpu_group + router_config + signal_path + "/config_r_cpu_valid\n") 
                    wave_lines.append(pe_group + cpu_group + router_config + " -divider ConfigModuleIN\n")
                    wave_lines.append(pe_group + cpu_group + router_config + signal_path + "/config_inport_subconfig\n")
                    wave_lines.append(pe_group + cpu_group + router_config + signal_path + "/config_outport_subconfig\n")
                    wave_lines.append(pe_group + cpu_group + router_config + signal_path + "/config_valid_subconfig\n")
            
            dmni_group = "-group {DMNI "+pe+"} "
            pe_path = signal_path
            signal_path = pe_path+"/dmni_qos"
            send_ps = "-group send_"+pe+"_PS "
            receive_ps = "-group receive_"+pe+"_PS "
            
            if ENABLE_DMNI:
                wave_lines.append(pe_group + dmni_group + signal_path + "/send_active\n");
                wave_lines.append(pe_group + dmni_group + signal_path + "/receive_active\n");
                
                        
                wave_lines.append(pe_group + dmni_group + send_ps + signal_path + "/tx_ps\n") 
                wave_lines.append(pe_group + dmni_group + send_ps + signal_path + "/data_out_ps\n")
                wave_lines.append(pe_group + dmni_group + send_ps + signal_path + "/credit_in_ps\n") 
                
                wave_lines.append(pe_group + dmni_group + receive_ps + signal_path + "/rx_ps\n") 
                wave_lines.append(pe_group + dmni_group + receive_ps + signal_path + "/data_in_ps\n") 
                wave_lines.append(pe_group + dmni_group + receive_ps + signal_path + "/credit_out_ps\n") 
                    
                config = "-group config_"+pe+" "
                
                wave_lines.append(pe_group + dmni_group + config + signal_path + "/config_valid\n") 
                wave_lines.append(pe_group + dmni_group + config + signal_path + "/config_code\n") 
                wave_lines.append(pe_group + dmni_group + config + signal_path + "/config_data\n")
            
            if ENABLE_CS:
                CS_router_group = "-group CS_routers_DMNI_OUT"+pe+" "
                
                for i in range(0,subnet_number-1):
                    s = str(i);
                    subnet_group = "-group CS("+s+")_"+pe+" "
                    full_path = pe_group + CS_router_group + subnet_group + signal_path
                    
                    wave_lines.append(pe_group + CS_router_group + subnet_group + "-divider send\n") 
                    
                    wave_lines.append(full_path + "/tx_cs("+s+")\n") 
                    wave_lines.append(full_path + "/data_out_cs("+s+")\n") 
                    wave_lines.append(full_path + "/credit_in_cs("+s+")\n") 
                   
                    wave_lines.append(pe_group + CS_router_group + subnet_group + "-divider receive\n") 
                   
                    wave_lines.append(full_path + "/rx_cs("+s+")\n") 
                    wave_lines.append(full_path + "/data_in_cs("+s+")\n") 
                    wave_lines.append(full_path + "/credit_out_cs("+s+")\n") 
                
            if ENABLE_PS:
                input_name = "EAST", "WEST", "NORTH", "SOUTH"
                signal_path = pe_path+"/"+PS_router_name+"/"
                router_group = "-group {PS_router "+pe+"} "
                wave_lines.append(pe_group + router_group + "-divider receive\n") 
                count=0
                for input in input_name:
                    p = str(count)
                    i_name = "input_"+input;
                    input_group = "-group {"+i_name+" "+pe+"} "
                    wave_lines.append(pe_group + router_group + input_group + signal_path + "rx("+p+")\n") 
                    wave_lines.append(pe_group + router_group + input_group + signal_path + "data_in("+p+")\n")  
                    wave_lines.append(pe_group + router_group + input_group + signal_path + "credit_o("+p+")\n")
                    count= count+1
                    
                wave_lines.append(pe_group + router_group + "-divider send\n")
                count=0
                for input in input_name:
                    p = str(count)
                    i_name = "output_"+input;
                    input_group = "-group {"+i_name+" "+pe+"} "
                    wave_lines.append(pe_group + router_group + input_group + signal_path + "tx("+p+")\n") 
                    wave_lines.append(pe_group + router_group + input_group + signal_path + "data_out("+p+")\n")  
                    wave_lines.append(pe_group + router_group + input_group + signal_path + "credit_i("+p+")\n")  
                    count = count+1;
            
    wave_lines.append("configure wave -signalnamewidth 1\n")
    wave_lines.append("configure wave -namecolwidth 217\n")
    wave_lines.append("configure wave -timelineunits ns\n")
    
    return  wave_lines
    
