##------------------------------------------------------------------------------------------------
##  Memphis Kernel Boot
##	Author: Marcelo Ruaro (September, 2019)
##  Research group: GAPH-PUCRS    -    contact  marcelo.ruaro@acad.pucrs.br and fernando.moraes@pucrs.br
##  Brief description: Implements the interrupt, syscall handling for MIPS processor
##
##  MIPS has jumps to the following addresses:
##
##    	0x3C: memory address that CPU jumps when a hardware interruption occurs
##	        - NoC Interrupction
##	        - Scheduler timer
##	        - Monitors timers
##    	0x44: memory address that CPU jumps when a task call by a syscall
##
##  MIPS has the following co-processor register mapped:
##		- $10: page
##		- $12: interruptions
##      - $14: program counter
##------------------------------------------------------------------------------------------------

        .text      
        .align  2
        .set noat		#Disable warning when $at register is used by user
        .globl  entry
        .ent    entry
entry:
        .set noreorder
        
        li      $sp,sp_addr
        mtc0    $0,$12          # Disables interrupts using $1 value (0==FALSE)
        jal     main
        nop

$L1:
        j $L1
        nop

        .org 0x3c		#Forces the next instruction to start at address 0x3c
#address 0x3c - This address is mapped inside reg_back of MIPS
interrupt_service_routine:
        j       HAL_interruption
        nop

#address 0x44 - This address is mapped inside reg_back of MIPS
system_service_routine:
        j       HAL_syscall
        nop


HAL_interruption:

        la      $27,current	#Uses register $27 to store current poiter address
        lw      $27,0($27)
                                   
        sw      $2,0($27)       # reg[0] = $v0
        sw      $3,4($27)       # reg[1] = $v1
        sw      $4,8($27)       # reg[2] = $a0
        sw      $5,12($27)      # reg[3] = $a1
        sw      $6,16($27)      # reg[4] = $a2
        sw      $7,20($27)      # reg[5] = $a3
        sw      $8,24($27)      # reg[6] = $t0
        sw      $9,28($27)      # reg[7] = $t1
        sw      $10,32($27)     # reg[8] = $t2
        sw      $11,36($27)     # reg[9] = $t3
        sw      $12,40($27)     # reg[10] = $t4
        sw      $13,44($27)     # reg[11] = $t5
        sw      $14,48($27)     # reg[12] = $t6
        sw      $15,52($27)     # reg[13] = $t7
        sw      $16,56($27)     # reg[14] = $s0
        sw      $17,60($27)     # reg[15] = $s1
        sw      $18,64($27)     # reg[16] = $s2
        sw      $19,68($27)     # reg[17] = $s3
        sw      $20,72($27)     # reg[18] = $s4
        sw      $21,76($27)     # reg[19] = $s5
        sw      $22,80($27)     # reg[20] = $s6
        sw      $23,84($27)     # reg[21] = $s7
        sw      $24,88($27)     # reg[22] = $t8
        sw      $25,92($27)     # reg[23] = $t9

        #Note that registers $26 and $27 are reserved for kernel, thus they are not being saved here
#       sw      $28,96($27)     # reg[24] = $gp - GP is not saved in order to allows PC to refer to absolute task page and not the relative page. The relative memory management is performed by a turnarround implemented insed PE.vhd and PE.cpp (PE top)
        sw      $29,100($27)    # reg[25] = $sp
        sw      $30,104($27)    # reg[26] = $s8
        sw      $31,108($27)    # reg[27] = $ra
        mfhi    $26             # Save the value of hi into $26
        sw      $26,112($27)    # reg[28] = $hi
        mflo    $26             # Save the value of lo into $26
        sw      $26,116($27)    # reg[29] = $lo
        mfc0    $26,$14         # Copy $14 from coprocessor 0 to the register $26
        sw      $26,120($27)    # reg[30] = save PC

        #Reset kernel stack pointer
        li $sp,sp_addr
        
        #JUMP TO INTERRUPT SERVICE ROUTINE
        lui     $5,0x2000    
        lw      $4,0x20($5)     		 # IRQ_STATUS - Get the status of interruption (address=0x20000020) storing in $4
        lw      $6,0x10($5)     		 # IRQ_MASK   - Get the status of mask (address=0x20000010) storing in $6
        jal     OS_interruption_handler  # Calls the OS_interruption_handler inside kernel
        and     $4,$4,$6        		 # Creates the final value of first argument (status) of function  OS_interruption_handler
        # Every jump in MIPS execute the always execute the instruction above jump. Some cases the compiler puts a nop after a jump. In this case the space of nop was reused to execute a usefull instruction

####################################################

HAL_syscall:
        # registers $26 and $27 are reserved for kernel       
        la      $27,current     # Loads the address of current TCB pointer at reg $27
        lw      $27,0($27)
        
        # Saves partial task context (save the value of arguments (A0 - A3) passed by the task to SystemCall)
        sw      $4,8($27)       # reg[2] = $a0
        sw      $5,12($27)      # reg[3] = $a1
        sw      $6,16($27)      # reg[4] = $a2
        sw      $7,20($27)      # reg[5] = $a3

        sw      $29,100($27)    # reg[25] = $sp
        sw      $31,108($27)    # reg[27] = $ra
        
        mfc0    $26,$14         # Get PC and stores in $26
        sw      $26,120($27)    # Saves PC at reg[30]

        # Reset the stack pointer to kernel
        li $sp,sp_addr

        # jumps to OS system calls handler
        jal     OS_syscall_handler
        nop
        
        
        # Verifies if scheduling is needed
        la      $26,schedule_after_syscall
        lw      $26,0($26)
        beqz    $26,restore_partial_task_context # IF (schedule_after_syscall == 0) THEN restore_partial_task_context ELSE save_remaining_task_context and calls Scheduler
        nop
        
        # The code below will be executed only if schedule_after_syscall == 1
        # Saves the context of the running task
        la      $4,current
        lw      $4,0($4)
        jal     save_remaining_task_context
        nop
        
        # Call scheduler inside kernel to select a new task if ready
        jal     OS_scheduler
        nop
        
        # Set the $a0 (first argmument) register pointer to current and calls HAL_run_scheduled_task to restore task context and to run it
        la      $4,current
        lw      $4,0($4)
        jal     HAL_run_scheduled_task
        nop
       
# Restore the remaining context of the running task since the scheduler not was called to selected a different task
restore_partial_task_context:

        # Loads the address of current TCB at reg $27
        la      $27,current
        lw      $27,0($27)        
        
        # Restores the partial context previously saved
        lw      $29,100($27)    # $sp = reg[25]
        lw      $31,108($27)    # $ra = reg[27]
        
        lw      $26,120($27)    # $pc = reg[30]
        lw      $27,124($27)    # Loads TCB.offset of the task (for paging setup)
        li      $1,0x1  
        
        mtc0    $27,$10         # Set page register
        jr      $26             # Jumps to pc
        mtc0    $1,$12          # Enables interrupts

        .set    reorder 
        .end    entry
        
###################################################
        
        .globl  save_remaining_task_context
        .ent    save_remaining_task_context
        .set    noreorder
        
save_remaining_task_context:

        sw      $2,0($4)        #  reg[0] = $v0
        sw      $3,4($4)        #  reg[1] = $v1
        # ...
        sw      $16,56($4)      #  reg[14] = $s0
        sw      $17,60($4)      #  reg[15] = $s1
        sw      $18,64($4)      #  reg[16] = $s2
        sw      $19,68($4)      #  reg[17] = $s3
        sw      $20,72($4)      #  reg[18] = $s4
        sw      $21,76($4)      #  reg[19] = $s5
        sw      $22,80($4)      #  reg[20] = $s6
        sw      $23,84($4)      #  reg[21] = $s7
        # ...
        sw      $30,104($4)     #  reg[26] = $s8
        
        jr      $31
        nop
        
        .set    reorder
        .end    save_remaining_task_context
        
###################################################

        .globl  HAL_run_scheduled_task
        .ent    HAL_run_scheduled_task
        .set    noreorder

# Restorse the full context of the task to be executed
HAL_run_scheduled_task:

        move    $27,$4          # Gets the first argument pointer and stores to reg $27
        
        lw      $2,0($27)       # $v0 = reg[0]
        lw      $3,4($27)       # $v1 = reg[1]
        lw      $4,8($27)       # $a0 = reg[2]
        lw      $5,12($27)      # $a1 = reg[3]
        lw      $6,16($27)      # $a2 = reg[4]
        lw      $7,20($27)      # $a3 = reg[5]
        lw      $8,24($27)      # $t0 = reg[6]
        lw      $9,28($27)      # $t1 = reg[7]
        lw      $10,32($27)     # $t2 = reg[8]
        lw      $11,36($27)     # $t3 = reg[9]
        lw      $12,40($27)     # $t4 = reg[10]
        lw      $13,44($27)     # $t5 = reg[11]
        lw      $14,48($27)     # $t6 = reg[12]
        lw      $15,52($27)     # $t7 = reg[13]
        lw      $16,56($27)     # $s0 = reg[14]
        lw      $17,60($27)     # $s1 = reg[15]
        lw      $18,64($27)     # $s2 = reg[16]
        lw      $19,68($27)     # $s3 = reg[17]
        lw      $20,72($27)     # $s4 = reg[18]
        lw      $21,76($27)     # $s5 = reg[19]
        lw      $22,80($27)     # $s6 = reg[20]
        lw      $23,84($27)     # $s7 = reg[21]
        lw      $24,88($27)     # $t8 = reg[22]
        lw      $25,92($27)     # $t9 = reg[23]
#       lw      $28,96($27)     # $gp - GP is never saved or restored
        lw      $29,100($27)    # $sp = reg[25]
        lw      $30,104($27)    # $s8 = reg[26]
        lw      $31,108($27)    # $ra = reg[27]
        lw      $26,112($27)    # Move the content of reg[28] (hi) to $26
        mthi    $26             # hi = reg[28]
        lw      $26,116($27)    # Move the content of reg[29] (lo) to $26
        mtlo    $26             # hi = reg[29]

        lw      $26,120($27)    # pc = reg[30]
        
        # Load TCB.offet to $27 to used this value to set the page in mtc0    $27,$10
        lw      $27,124($27)    # $27 = TCB.offset
        li      $1,0x1          # $1 = 1 - used in sequence

        mtc0    $27,$10         # Set the page register on MIPS that is mapped as a co-processor $10
        jr      $26             # jumps to pc
        mtc0    $1,$12          # Enables interrupts using $1 value (1==TRUE)
        
        .set    reorder
        .end    HAL_run_scheduled_task
        
###################################################

        .globl  HAL_set_interrupt_enabled
        .ent    HAL_set_interrupt_enabled

# Enable or disable interruption accord to value of $a0 - argument 0 register that is represented by enable input
HAL_set_interrupt_enabled:
        .set    noreorder
        mfc0    $2,$12          # There is no reason to this instruction be there since it does't nothing usefull
        jr      $31
        mtc0    $4,$12          # Set co-processor $12 accoriding the value of $a0 - enabling/disabling interrupts

        .set    reorder
        .end    HAL_set_interrupt_enabled

