library ieee;
use ieee.std_logic_1164.all;

entity latch_d is
    port ( 
    	D  : in  std_logic;
        EN : in  std_logic;
        Q  : out std_logic
    );
end latch_d;

architecture latch_d of latch_d is
    signal data : std_logic;
begin
	Q <= data;
	process(EN, D)
	begin
		if EN = '1' then
			data <= D;
		end if;
	end process;
end latch_d;