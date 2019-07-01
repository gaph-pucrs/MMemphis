------------------------------------------------------------------------------------------------
--
--  DISTRIBUTED MEMPHIS  - version 5.0
--
--  Research group: GAPH-PUCRS    -    contact   fernando.moraes@pucrs.br
--
--  Distribution:  September 2013
--
--  Source name:  Memphis.vhd
--
--  Brief description:  NoC generation
--
------------------------------------------------------------------------------------------------

library IEEE;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
use work.standards.all;
use work.memphis_pkg.all;

entity Memphis is
        port(
        	clock              : in  std_logic;
        	reset              : in  std_logic;

        	-- IO interface - App Injector
        	memphis_app_injector_tx 		: out std_logic;
		 	memphis_app_injector_credit_i 	: in std_logic;
		 	memphis_app_injector_data_out 	: out regflit;
		 	
		 	memphis_app_injector_rx 		: in std_logic;
		 	memphis_app_injector_credit_o 	: out std_logic;
		 	memphis_app_injector_data_in 	: in regflit
		 	
		 	-- IO interface - Create the IO interface for your component here:
        	
        );
end;

architecture Memphis of Memphis is  

        -- Interconnection signals 
		type array_wire_top_cs is array((NUMBER_PROCESSORS-1) downto 0) of array_subnet_wire_external;
		type array_data_top_cs is array((NUMBER_PROCESSORS-1) downto 0) of array_subnet_data_external;

		type array_wire_top_ps is array((NUMBER_PROCESSORS-1) downto 0) of std_logic_vector(3 downto 0);
		type array_data_top_ps is array((NUMBER_PROCESSORS-1) downto 0) of arrayNPORT_1_regflit;
			
		-- Interconnection CS signals 
        signal tx_cs       	: array_wire_top_cs;
        signal rx_cs       	: array_wire_top_cs;
        signal req_in   	: array_wire_top_cs;
        signal req_out  	: array_wire_top_cs;
        signal credit_i_cs 	: array_wire_top_cs;
        signal credit_o_cs 	: array_wire_top_cs;
        signal data_in_cs  	: array_data_top_cs;
        signal data_out_cs 	: array_data_top_cs;
        
        -- Interconnection PS signals 
        signal tx_ps       	: array_wire_top_ps;
        signal rx_ps       	: array_wire_top_ps;
        signal credit_i_ps 	: array_wire_top_ps;
        signal credit_o_ps 	: array_wire_top_ps;
        signal data_in_ps  	: array_data_top_ps;
        signal data_out_ps 	: array_data_top_ps;
	
		signal address_router : std_logic_vector(7 downto 0);
	
		type router_position is array (NUMBER_PROCESSORS - 1 downto 0) of integer range 0 to TR;
		signal position : router_position;
		
        begin
        
        core_type_gen:   for i in 0 to NUMBER_PROCESSORS-1 generate
                position(i) <= RouterPosition(i);
        end generate core_type_gen;
        
        
        proc: for i in 0 to NUMBER_PROCESSORS-1 generate
                
                PE: entity work.pe
                generic map (
                        router_address    	=> RouterAddress(i),
                        log_file            => log_filename(i)
                        )
                port map(
                        clock           => clock,
                        reset           => reset,
                        
                        --CS port map
                        tx_cs           => tx_cs(i),
                        data_out_cs     => data_out_cs(i),
                        credit_i_cs     => credit_i_cs(i),
                        rx_cs           => rx_cs(i),
                        data_in_cs      => data_in_cs(i),
                        credit_o_cs     => credit_o_cs(i),
                        
                        req_in			=> req_in(i),
                        req_out			=> req_out(i),
                        
                        --PS port map
                        tx_ps           => tx_ps(i),
                        data_out_ps     => data_out_ps(i),
                        credit_i_ps     => credit_i_ps(i),
                        rx_ps           => rx_ps(i),
                        data_in_ps      => data_in_ps(i),
                        credit_o_ps     => credit_o_ps(i)
                );                     
                
                ----------------------------------------------------
                -- CIRCUIT SWITCHING NETWORK CONNECTION
                ----------------------------------------------------
                
				subnet: for s in 0 to CS_SUBNETS_NUMBER-1 generate
	                --- EAST PORT CONNECTIONS ----------------------------------------------------
	                east_grounding: if RouterPosition(i) = BR or RouterPosition(i) = CRX or RouterPosition(i) = TR generate
	                        rx_cs(i)(s)(EAST)           <= '0';
	                        credit_i_cs(i)(s)(EAST)     <= '0';
	                        req_in(i)(s)(EAST)       	<= '0';
	                        data_in_cs(i)(s)(EAST)      <= (others =>'0');
	                end generate;
	
	                east_connection: if RouterPosition(i) = BL or RouterPosition(i) = CL or RouterPosition(i) = TL  or RouterPosition(i) = BC or RouterPosition(i) = TC or RouterPosition(i) = CC generate
	                        rx_cs(i)(s)(EAST)             <= tx_cs(i+1)(s)(WEST);
	                        credit_i_cs(i)(s)(EAST)       <= credit_o_cs(i+1)(s)(WEST);
	                        req_in(i)(s)(EAST)       	  <= req_out(i+1)(s)(WEST);
	                        data_in_cs(i)(s)(EAST)        <= data_out_cs(i+1)(s)(WEST);
	                end generate;
	
	                --- WEST PORT CONNECTIONS ----------------------------------------------------
	                west_grounding: if RouterPosition(i) = BL or RouterPosition(i) = CL or RouterPosition(i) = TL generate
	                        rx_cs(i)(s)(WEST)           <= '0';
	                        credit_i_cs(i)(s)(WEST)     <= '0';
	                        req_in(i)(s)(WEST)       	<= '0';
	                        data_in_cs(i)(s)(WEST)      <= (others => '0');
	                end generate;
	
	                west_connection: if (RouterPosition(i) = BR or RouterPosition(i) = CRX or RouterPosition(i) = TR or  RouterPosition(i) = BC or RouterPosition(i) = TC or RouterPosition(i) = CC) generate
	                        rx_cs(i)(s)(WEST)             <= tx_cs(i-1)(s)(EAST);
	                        credit_i_cs(i)(s)(WEST)       <= credit_o_cs(i-1)(s)(EAST);
	                        req_in(i)(s)(WEST)       	  <= req_out(i-1)(s)(EAST);
	                        data_in_cs(i)(s)(WEST)        <= data_out_cs(i-1)(s)(EAST);
	                end generate;
	
	                --- NORTH PORT CONNECTIONS ----------------------------------------------------
	                north_grounding: if RouterPosition(i) = TL or RouterPosition(i) = TC or RouterPosition(i) = TR generate
	                        rx_cs(i)(s)(NORTH)            <= '0';
	                        credit_i_cs(i)(s)(NORTH)      <= '0';
	                        req_in(i)(s)(NORTH)      	  <= '0';
	                        data_in_cs(i)(s)(NORTH)       <= (others => '0');
	                end generate;
	
	                north_connection: if RouterPosition(i) = BL or RouterPosition(i) = BC or RouterPosition(i) = BR or RouterPosition(i) = CL or RouterPosition(i) = CRX or RouterPosition(i) = CC generate
	                        rx_cs(i)(s)(NORTH)            <= tx_cs(i+NUMBER_PROCESSORS_X)(s)(SOUTH);
	                        credit_i_cs(i)(s)(NORTH)      <= credit_o_cs(i+NUMBER_PROCESSORS_X)(s)(SOUTH);
	                        req_in(i)(s)(NORTH)      	  <= req_out(i+NUMBER_PROCESSORS_X)(s)(SOUTH);
	                        data_in_cs(i)(s)(NORTH)       <= data_out_cs(i+NUMBER_PROCESSORS_X)(s)(SOUTH);
	                end generate;
	
	                --- SOUTH PORT CONNECTIONS -----------------------------------------------------
	                south_grounding: if RouterPosition(i) = BL or RouterPosition(i) = BC or RouterPosition(i) = BR generate
	                        rx_cs(i)(s)(SOUTH)            <= '0';
	                        credit_i_cs(i)(s)(SOUTH)      <= '0';
	                        req_in(i)(s)(SOUTH)      	  <= '0';
	                        data_in_cs(i)(s)(SOUTH)       <= (others => '0');
	                end generate;
	
	                south_connection: if RouterPosition(i) = TL or RouterPosition(i) = TC or RouterPosition(i) = TR or RouterPosition(i) = CL or RouterPosition(i) = CRX or RouterPosition(i) = CC generate
	                        rx_cs(i)(s)(SOUTH)            <= tx_cs(i-NUMBER_PROCESSORS_X)(s)(NORTH);
	                        credit_i_cs(i)(s)(SOUTH)      <= credit_o_cs(i-NUMBER_PROCESSORS_X)(s)(NORTH);
	                        req_in(i)(s)(SOUTH)      	  <= req_out(i-NUMBER_PROCESSORS_X)(s)(NORTH);
	                        data_in_cs(i)(s)(SOUTH)       <= data_out_cs(i-NUMBER_PROCESSORS_X)(s)(NORTH);
	                end generate;
	            end generate subnet;
		            
		            
		            
		        ----------------------------------------------------
                -- PACKET SWITCHING NETWORK CONNECTION
                ----------------------------------------------------
                
                --- EAST PORT CONNECTIONS ----------------------------------------------------
                east_grounding: if RouterPosition(i) = BR or RouterPosition(i) = CRX or RouterPosition(i) = TR generate
                        rx_ps(i)(EAST)             <= '0';
                        credit_i_ps(i)(EAST)       <= '0';
                        data_in_ps(i)(EAST)        <= (others => '0');
                end generate;

                east_connection: if RouterPosition(i) = BL or RouterPosition(i) = CL or RouterPosition(i) = TL  or RouterPosition(i) = BC or RouterPosition(i) = TC or RouterPosition(i) = CC generate
                        rx_ps(i)(EAST)             <= tx_ps(i+1)(WEST);
                        credit_i_ps(i)(EAST)       <= credit_o_ps(i+1)(WEST);
                        data_in_ps(i)(EAST)        <= data_out_ps(i+1)(WEST);
                end generate;

                ------------------------------------------------------------------------------
                --- WEST PORT CONNECTIONS ----------------------------------------------------
                west_grounding: if RouterPosition(i) = BL or RouterPosition(i) = CL or RouterPosition(i) = TL generate
                        rx_ps(i)(WEST)             <= '0';
                        credit_i_ps(i)(WEST)       <= '0';
                        data_in_ps(i)(WEST)        <= (others => '0');
                end generate;

                west_connection: if (RouterPosition(i) = BR or RouterPosition(i) = CRX or RouterPosition(i) = TR or  RouterPosition(i) = BC or RouterPosition(i) = TC or RouterPosition(i) = CC) generate
                        rx_ps(i)(WEST)             <= tx_ps(i-1)(EAST);
                        credit_i_ps(i)(WEST)       <= credit_o_ps(i-1)(EAST);
                        data_in_ps(i)(WEST)        <= data_out_ps(i-1)(EAST);
                end generate;

                -------------------------------------------------------------------------------
                --- NORTH PORT CONNECTIONS ----------------------------------------------------
                north_grounding: if RouterPosition(i) = TL or RouterPosition(i) = TC or RouterPosition(i) = TR generate
                        rx_ps(i)(NORTH)            <= '0';
                        credit_i_ps(i)(NORTH)      <= '0';
                        data_in_ps(i)(NORTH)       <= (others => '0');
                end generate;

                north_connection: if RouterPosition(i) = BL or RouterPosition(i) = BC or RouterPosition(i) = BR or RouterPosition(i) = CL or RouterPosition(i) = CRX or RouterPosition(i) = CC generate
                        rx_ps(i)(NORTH)            <= tx_ps(i+NUMBER_PROCESSORS_X)(SOUTH);
                        credit_i_ps(i)(NORTH)      <= credit_o_ps(i+NUMBER_PROCESSORS_X)(SOUTH);
                        data_in_ps(i)(NORTH)       <= data_out_ps(i+NUMBER_PROCESSORS_X)(SOUTH);
                end generate;

                --------------------------------------------------------------------------------
                --- SOUTH PORT CONNECTIONS -----------------------------------------------------
                south_grounding: if RouterPosition(i) = BL or RouterPosition(i) = BC or RouterPosition(i) = BR generate
                        rx_ps(i)(SOUTH)            <= '0';
                        credit_i_ps(i)(SOUTH)      <= '0';
                        data_in_ps(i)(SOUTH)       <= (others => '0');
                end generate;

                south_connection: if RouterPosition(i) = TL or RouterPosition(i) = TC or RouterPosition(i) = TR or RouterPosition(i) = CL or RouterPosition(i) = CRX or RouterPosition(i) = CC generate
                        rx_ps(i)(SOUTH)            <= tx_ps(i-NUMBER_PROCESSORS_X)(NORTH);
                        credit_i_ps(i)(SOUTH)      <= credit_o_ps(i-NUMBER_PROCESSORS_X)(NORTH);
                        data_in_ps(i)(SOUTH)       <= data_out_ps(i-NUMBER_PROCESSORS_X)(NORTH);
                end generate;
                
                
                --IO Wiring (Memphis <-> IO)
                
                app_injector_connection: if i = APP_INJECTOR and io_port(i) /= NPORT generate
                	
                	--IO App Injector connection
           			memphis_app_injector_tx	<=	tx_ps(APP_INJECTOR)(io_port(i));
           			memphis_app_injector_data_out <= data_out_ps(APP_INJECTOR)(io_port(i));
           			credit_i_ps(APP_INJECTOR)(io_port(i)) <= memphis_app_injector_credit_i;
           			
           			rx_ps(APP_INJECTOR)(io_port(i)) <= memphis_app_injector_rx ;
		 			memphis_app_injector_credit_o  <= credit_o_ps(APP_INJECTOR)(io_port(i));
		 			data_in_ps(APP_INJECTOR)(io_port(i)) <= memphis_app_injector_data_in;
		 			
		 		end generate;
		 		
		 		--Insert the IO wiring for your component here:
                
        end generate proc;
           
end architecture;
