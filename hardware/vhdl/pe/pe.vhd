------------------------------------------------------------------------------------------------
-- Memphis Processing Element
------------------------------------------------------------------------------------------------
library ieee;
use work.mlite_pack.all;                
use work.standards.all;
use work.memphis_pkg.all;
use ieee.std_logic_1164.all;
use ieee.std_logic_misc.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_textio.all;
use ieee.std_logic_unsigned.all;
use ieee.math_real.all;

use std.textio.all;
library unisim;
use unisim.vcomponents.all;

entity pe is
    generic 
    (
        log_file            : string := "output.txt";
        router_address      : adress_width:= (others=>'0');
        kernel_type			: kernel_str
    );
    port 
    (  
    	clock                   : in  std_logic;
        reset                   : in  std_logic;
        
        --CS NoCs
        rx_cs                   : in  array_subnet_wire_external;
        data_in_cs              : in  array_subnet_data_external;
        credit_o_cs             : out array_subnet_wire_external;
        req_out					: out array_subnet_wire_external;
        tx_cs                   : out array_subnet_wire_external;
        data_out_cs             : out array_subnet_data_external;
        credit_i_cs             : in  array_subnet_wire_external;
        req_in					: in  array_subnet_wire_external;
        
		-- PS NoC
        rx_ps                   : in  std_logic_vector(3 downto 0);
        data_in_ps              : in  arrayNPORT_1_regflit;
        credit_o_ps             : out std_logic_vector(3 downto 0);
        tx_ps                   : out std_logic_vector(3 downto 0);
        data_out_ps             : out arrayNPORT_1_regflit;
        credit_i_ps             : in  std_logic_vector(3 downto 0)
        
    );
end entity pe;

architecture structural of pe is
	
-----------------------------------------------------------------------------------
-- INTERNAL NOC SIGNALS
-----------------------------------------------------------------------------------
	-- Circuit-Switching Multi-NoC interface
	-- NoC CS Interface
	signal tx_noc_cs       		: array_subnet_wire;
	signal data_out_noc_cs 		: array_subnet_data;
	signal credit_i_noc_cs 		: array_subnet_wire;
	signal rx_noc_cs       		: array_subnet_wire;
	signal data_in_noc_cs  		: array_subnet_data;
	signal credit_o_noc_cs 		: array_subnet_wire;
	signal req_in_noc   		: array_subnet_wire;
	signal req_out_noc  		: array_subnet_wire;
	
	-- PE Local CS interface
	signal rx_dmni_cs        	: array_subnet_wire_pe;
	signal data_in_dmni_cs   	: array_subnet_data_pe;
	signal credit_o_dmni_cs     : array_subnet_wire_pe;
	signal tx_dmni_cs           : array_subnet_wire_pe;
	signal data_out_dmni_cs     : array_subnet_data_pe;
	signal credit_i_dmni_cs     : array_subnet_wire_pe;
	signal req_out_pe      		: array_subnet_wire_pe;
	signal req_in_pe       		: array_subnet_wire_pe;
	
	-- Packet Switching interface
	-- NoC PS Interface
    signal rx_noc_ps           	: regNport;
    signal data_in_noc_ps      	: arrayNport_regflit;
    signal credit_o_noc_ps     	: regNport;
    signal tx_noc_ps           	: regNport;
    signal data_out_noc_ps     	: arrayNport_regflit;
    signal credit_i_noc_ps     	: regNport;
    -- PE Local PS interface
    signal tx_dmni_ps           : std_logic;
    signal data_out_dmni_ps     : regflit;
    signal credit_i_dmni_ps     : std_logic;
    signal rx_dmni_ps           : std_logic;
    signal data_in_dmni_ps      : regflit;
    signal credit_o_dmni_ps     : std_logic;

	
-----------------------------------------------------------------------------------
-- INTERNAL CPU SIGNALS
-----------------------------------------------------------------------------------	
    signal cpu_mem_address_reg           : std_logic_vector(31 downto 0);
    signal cpu_mem_data_write_reg        : std_logic_vector(31 downto 0);
    signal cpu_mem_write_byte_enable_reg : std_logic_vector(3 downto 0);
    signal irq_mask_reg                  : std_logic_vector(31 downto 0);
    signal irq_status                    : std_logic_vector(31 downto 0);
    signal irq                           : std_logic;
    signal time_slice                    : std_logic_vector(31 downto 0);
    signal write_enable                  : std_logic;
    signal tick_counter_local            : std_logic_vector(31 downto 0);
    signal tick_counter                  : std_logic_vector(31 downto 0);
    signal current_page                  : std_logic_vector(7 downto 0);
    signal cpu_mem_address               : std_logic_vector(31 downto 0);
    signal cpu_mem_data_write            : std_logic_vector(31 downto 0);
    signal cpu_mem_data_read             : std_logic_vector(31 downto 0);
    signal cpu_mem_write_byte_enable     : std_logic_vector(3 downto 0);
    signal cpu_mem_pause                 : std_logic;
    signal cpu_enable_ram                : std_logic;
    
    signal clock_aux                     : std_logic;
    signal clock_hold_s                  : std_logic;
    signal pending_service               : std_logic;
    
    
    -- Routing config
    signal config_r_cpu_inport  		 : std_logic_vector(2 downto 0);   
    signal config_r_cpu_outport  		 : std_logic_vector(2 downto 0);  
    signal config_r_cpu_valid  		   	 : array_subnet_wire_pe;
    
    --DMNI config
    signal cpu_valid_dmni              	 : std_logic;
    signal cpu_code_dmni   				 : std_logic_vector(2 downto 0);
    signal dmni_intr_subnet  			 : array_total_subnet_wire;
    signal dmni_send_active_sig        	 : array_total_subnet_wire;
    signal dmni_receive_active_sig    	 : array_total_subnet_wire;
    
    
    signal data_read_ram                 : std_logic_vector(31 downto 0);
    signal mem_data_read                 : std_logic_vector(31 downto 0);
    signal dmni_mem_address              : std_logic_vector(31 downto 0);
    signal dmni_mem_write_byte_enable    : std_logic_vector(3 downto 0);
    signal dmni_mem_data_write           : std_logic_vector(31 downto 0);
    signal dmni_mem_data_read            : std_logic_vector(31 downto 0);
    signal dmni_enable_internal_ram      : std_logic;
    signal addr_a                        : std_logic_vector(31 downto 2);
    signal addr_b                        : std_logic_vector(31 downto 2);
    signal data_av                       : std_logic;
    signal end_sim_reg                   : std_logic_vector(31 downto 0);
    signal uart_write_data               : std_logic;
    
    signal slack_update_timer			 : std_logic_vector(31 downto 0);
    signal req_in_reg       			 : array_subnet_wire_pe;
    signal handle_req       			 : array_subnet_wire_pe;

    signal config_en, config_wait_header, dmni_rec_en  : std_logic;
    signal config_inport_subconfig, config_outport_subconfig : std_logic_vector (2 downto 0);
    signal config_valid_subconfig : array_subnet_wire_pe;
begin
	
-----------------------------------------------------------------------------------
-- PE COMPONENTS INSTANTIATION
-----------------------------------------------------------------------------------
	cpu : entity work.mlite_cpu
		port map(
			clk          => clock_hold_s,
			reset_in     => reset,
			intr_in      => irq,
			mem_address  => cpu_mem_address,
			mem_data_w   => cpu_mem_data_write,
			mem_data_r   => cpu_mem_data_read,
			mem_byte_we  => cpu_mem_write_byte_enable,
			mem_pause    => cpu_mem_pause,
			current_page => current_page
		);


master: if kernel_type = "mas" generate
	mem : entity work.ram_master
		port map(
			clk          => clock,
			enable_a     => cpu_enable_ram,
			wbe_a        => cpu_mem_write_byte_enable,
			address_a    => addr_a,
			data_write_a => cpu_mem_data_write,
			data_read_a  => data_read_ram,
			enable_b     => dmni_enable_internal_ram,
			wbe_b        => dmni_mem_write_byte_enable,
			address_b    => addr_b,
			data_write_b => dmni_mem_data_write,
			data_read_b  => mem_data_read
		);
end	generate;
		
slave: if kernel_type = "sla" generate
	mem : entity work.ram_slave
		port map(
			clk          => clock,
			enable_a     => cpu_enable_ram,
			wbe_a        => cpu_mem_write_byte_enable,
			address_a    => addr_a,
			data_write_a => cpu_mem_data_write,
			data_read_a  => data_read_ram,
			enable_b     => dmni_enable_internal_ram,
			wbe_b        => dmni_mem_write_byte_enable,
			address_b    => addr_b,
			data_write_b => dmni_mem_data_write,
			data_read_b  => mem_data_read
		);
end	generate;
	
	PS_router : entity work.RouterCC
		generic map(address => router_address)
		port map(
			clock    => clock,
			reset    => reset,
			rx       => rx_noc_ps,
			data_in  => data_in_noc_ps,
			credit_o => credit_o_noc_ps,
			tx       => tx_noc_ps,
			data_out => data_out_noc_ps,
			credit_i => credit_i_noc_ps
		);

	CS_router : for i in 0 to CS_SUBNETS_NUMBER - 1 generate
		router : entity work.CS_router
		port map(
			clock          => clock,
			reset          => reset,
			rx             => rx_noc_cs(i),
			data_in        => data_in_noc_cs(i),
			credit_out     => credit_o_noc_cs(i),
			req_out        => req_out_noc(i),
			tx             => tx_noc_cs(i),
			data_out       => data_out_noc_cs(i),
			credit_in      => credit_i_noc_cs(i),
			req_in         => req_in_noc(i),
			
			config_inport  => config_r_cpu_inport,
			config_outport => config_r_cpu_outport,
			config_valid   => config_r_cpu_valid(i)
		);
	end generate CS_router;

   CS_config : entity work.CS_config
      port map(
         clock          => clock,
         reset          => reset,
         rx             => tx_noc_ps(LOCAL),
         data_in        => data_in_dmni_ps,
         credit_o       => credit_o_dmni_ps,

         config_inport  => config_inport_subconfig,
         config_outport => config_outport_subconfig,
         config_valid   => config_valid_subconfig,

         wait_header    => config_wait_header,
         config_en      => config_en
      );

    dmni_qos : entity work.dmni_qos
    	generic map(
    		address_router => router_address
    	)
    	port map(
    		clock          => clock,
    		reset          => reset,
    		--Configuration interface
    		config_valid	=> cpu_valid_dmni,
	        config_code		=> cpu_code_dmni,
	        config_data     => cpu_mem_data_write_reg,
	
	        -- Status outputs
	        intr_subnet		=> dmni_intr_subnet,
	        send_active     => dmni_send_active_sig,
	        receive_active  => dmni_receive_active_sig,    
	        
	        -- Memory interface
	        mem_address     => dmni_mem_address, 
	        mem_data_write  => dmni_mem_data_write,
	        mem_data_read   => dmni_mem_data_read,
	        mem_byte_we     => dmni_mem_write_byte_enable,      
	
	        --NoC CS Interface (Local port)
	        tx_cs              => tx_dmni_cs,
	        data_out_cs        => data_out_dmni_cs,
	        credit_in_cs       => credit_i_dmni_cs,
	        rx_cs              => rx_dmni_cs,
	        data_in_cs         => data_in_dmni_cs,
	        credit_out_cs      => credit_o_dmni_cs,
	        
	        --NoC PS Interface (Local port)
	        tx_ps            => tx_dmni_ps,
	        data_out_ps      => data_out_dmni_ps,
	        credit_in_ps     => credit_i_dmni_ps,
	        rx_ps            => rx_dmni_ps,
	        data_in_ps       => data_in_dmni_ps,
	        credit_out_ps    => credit_o_dmni_ps
    	);

    uart : entity work.UartFile
    	generic map(
    		log_file => log_file
    	)
    	port map(
    		reset   => reset,
    		data_av => uart_write_data,
    		data_in => cpu_mem_data_write_reg
    	);
    	
-----------------------------------------------------------------------------------
-- NETWORK WIRING
-----------------------------------------------------------------------------------

    -- CIRCUIT SWITCHING INTERCONNECTION
	noc: for i in 0 to CS_SUBNETS_NUMBER-1 generate
		--External: connecting east, weast, north and south ports to the inputs and outputs
		rx_noc_cs(i)(3 downto 0) 		<= rx_cs(i);
		data_in_noc_cs(i)(EAST) 		<= data_in_cs(i)(EAST);
		data_in_noc_cs(i)(WEST) 		<= data_in_cs(i)(WEST);
		data_in_noc_cs(i)(NORTH) 		<= data_in_cs(i)(NORTH);
		data_in_noc_cs(i)(SOUTH) 		<= data_in_cs(i)(SOUTH);
		credit_o_cs(i) 					<= credit_o_noc_cs(i)(3 downto 0);
		tx_cs(i) 						<= tx_noc_cs(i)(3 downto 0);
		data_out_cs(i)(EAST) 			<= data_out_noc_cs(i)(EAST);
		data_out_cs(i)(WEST) 			<= data_out_noc_cs(i)(WEST);
		data_out_cs(i)(NORTH) 			<= data_out_noc_cs(i)(NORTH);
		data_out_cs(i)(SOUTH) 			<= data_out_noc_cs(i)(SOUTH);
		credit_i_noc_cs(i)(3 downto 0) 	<= credit_i_cs(i);
		
		--Internal: connecting router local port to dmni
		rx_noc_cs(i)(LOCAL) 			<= tx_dmni_cs(i);
		data_in_noc_cs(i)(LOCAL) 		<= data_out_dmni_cs(i);
		credit_i_noc_cs(i)(LOCAL) 		<= credit_o_dmni_cs(i);
		rx_dmni_cs(i) 					<= tx_noc_cs(i)(LOCAL);
		data_in_dmni_cs(i) 				<= data_out_noc_cs(i)(LOCAL);
		credit_i_dmni_cs(i) 			<= credit_o_noc_cs(i)(LOCAL);
		
		-- request external
		req_in_noc(i)(3 downto 0) 		<= req_in(i);
		req_out(i) 						<= req_out_noc(i)(3 downto 0);
	
		-- request local
		req_in_pe(i) 					<= req_out_noc(i)(LOCAL);
		req_in_noc(i)(LOCAL)			<= req_out_pe(i);
			
	end generate noc;
	
	-- PACKET SWITCHING INTERCONNECTION
	-- External: connecting east, weast, north and south ports to the inputs and outputs
    rx_noc_ps(3 downto 0)               <= rx_ps;
    data_in_noc_ps(EAST)               	<= data_in_ps(EAST);
    data_in_noc_ps(WEST)                <= data_in_ps(WEST);
    data_in_noc_ps(NORTH)               <= data_in_ps(NORTH);
    data_in_noc_ps(SOUTH)               <= data_in_ps(SOUTH);
    credit_o_ps 						<= credit_o_noc_ps(3 downto 0);
    tx_ps 								<= tx_noc_ps(3 downto 0);
    data_out_ps(EAST)                	<= data_out_noc_ps(EAST);
    data_out_ps(WEST)                	<= data_out_noc_ps(WEST);
    data_out_ps(NORTH)               	<= data_out_noc_ps(NORTH);
    data_out_ps(SOUTH)               	<= data_out_noc_ps(SOUTH);
    credit_i_noc_ps(3 downto 0) 		<= credit_i_ps;

    -- Internal: connecting router local port to dmni
    									--bit 16 indicates a configuration packet
   	dmni_rec_en                        	<= not( (data_in_dmni_ps(16) and config_wait_header) or config_en);
   	rx_dmni_ps                    		<= tx_noc_ps(LOCAL) and dmni_rec_en;

	rx_noc_ps(LOCAL) 					<= tx_dmni_ps;
	credit_i_dmni_ps 					<= credit_o_noc_ps(LOCAL);
	credit_i_noc_ps(LOCAL) 			 	<= credit_o_dmni_ps;
	data_in_dmni_ps 					<= data_out_noc_ps(LOCAL);
	data_in_noc_ps(LOCAL) 				<= data_out_dmni_ps;
    
    
-----------------------------------------------------------------------------------
-- COMBINATIONAL LOGIC
-----------------------------------------------------------------------------------
    	
    -- UART 
    uart_write_data <= '1' when cpu_mem_address_reg = DEBUG and write_enable = '1' else '0';

	-- CPU data read mux
    MUX_CPU : cpu_mem_data_read <=  irq_mask_reg when cpu_mem_address_reg = IRQ_MASK else
                                    irq_status when cpu_mem_address_reg = IRQ_STATUS_ADDR else
                                    time_slice when cpu_mem_address_reg = TIME_SLICE_ADDR else
                                    ZERO(31 downto 16) & router_address when cpu_mem_address_reg = NET_ADDRESS else
                                    tick_counter when cpu_mem_address_reg = TICK_COUNTER_ADDR else                               
                                    ZERO(31 downto SUBNETS_NUMBER) & dmni_send_active_sig when cpu_mem_address_reg = DMNI_SEND_ACTIVE else                                    
                                    ZERO(31 downto SUBNETS_NUMBER) & dmni_receive_active_sig when cpu_mem_address_reg = DMNI_RECEIVE_ACTIVE else
                                    ZERO(31 downto CS_SUBNETS_NUMBER) & req_in_reg when cpu_mem_address_reg = READ_CS_REQUEST else
                                    data_read_ram;
    
    --Comb assignments
    addr_a(31 downto 28)                    <= cpu_mem_address(31 downto 28);
    addr_a(27 downto PAGE_SIZE_H_INDEX + 1) <= ZERO(27 downto PAGE_SIZE_H_INDEX + 9) & current_page when current_page /= "00000000" and cpu_mem_address(31 downto PAGE_SIZE_H_INDEX + 1) /= ZERO(31 downto PAGE_SIZE_H_INDEX + 1)
    										 	else cpu_mem_address(27 downto PAGE_SIZE_H_INDEX + 1);
    
    addr_a(PAGE_SIZE_H_INDEX downto 2) <= cpu_mem_address(PAGE_SIZE_H_INDEX downto 2);
    addr_b                  <= dmni_mem_address(31 downto 2);
    data_av                 <= '1' when cpu_mem_address_reg = DEBUG and write_enable = '1' else '0';
    cpu_mem_pause           <= '0';
    irq                     <= '1' when (irq_status and irq_mask_reg) /= 0 else '0';
    dmni_mem_data_read      <= mem_data_read;
    cpu_enable_ram          <= '1';
    dmni_enable_internal_ram <= '1';
    end_sim_reg             <= x"00000000" when cpu_mem_address_reg = END_SIM and write_enable = '1' else x"00000001";
   	write_enable 			<= '1' when cpu_mem_write_byte_enable_reg /= "0000" else '0';
   	
	--Interruption status   
	irq_status (31 downto 4)<= ZERO(31 downto (SUBNETS_NUMBER + 4)) & dmni_intr_subnet;
	irq_status(3) 			<= '1' when req_in_reg /= 0 else '0';
	irq_status(2) 			<= '1' when dmni_send_active_sig(0) = '0' and slack_update_timer = 1 else '0';
	irq_status(1) 			<= '1' when dmni_send_active_sig(0) = '0' and pending_service = '1' else '0';
    irq_status(0) 			<= '1' when time_slice = x"00000001" else '0';

  
  --Router config  
  	config_r_cpu_inport   	<= cpu_mem_data_write_reg(15 downto 13) when (cpu_mem_address_reg = CONFIG_VALID_NET) and write_enable = '1' else config_inport_subconfig;
  	config_r_cpu_outport 	<= cpu_mem_data_write_reg(12 downto 10) when (cpu_mem_address_reg = CONFIG_VALID_NET) and write_enable = '1' else config_outport_subconfig;
	config_r_cpu_valid		<= cpu_mem_data_write_reg(CS_SUBNETS_NUMBER downto 1) when cpu_mem_address_reg = CONFIG_VALID_NET and write_enable = '1' else config_valid_subconfig;

	--DMNI config
	cpu_code_dmni <= 	CODE_CS_NET 	when cpu_mem_address_reg = DMNI_CS_NET 	   	else
						CODE_MEM_ADDR 	when cpu_mem_address_reg = DMNI_MEM_ADDR  	else
						CODE_MEM_SIZE 	when cpu_mem_address_reg = DMNI_MEM_SIZE  	else
						CODE_MEM_ADDR2 	when cpu_mem_address_reg = DMNI_MEM_ADDR2 	else
						CODE_MEM_SIZE2 	when cpu_mem_address_reg = DMNI_MEM_SIZE2 	else
						CODE_OP			when cpu_mem_address_reg = DMNI_OP		   	else
						(others=> '0');
	cpu_valid_dmni <= '0' when cpu_code_dmni = 0 else '1';
	
	handle_req <= cpu_mem_data_write_reg(CS_SUBNETS_NUMBER-1 downto 0) when cpu_mem_address_reg = HANDLE_CS_REQUEST and write_enable = '1' else (others=> '0');
    req_out_pe <= cpu_mem_data_write_reg(CS_SUBNETS_NUMBER-1 downto 0) when cpu_mem_address_reg = WRITE_CS_REQUEST and write_enable = '1' else (others=> '0');

-----------------------------------------------------------------------------------
-- SYNCHRONOUS PROCESSES
-----------------------------------------------------------------------------------
	
	cs_request : process (clock, reset) is
	begin
		if reset = '1' then
			req_in_reg <= (others=> '0');
		elsif rising_edge(clock) then
			req_in_reg  <= req_in_pe or (req_in_reg and not handle_req);
		end if;
	end process cs_request;
	

    sequential_attr: process(clock, reset)
    begin            
        if reset = '1' then
            cpu_mem_address_reg <= ZERO;
            cpu_mem_data_write_reg <= ZERO;
            cpu_mem_write_byte_enable_reg <= ZERO(3 downto 0);
            irq_mask_reg <= ZERO;
            time_slice <= ZERO;
            tick_counter <= ZERO;
            pending_service <= '0';
            slack_update_timer <= ZERO;
        elsif (clock'event and clock = '1') then
            if cpu_mem_pause = '0' then
                cpu_mem_address_reg <= cpu_mem_address;
                cpu_mem_data_write_reg <= cpu_mem_data_write;
                cpu_mem_write_byte_enable_reg <= cpu_mem_write_byte_enable;
        
                if cpu_mem_address_reg = IRQ_MASK and write_enable = '1' then
                    irq_mask_reg <= cpu_mem_data_write_reg;
                end if;     
               -- Decrements the time slice when executing a task (current_page /= x"00") or handling a syscall (syscall = '1')
                if time_slice > 1 then
                    time_slice <= time_slice - 1;
                end if;  

                if(cpu_mem_address_reg = PENDING_SERVICE_INTR and write_enable = '1') then
                    if cpu_mem_data_write_reg = ZERO then
                        pending_service <= '0';
                    else
                        pending_service <= '1';
                    end if;
                end if; 
            end if;
            
            if cpu_mem_address_reg = SLACK_TIME_MONITOR and write_enable = '1' then
            	slack_update_timer <= cpu_mem_data_write_reg;
            elsif slack_update_timer > 1 then
            	slack_update_timer <= slack_update_timer - 1;
            end if;
                                    
            if cpu_mem_address_reg = TIME_SLICE_ADDR and write_enable = '1' then
                time_slice <= cpu_mem_data_write_reg;
            end if;

            tick_counter <= tick_counter + 1;   
        end if;
    end process sequential_attr;

    clock_stop: process(reset,clock)
    begin
        if(reset = '1') then
            tick_counter_local <= (others=> '0');
            clock_aux <= '1';
        else
            if cpu_mem_address_reg = CLOCK_HOLD and write_enable = '1' then
                clock_aux <= '0';
            --elsif tx_router(LOCAL) = '1' or ni_intr = '1' or time_slice = x"00000001" then 
            elsif irq = '1' then 
                clock_aux <= '1';
            end if;

            if(clock_aux ='1' and clock ='1') then
                clock_hold_s <= '1';
                tick_counter_local <= tick_counter_local + 1;
            else
                clock_hold_s <= '0';
            end if;
        end if;
    end process clock_stop;

end architecture structural;

  
