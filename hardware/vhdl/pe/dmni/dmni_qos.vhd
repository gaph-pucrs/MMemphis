library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;
use work.standards.all;
use work.memphis_pkg.all;

entity dmni_qos is
    generic(address_router: adress_width := (others=>'0'));
    port
    (  
        clock          	: in  std_logic;
        reset          	: in  std_logic;
        
         --Configuration interface
        config_valid	: in  std_logic;
        config_code 	: in  std_logic_vector(2 downto 0);
        config_data    	: in  std_logic_vector(31 downto 0);
        
         --Status interface
        intr_subnet 	: out  std_logic_vector((SUBNETS_NUMBER-1) downto 0);
        send_active 	: out  std_logic_vector((SUBNETS_NUMBER-1) downto 0);
     	receive_active  : out  std_logic_vector((SUBNETS_NUMBER-1) downto 0);
     	
         --Memory interface
        mem_address    : out std_logic_vector(31 downto 0);
        mem_data_write : out std_logic_vector(31 downto 0);
        mem_data_read  : in  std_logic_vector(31 downto 0);
        mem_byte_we    : out std_logic_vector(3 downto 0);
        
         --Noc CS Interface (Local port)
        tx_cs              : out  std_logic_vector((CS_SUBNETS_NUMBER-1) downto 0);
        data_out_cs        : out  array_subnet_data_pe;
        credit_in_cs       : in   std_logic_vector((CS_SUBNETS_NUMBER-1) downto 0);
        rx_cs              : in   std_logic_vector((CS_SUBNETS_NUMBER-1) downto 0);
        data_in_cs         : in   array_subnet_data_pe;
        credit_out_cs      : out  std_logic_vector((CS_SUBNETS_NUMBER-1) downto 0);
        
        --Noc PS Interface (Local port)
        tx_ps              : out  std_logic;
        data_out_ps        : out  regflit;
        credit_in_ps       : in   std_logic;
        rx_ps              : in   std_logic;
        data_in_ps         : in   regflit;
        credit_out_ps      : out  std_logic;
        
		--SDN configuration interface
		sdn_config_inport  : out std_logic_vector(2 downto 0);
		sdn_config_outport : out std_logic_vector(2 downto 0);
		sdn_config_valid   : out std_logic_vector(CS_SUBNETS_NUMBER - 1 downto 0)
       
    );  
end;

architecture dmni_qos of dmni_qos is
	
	type array_mem_data is array((SUBNETS_NUMBER-1) downto 0) of std_logic_vector(31 downto 0);
    constant WORD_SIZE: std_logic_vector(4 downto 0):="00100";
   
	--send - serializer
	signal busy: std_logic_vector((SUBNETS_NUMBER-1) downto 0);
	--receive - serializer
	signal valid_receive: std_logic_vector((SUBNETS_NUMBER-1) downto 0);
	signal data_to_write: array_mem_data;
	--config registers
	signal s_mem_address_reg, r_mem_address_reg, s_mem_size_reg, r_mem_size_reg : array_mem_data;
	signal s_ready,r_ready : std_logic_vector((SUBNETS_NUMBER-1) downto 0); -- TASK_PER_PE downto 0 PORQUE e TASK_PER_PE + 1 (KERNEL) 
	
	--used only for subnet 0 - Packet Switching
	signal mem_address2, mem_size2 : std_logic_vector(31 downto 0);
	
	--Auxiliary
	signal code_config, load_mem :std_logic;
	signal cs_net_config : integer range 0 to SUBNETS_NUMBER-1;
	
	 -- combinational TDM wheel control
	signal s_next_count, r_next_count : integer range 0 to SUBNETS_NUMBER-1;
	signal s_curr, s_next, r_curr : integer range 0 to SUBNETS_NUMBER-1;
	signal s_valid, r_valid, s_wheel, r_wheel: std_logic_vector((SUBNETS_NUMBER-1) downto 0);
	
	--Send and Receive Arbiter
	signal dmni_mode :std_logic; -- 0 send     1 receive
	signal timer  : std_logic_vector(3 downto 0);
	
begin
	
	noc_injector_gen: for i in 0 to SUBNETS_NUMBER-1 generate
		
		CS_nets: if i < CS_SUBNETS_NUMBER generate
		
			noc_CS_sender : entity work.noc_cs_sender
				port map(
					clock            => clock,
					reset            => reset,
					data_from_memory => mem_data_read,
					valid            => s_valid(i),
					busy             => busy(i),
					tx               => tx_cs(i),
					data_out         => data_out_cs(i),
					credit_in        => credit_in_cs(i)
				);
				
			noc_CS_receiver: entity work.noc_cs_receiver
				port map(
					clock          => clock,
					reset          => reset,
					data_to_memory => data_to_write(i),
					valid          => valid_receive(i),
					consume        => r_valid(i),
					rx			   => rx_cs(i),
					data_in		   => data_in_cs(i),
					credit_out	   => credit_out_cs(i)
				);
				
		end generate CS_nets;
		
		PS_net: if i = CS_SUBNETS_NUMBER generate
			
			noc_PS_sender : entity work.noc_ps_sender
			port map(
				clock            => clock,
				reset            => reset,
				data_from_memory => mem_data_read,
				valid            => s_valid(i),
				busy             => busy(i),
				tx               => tx_ps,
				data_out         => data_out_ps,
				credit_in        => credit_in_ps
			);
		
	
			noc_PS_receiver: entity work.noc_ps_receiver
			port map(
				clock          => clock,
				reset          => reset,
				data_to_memory => data_to_write(i),
				valid          => valid_receive(i),
				consume        => r_valid(i),
				rx			   => rx_ps,
				data_in		   => data_in_ps,
				credit_out	   => credit_out_ps,
				
				--SDN configuration interface
				config_inport  => sdn_config_inport,
				config_outport => sdn_config_outport,
				config_valid   => sdn_config_valid
			);
			
		end generate PS_net;

	end generate noc_injector_gen;
	
	s_valid <= s_wheel and s_ready and (not busy); --enable to send
	r_valid <= r_wheel and r_ready and valid_receive; -- enable to receive
	
	send_active <= s_ready;
	receive_active <= r_ready;
	
	--Memory
	mem_data_write <= data_to_write(r_curr) when r_valid(r_curr) = '1';
	mem_byte_we <= "1111" when r_valid(r_curr) = '1' else "0000";
	mem_address <= s_mem_address_reg(s_next) when dmni_mode = '0' else  r_mem_address_reg(r_curr) when dmni_mode = '1';
	
	--Interruption
	intr_subnet <= (not r_ready) and valid_receive; 
	
	bitwise_update: for i in 0 to SUBNETS_NUMBER-1 generate
		--Ready update
		s_ready(i) <= '0' when s_mem_size_reg(i) = 0 else '1';
		r_ready(i) <= '0' when r_mem_size_reg(i) = 0 else '1';
		
		--TDM update
		s_wheel(i) <= '1' when s_curr = i and dmni_mode = '0' and load_mem = '0' else '0';
		r_wheel(i) <= '1' when r_curr = i and dmni_mode = '1' else '0';
	end generate bitwise_update;
	

----------- Dynamic TDM implementation for 3 subnets ---------------------------------
TDM_mux3: if SUBNETS_NUMBER = 3 generate
	s_next_count <= 2 when s_ready((s_next + 2) mod SUBNETS_NUMBER) = '1' else		
				    1;																
																						    																--
	r_next_count <= 1 when r_ready((r_curr + 1) mod SUBNETS_NUMBER) = '1' else		
				    2 when r_ready((r_curr + 2) mod SUBNETS_NUMBER) = '1' else		
				    0;		
end generate TDM_mux3;			    														
--------------------------------------------------------------------------------------
	
----------- Dynamic TDM implementation for 4 subnets ---------------------------------
TDM_mux4: if SUBNETS_NUMBER = 4 generate
	s_next_count <= 2 when s_ready((s_next + 2) mod SUBNETS_NUMBER) = '1' else		
				    3 when s_ready((s_next + 3) mod SUBNETS_NUMBER) = '1' else		
				    1;																
																					
				    																
	r_next_count <= 1 when r_ready((r_curr + 1) mod SUBNETS_NUMBER) = '1' else		
				    2 when r_ready((r_curr + 2) mod SUBNETS_NUMBER) = '1' else		
				    3 when r_ready((r_curr + 3) mod SUBNETS_NUMBER) = '1' else		
				    0;																
end generate TDM_mux4;		
--------------------------------------------------------------------------------------

----------- Dynamic TDM implementation for 6 subnets ---------------------------------
TDM_mux6: if SUBNETS_NUMBER = 6 generate
	s_next_count <= 2 when s_ready((s_next + 2) mod SUBNETS_NUMBER) = '1' else		
				    3 when s_ready((s_next + 3) mod SUBNETS_NUMBER) = '1' else		
				    4 when s_ready((s_next + 4) mod SUBNETS_NUMBER) = '1' else		
				    5 when s_ready((s_next + 5) mod SUBNETS_NUMBER) = '1' else		
				    1;																
																					
				    																
	r_next_count <= 1 when r_ready((r_curr + 1) mod SUBNETS_NUMBER) = '1' else		
				    2 when r_ready((r_curr + 2) mod SUBNETS_NUMBER) = '1' else		
				    3 when r_ready((r_curr + 3) mod SUBNETS_NUMBER) = '1' else		
				    4 when r_ready((r_curr + 4) mod SUBNETS_NUMBER) = '1' else		
				    5 when r_ready((r_curr + 5) mod SUBNETS_NUMBER) = '1' else		
				    0;	
end generate TDM_mux6;				    															
--------------------------------------------------------------------------------------
	
	--This process is divided into the PE configuration FSM, and the definition to update the size and address of the send and receive signals
	andress_and_size_update: process(clock, reset)
	begin
		if reset = '1' then
			cs_net_config <= 0;
			s_mem_address_reg <= (others=> (others=> '0'));
			r_mem_address_reg <= (others=> (others=> '0'));
			s_mem_size_reg <= (others=> (others=> '0'));
			r_mem_size_reg <= (others=> (others=> '0'));
			mem_address2 <= (others=> '0');
			mem_size2 <= (others=> '0');
		elsif rising_edge(clock) then
			
			--Config valid is a signal from PE, when is 1 means the the FSM can execute to decode the config_code
			--and gets the config_data to assigns to the apropriated config signals.
			if config_valid = '1' then
				
				case config_code is
				
				--Defines which subnoc the configuration will set
				when CODE_CS_NET => 
					cs_net_config <= conv_integer(config_data(SUBNETS_NUMBER-1 downto 0));		
				
				--Set the DMNI programming mode, receive = (1) or send = (0) 
				when CODE_OP => -- 4 - op
					code_config <= config_data(0);
				
				--Memory size and address 2 are present to suport the memory jump feature for PS subnoc
				when CODE_MEM_ADDR2 => 
					mem_address2 <= config_data;
				when CODE_MEM_SIZE2 =>
					mem_size2 <= config_data;
					
				when CODE_MEM_ADDR =>
					--When code_config is 1 the receive process receive the address data, when 0, the send receives the data
					if code_config = '1' then
						r_mem_address_reg(cs_net_config) <= config_data;
					else
						s_mem_address_reg(cs_net_config) <= config_data;
					end if;
					
				when CODE_MEM_SIZE =>
					
					--When code_config is 1 the receive process receive the size data, when 0, the send receives the data
					if code_config = '1' then
						r_mem_size_reg(cs_net_config) <= config_data;
					else
						s_mem_size_reg(cs_net_config) <= config_data;
					end if;
					
				when others =>
						
				end case;
			end if;
			
			--Send address and size update, s_valid positive means the the subnoc have data to transmitt yet
			if s_valid(s_curr) = '1' then
				--If the send memory size becomes zero, them the packet is finished
				if s_mem_size_reg(s_curr) = 1 then
					--Test if the PS subnoc have a second send and address values to perfoms the memory jump
					if s_curr = PS_NET_INDEX and mem_size2 > 0 then
						s_mem_address_reg(s_curr) <= mem_address2;
						s_mem_size_reg(s_curr) <= mem_size2;
						mem_address2 <= (others=> '0');
						mem_size2 <= (others=> '0');
					else --If there is no a PS noc or the PS noc not have seconds address and sizes, them the packet is finsehs and the send process is termianted for that subnoc
						s_mem_address_reg(s_curr) <= (others=> '0');
						s_mem_size_reg(s_curr) <= (others=> '0');
					end if;
				else --If the send size is not zero continue to increment the memory address and decrement the size
					s_mem_address_reg(s_curr) <= s_mem_address_reg(s_curr) + WORD_SIZE;
					s_mem_size_reg(s_curr) <= s_mem_size_reg(s_curr) - 1;
				end if;
			end if;
			
			-- Receive address and size update, r_valid positive mean that the subnoc have data to be received
			if r_valid(r_curr) = '1' then
				--Test if the size is zero, if positive, the receive alread receves all data and the receive process is termianted for that subnoc
				if r_mem_size_reg(r_curr) = 1 then
					r_mem_address_reg(r_curr) <= (others=> '0');
					r_mem_size_reg(r_curr) <= (others=> '0');
				else --If the receive address is not zero continue to incrementent the memory address and decrement the size
					r_mem_address_reg(r_curr) <= r_mem_address_reg(r_curr) + WORD_SIZE;
					r_mem_size_reg(r_curr) <= r_mem_size_reg(r_curr) - 1;
				end if;
			end if;
			
		end if;
	
	end process andress_and_size_update;
	
	--Controls the TDM between the sub-nocs by setup the s_whell signal to walkthrough the sub-nocs
	--Aditionally, setup the s_curr and s_next signals, these signals indicates the index of each subnocs and drive the memory and NoC access
	TDM_wheel : process (clock, reset) is
	begin
		if reset = '1' then
			s_curr <= 0;
			s_next <= 0;
			r_curr <= 0;
		elsif rising_edge(clock) then
			
			s_curr <= s_next;
			s_next <= (s_next + s_next_count) mod SUBNETS_NUMBER;
			
			r_curr <= (r_curr + r_next_count) mod SUBNETS_NUMBER;
			
		end if;
	end process TDM_wheel;
	
	--This arbiter interleaves the send and receive process by controling the dmni_mode signal
	arbiter : process (clock, reset) is
	begin
		if reset = '1' then
			dmni_mode <= '0'; --init in send mode
			timer <= (others => '0');
			load_mem <= '0';
		elsif rising_edge(clock) then
			timer <= timer + 1;
			
			if dmni_mode = '0' then
				load_mem <= '0';
			else
				load_mem <= '1';
			end if;
			
			if timer = 0 then
				
				if s_ready /= 0 and r_ready /= 0 then
					dmni_mode <= not dmni_mode;
				elsif r_ready /= 0 then
					dmni_mode <= '1';
				else
					dmni_mode <= '0';
				end if;
				
			end if;
		end if;
	end process arbiter;
	
     
end dmni_qos;
