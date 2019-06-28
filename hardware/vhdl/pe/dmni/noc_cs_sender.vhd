
library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;
use ieee.numeric_std.all;
use work.standards.all;
use work.memphis_pkg.all;

entity noc_cs_sender is
	port (
		clock 				: in std_logic;
		reset 				: in std_logic;
		data_from_memory	: in std_logic_vector(31 downto 0); -- from 
		valid				: in std_logic;
		busy				: out std_logic;
		
		-- EB and wormhole flow control interface
		tx 					: out std_logic;
		data_out 			: out regCSflit;
		credit_in			: in std_logic
	);
end entity noc_cs_sender;

architecture noc_cs_sender of noc_cs_sender is
	
	constant zero_padding: regCSflit := (others=>'0');
	constant max_shift: std_logic_vector(3 downto 0) := std_logic_vector(to_unsigned((32/TAM_CS_FLIT), 4));
	subtype mem_word is std_logic_vector(31 downto 0);
	type send_buffer is array (1 downto 0) of mem_word;
	
	signal data : send_buffer := (others=> (others=>'0'));
	signal full : std_logic_vector(1 downto 0);
	signal head, tail: std_logic_vector(0 downto 0);
	signal shifter_count : std_logic_vector(3 downto 0);
	
begin
	
	busy <=  full(0) and full(1);
	tx <= full(0) or full(1);
	data_out <= data(conv_integer(head))((TAM_CS_FLIT-1) downto 0);
	
	process(clock, reset)
	begin
		if reset = '1' then
			tail <= "0";
			head <= "0";
			full <= (others=>'0');
			shifter_count <= max_shift;
		elsif rising_edge(clock) then
			
			--reads from memory
			if valid = '1' and full( conv_integer(tail) ) = '0' then
				full(conv_integer(tail)) <= '1';
				data(conv_integer(tail)) <= data_from_memory;
				tail <= not tail;
			end if;
			
			--write to noc
			if credit_in = '1' and full(conv_integer(head)) = '1' then
				
				data(conv_integer(head)) <= zero_padding & data(conv_integer(head))(31 downto TAM_CS_FLIT );
				
				shifter_count <= shifter_count - 1;
				if shifter_count = 1 then
					shifter_count <= max_shift;
					full(conv_integer(head)) <= '0';
					head <= not head;
				end if;
			end if;
			
		end if;
	end process;
	
	
end architecture noc_cs_sender;