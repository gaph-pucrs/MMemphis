README.txt
 Created on: Aug 9, 2019
     Author: Marcelo Ruaro (marcelo.ruaro@acad.pucrs.br)

     The HALL folter centralize all functions that have a direct relation to the architeture.
     The following features are implemented:
     	- Syscall handler
     	- Interrupt handler
     	- Timers (Scheduler, monitors)
     	- Memory Mapped Register (MMR)
     	- Drivers
     
     The intention with this HAL is to allows the kernel developer only works in the files inside the folder when
     a new architeture needs to be integrated.
     
     Each HAL directory contains a folder that specify the name of the target CPU. Inside the directory there are
     three files: 
     	- HAL_kernel_asm.S 	: Implements the assembly code to handle interrutions and syscalls, saving and restoring the task context
     	- HAL_kernel(.c,.h) : Implements driver for DMNI, timers, Interruption Mask Set, and MMR. 
     	- HAL_task_ams.c 	: Implements the assembly code to start the execution of task and give access to SysCall jumping