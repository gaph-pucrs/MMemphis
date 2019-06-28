library IEEE;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;
use work.standards.all;

entity CS_router is
	port(
		clock			: in std_logic;
		reset   		: in std_logic;
		
		rx				: in  regNport;
        data_in			: in  arrayNport_CSregflit;
        credit_out		: out regNport;
        req_out			: out regNport;
        
        tx				: out regNport;
        data_out		: out arrayNport_CSregflit;
        credit_in		: in regNport;
        req_in			: in regNport;
        
        config_inport 	: in std_logic_vector(2 downto 0);
        config_outport 	: in std_logic_vector(2 downto 0);
        config_valid 	: in std_logic
	);
	
end;

architecture CS_router of CS_router is
	
	signal ORT, configORT : arrayNport_reg3      := (others => "101"); -- clear
	signal IRT, configIRT : arrayNport_reg3      := (others => "101"); -- clear
	signal input_reg      : arrayNport_CSregflit := (others => (others => '0'));
	signal data_av        : regNport             := (others => '0');
	signal credit_reg     : regNport             := (others => '0');
	signal req_reg        : regNport             := (others => '0');
	signal setORT, setIRT : regNport             := (others => '0');
begin	
-- Local	
        tx(LOCAL) <= 		data_av(EAST)  when ORT(LOCAL)="000" else
	                        data_av(WEST)  when ORT(LOCAL)="001" else
	                        data_av(NORTH) when ORT(LOCAL)="010" else
	                        data_av(SOUTH) when ORT(LOCAL)="011" else
	                        '0';

        data_out(LOCAL) <= 	input_reg(EAST)  when ORT(LOCAL)="000" else
                        	input_reg(WEST)  when ORT(LOCAL)="001" else
                        	input_reg(NORTH) when ORT(LOCAL)="010" else
                        	input_reg(SOUTH);
        
        credit_reg(LOCAL) <=credit_in(EAST)  when IRT(LOCAL)="000" else
        					credit_in(WEST)  when IRT(LOCAL)="001" else
        					credit_in(NORTH) when IRT(LOCAL)="010" else
        					credit_in(SOUTH) when IRT(LOCAL)="011" else
	                        '0';
	                        
        req_out(LOCAL) <=   req_reg(EAST)  when IRT(LOCAL)="000" else
        					req_reg(WEST)  when IRT(LOCAL)="001" else
        					req_reg(NORTH) when IRT(LOCAL)="010" else
        					req_reg(SOUTH) when IRT(LOCAL)="011" else
	                        '0';
        
-- East	
        tx(EAST) <= 		data_av(WEST)  when ORT(EAST)="001" else
	                        data_av(NORTH) when ORT(EAST)="010" else
	                        data_av(SOUTH) when ORT(EAST)="011" else
	                        data_av(LOCAL) when ORT(EAST)="100" else
	                        '0';

        data_out(EAST) <= 	input_reg(WEST)  when ORT(EAST)="001" else
                        	input_reg(NORTH) when ORT(EAST)="010" else
                        	input_reg(SOUTH) when ORT(EAST)="011" else
                        	input_reg(LOCAL);
                        	
    	credit_reg(EAST) <=	credit_in(WEST)  when IRT(EAST)="001" else
        					credit_in(NORTH) when IRT(EAST)="010" else
        					credit_in(SOUTH) when IRT(EAST)="011" else
        					credit_in(LOCAL) when IRT(EAST)="100" else
	                        '0';
	                        
	    req_out(EAST) <=	req_reg(WEST)  when IRT(EAST)="001" else
        					req_reg(NORTH) when IRT(EAST)="010" else
        					req_reg(SOUTH) when IRT(EAST)="011" else
        					req_reg(LOCAL) when IRT(EAST)="100" else
	                        '0';
-- West	
        tx(WEST) <= 		data_av(EAST)  when ORT(WEST)="000" else
	                        data_av(NORTH) when ORT(WEST)="010" else
	                        data_av(SOUTH) when ORT(WEST)="011" else
	                        data_av(LOCAL) when ORT(WEST)="100" else
	                        '0';

        data_out(WEST) <= 	input_reg(EAST)  when ORT(WEST)="000" else
                        	input_reg(NORTH) when ORT(WEST)="010" else
                        	input_reg(SOUTH) when ORT(WEST)="011" else
                        	input_reg(LOCAL);
                        	
    	credit_reg(WEST) <=	credit_in(EAST)  when IRT(WEST)="000" else
        					credit_in(NORTH) when IRT(WEST)="010" else
        					credit_in(SOUTH) when IRT(WEST)="011" else
        					credit_in(LOCAL) when IRT(WEST)="100" else
	                        '0';
	                        
	    req_out(WEST) <=	req_reg(EAST)  when IRT(WEST)="000" else
        					req_reg(NORTH) when IRT(WEST)="010" else
        					req_reg(SOUTH) when IRT(WEST)="011" else
        					req_reg(LOCAL) when IRT(WEST)="100" else
	                        '0';
-- North	
        tx(NORTH) <= 		data_av(EAST)  when ORT(NORTH)="000" else
	                        data_av(WEST)  when ORT(NORTH)="001" else
	                        data_av(SOUTH) when ORT(NORTH)="011" else
	                        data_av(LOCAL) when ORT(NORTH)="100" else
	                        '0';

        data_out(NORTH) <= 	input_reg(EAST)  when ORT(NORTH)="000" else
                        	input_reg(WEST)  when ORT(NORTH)="001" else
                        	input_reg(SOUTH) when ORT(NORTH)="011" else
                        	input_reg(LOCAL);
                        	
    	credit_reg(NORTH) <=credit_in(EAST)  when IRT(NORTH)="000" else
        					credit_in(WEST)  when IRT(NORTH)="001" else
        					credit_in(SOUTH) when IRT(NORTH)="011" else
        					credit_in(LOCAL) when IRT(NORTH)="100" else
	                        '0';
	                        
	    req_out(NORTH) <=   req_reg(EAST)  when IRT(NORTH)="000" else
        					req_reg(WEST)  when IRT(NORTH)="001" else
        					req_reg(SOUTH) when IRT(NORTH)="011" else
        					req_reg(LOCAL) when IRT(NORTH)="100" else
	                        '0';
-- Local	
        tx(SOUTH) <= 		data_av(EAST)  when ORT(SOUTH)="000" else
	                        data_av(WEST)  when ORT(SOUTH)="001" else
	                        data_av(NORTH) when ORT(SOUTH)="010" else
	                        data_av(LOCAL) when ORT(SOUTH)="100" else
	                        '0';

        data_out(SOUTH) <= 	input_reg(EAST)  when ORT(SOUTH)="000" else
                        	input_reg(WEST)  when ORT(SOUTH)="001" else
                        	input_reg(NORTH) when ORT(SOUTH)="010" else
                        	input_reg(LOCAL);
                        	
    	credit_reg(SOUTH) <=credit_in(EAST)  when IRT(SOUTH)="000" else
        					credit_in(WEST)  when IRT(SOUTH)="001" else
        					credit_in(NORTH) when IRT(SOUTH)="010" else
        					credit_in(LOCAL) when IRT(SOUTH)="100" else
	                        '0';
	                        
	    req_out(SOUTH) <=	req_reg(EAST)  when IRT(SOUTH)="000" else
        					req_reg(WEST)  when IRT(SOUTH)="001" else
        					req_reg(NORTH) when IRT(SOUTH)="010" else
        					req_reg(LOCAL) when IRT(SOUTH)="100" else
	                        '0';
               
   lookup_enable : for i in 0 to NPORT-1 generate
      
      configORT(i)	<= config_inport 	when i = CONV_INTEGER(config_outport) and reset = '0' else "101"; -- define se o valor de ORT(i) deve receber config_inport ou "101"
      configIRT(i) 	<= config_outport 	when i = CONV_INTEGER(config_inport)  and reset = '0' else "101"; -- define se o valor de IRT(i) deve receber config_inport ou "101"
      
      -- Enables the writing at i index of ORT
      setORT(i) <= '1' when reset = '1' or (config_valid = '1' and (i = CONV_INTEGER(config_outport) or ORT(i) = config_inport)) else '0';
      
      -- Enables the writing at i index of IRT
      setIRT(i) <= '1' when reset = '1' or (config_valid = '1' and (i = CONV_INTEGER(config_inport) or  IRT(i) = config_outport)) else '0';         
        
   end generate lookup_enable;      
         
-- Configuration control
	lookup_control: process(clock, reset)
	begin
		if rising_edge(clock) then
			
            if setORT(EAST) = '1' then
               ORT(EAST) <= configORT(EAST) ;
            end if;
            if setORT(WEST) = '1' then
               ORT(WEST) <= configORT(WEST) ;
            end if;
            if setORT(NORTH) = '1' then
               ORT(NORTH) <= configORT(NORTH) ;
            end if;
            if setORT(SOUTH) = '1' then
               ORT(SOUTH) <= configORT(SOUTH) ;
            end if;
            if setORT(LOCAL) = '1' then
               ORT(LOCAL) <= configORT(LOCAL) ;
            end if;
            
            if setIRT(EAST) = '1' then
               IRT(EAST) <= configIRT(EAST);
            end if;
            if setIRT(WEST) = '1' then
               IRT(WEST) <= configIRT(WEST);
            end if;
            if setIRT(NORTH) = '1' then
               IRT(NORTH) <= configIRT(NORTH);
            end if;
            if setIRT(SOUTH) = '1' then
               IRT(SOUTH) <= configIRT(SOUTH);
            end if;
            if setIRT(LOCAL) = '1' then
               IRT(LOCAL) <= configIRT(LOCAL);
            end if;
            
		end if;
	end process lookup_control;
	
-- Request switch assingment
	request: process(clock, reset)
	begin
		if reset = '1' then
			req_reg <= (others=> '0');
		elsif rising_edge(clock) then
			req_reg(LOCAL)	<= req_in(LOCAL);
			req_reg(EAST) 	<= req_in(EAST);
			req_reg(WEST) 	<= req_in(WEST);
			req_reg(NORTH) 	<= req_in(NORTH);
			req_reg(SOUTH) 	<= req_in(SOUTH);
		end if;
	end process request;                        
	                
	input_LOCAL: entity work.EB_input
		port map(
			clock      => clock,
			reset      => reset,
			valid_in   => rx(LOCAL),
			data_in    => data_in(LOCAL),
			ready_in   => credit_out(LOCAL),
			valid_out  => data_av(LOCAL),
			data_out   => input_reg(LOCAL),
			ready_out  => credit_reg(LOCAL)
		);
		
	input_WEST: entity work.EB_input
		port map(
			clock      => clock,
			reset      => reset,
			valid_in   => rx(WEST),
			data_in    => data_in(WEST),
			ready_in   => credit_out(WEST),
			valid_out  => data_av(WEST),
			data_out   => input_reg(WEST),
			ready_out  => credit_reg(WEST)
		);  
		
		
	input_EAST: entity work.EB_input
		port map(
			clock      => clock,
			reset      => reset,
			valid_in   => rx(EAST),
			data_in    => data_in(EAST),
			ready_in   => credit_out(EAST),
			valid_out  => data_av(EAST),
			data_out   => input_reg(EAST),
			ready_out  => credit_reg(EAST)
		); 
		
	input_NORTH: entity work.EB_input
		port map(
			clock      => clock,
			reset      => reset,
			valid_in   => rx(NORTH),
			data_in    => data_in(NORTH),
			ready_in   => credit_out(NORTH),
			valid_out  => data_av(NORTH),
			data_out   => input_reg(NORTH),
			ready_out  => credit_reg(NORTH)
		);   
		
	input_SOUTH: entity work.EB_input
		port map(
			clock      => clock,
			reset      => reset,
			valid_in   => rx(SOUTH),
			data_in    => data_in(SOUTH),
			ready_in   => credit_out(SOUTH),
			valid_out  => data_av(SOUTH),
			data_out   => input_reg(SOUTH),
			ready_out  => credit_reg(SOUTH)
		);               
 

end CS_router;