
library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;
use ieee.numeric_std.all;
use work.standards.all;

entity noc_ps_receiver is
	port (
		clock 			: in std_logic;
		reset 			: in std_logic;
		data_to_memory 	: out std_logic_vector(31 downto 0);
		valid			: out std_logic;
		consume			: in std_logic;
		rx				: in std_logic;
		data_in			: in regflit;
		credit_out		: out std_logic
		
	);
end entity noc_ps_receiver;

architecture noc_ps_receiver of noc_ps_receiver is
	
	type send_buffer is array (1 downto 0) of regflit;
	
	signal data : send_buffer := (others=> (others=>'0'));
	signal full : std_logic_vector(1 downto 0);
	signal head, tail: std_logic_vector(0 downto 0);
	signal shifter_count : std_logic_vector(3 downto 0);
	
begin
	
	credit_out <=  (not full(0)) or (not full(1));
	valid <= full(0) or full(1);
	data_to_memory <= data(conv_integer(head)); 
	
	process(clock, reset)
	begin
		if reset = '1' then
			tail <= "0";
			head <= "0";
			full <= (others=>'0');
		elsif rising_edge(clock) then
			--reads from noc
			if rx = '1' and full( conv_integer(tail) ) = '0' then
				full(conv_integer(tail)) <= '1';
				data(conv_integer(tail)) <= data_in;
				tail <= not tail;
			end if;
			
			--writes to memory
			if consume = '1' and full(conv_integer(head)) = '1' then
				full(conv_integer(head)) <= '0';
				head <= not head;
			end if;
		end if;
		
end process;
	
	
end architecture noc_ps_receiver;