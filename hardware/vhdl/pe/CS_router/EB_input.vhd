library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.standards.all;

--Elastic Buffer implementation: George Michelogiannakis, James Balfour ; William J. Dally. "Elastic-buffer flow control for on-chip networks" in HPCA 2009

entity EB_input is
	port (
		clock 		: in std_logic;
		reset 		: in std_logic;
		
		valid_in	: in std_logic; --rx
		data_in 	: in regCSflit;
		ready_in	: out std_logic; -- credit_out
		
		valid_out 	: out std_logic; --tx
		data_out 	: out regCSflit;
		ready_out	: in std_logic --credit_in
	);
end entity EB_input;

architecture EB_input of EB_input is
	
	signal slave_latch : regCSflit;
	signal master_latch : regCSflit;
	signal E1, E2, ENS, ENM: std_logic;
	signal n_clock, a_in, b_in, a_m, b_m, a, b: std_logic;
	
begin
	
	master: process(ENM, data_in)
	begin
    	if ENM = '1' then
			  master_latch <= data_in;
		end if;
	end process;
	
	slave: process(ENS, master_latch)
	begin
		if ENS = '1' then
			   slave_latch <= master_latch;		
		end if;
	end process;
	
	data_out <= slave_latch;
	n_clock <= not clock;
	
	ENM <= n_clock and valid_in and E1;
	ENS <= clock and E2;
	
	E1 <= (not a) or b; --Esta diferente do artigo, pois o artigo esta errado
	E2 <= (not a_m) and b_m; --(master latch)
	ready_in <= E1;
	valid_out <= (a or b); --Esta diferente do artigo, pois o artigo esta errado
	
	a_in <= (not ready_out) and (a or b);
	b_in <= (valid_in and ready_out) or  --Esta diferente do artigo, pois o artigo esta errado
		((not a) and (not b) and valid_in) or 
		((not valid_in) and (not ready_out) and b) or 
		(a and (not b) and ready_out);
	
		
	a_master: entity work.latch_d
		port map(
			D => a_in,
			EN => n_clock,
			Q => a_m
		);
	a_slave: entity work.latch_d
		port map(
			D => a_m,
			EN => clock,
			Q => a
		);
		
	b_master: entity work.latch_d
		port map(
			D => b_in,
			EN => n_clock,
			Q => b_m
		);
	b_slave: entity work.latch_d
		port map(
			D => b_m,
			EN => clock,
			Q => b
		);
	
end architecture EB_input;


	

