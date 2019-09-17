## HEMPS VERSION - 8.0 - support for RT applications
##
## Distribution:  June 2016
##
## Created by: Marcelo Ruaro - contact: marcelo.ruaro@acad.pucrs.br
##
## Research group: GAPH-PUCRS   -  contact:  fernando.moraes@pucrs.br
##
## Brief description: Initializes the stack pointer and jumps to main(). Handles the syscall.



        .text
        .align  2
        .globl  entry
        .ent    entry
entry:
   .set noreorder

   li $sp,sp_addr # Initializes the Stack Pointer at upper part of task page

   jal   main	  #Jumps to main function
   nop
   
   move $4,$0     #Set the syscall code 0 (EXIT) AT $a0 register when main finishes
   syscall 		  #Set argument at return and calls syscall
   nop
   
$L1:
   j $L1
   nop

        .end entry
  
###################################################

   .globl SystemCall
   .ent SystemCall
SystemCall:
   .set	noreorder
   
   syscall 
   nop
   jr	$31
   nop
   
   .set reorder
   .end SystemCall


