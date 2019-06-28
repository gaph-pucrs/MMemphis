------------------------------------------------------------------------------------------------
library ieee;
use work.standards.all;
use work.memphis_pkg.all;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;

entity CS_config is
	port(
		clock, reset   : in  std_logic;
		data_in        : in  regflit;
		rx, credit_o   : in  std_logic;
		wait_header    : out std_logic;
		config_en      : out std_logic;
		config_inport  : out std_logic_vector(2 downto 0);
		config_outport : out std_logic_vector(2 downto 0);
		config_valid   : out std_logic_vector(CS_SUBNETS_NUMBER - 1 downto 0)
	);
end CS_config;

architecture CS_config of CS_config is
	type state is (header, p_size, ppayload, config);
	signal PS                    : state;
	signal payload               : regflit;
	signal en, cfg_period 		: std_logic;
	signal subnet                : std_logic_vector(CS_SUBNETS_NUMBER - 1 downto 0);

begin
	config_valid <= subnet when cfg_period = '1' else (others => '0');

	wait_header <= '1' when PS = header else '0';
	cfg_period  <= '1' when PS = config else '0';

	config_en <= en;

	config_inport  <= data_in(15 downto 13);
	config_outport <= data_in(12 downto 10);
	subnet         <= data_in(CS_SUBNETS_NUMBER downto 1);

	FMS_config : process(clock, reset)
	begin
		if reset = '1' then
			en <= '0';
			PS <= header;

		elsif rising_edge(clock) then
			
			if rx = '1' and credit_o = '1' then
				case PS is
					when header =>
						if data_in(16) = '1' then
							en <= '1';
						end if;
						PS <= p_size;

					when p_size =>
						if en = '1' then
							PS <= config;
						else
							payload <= data_in;
							PS      <= ppayload;
						end if;

					when ppayload =>
						if payload = 1 then
							PS <= header;
						else
							PS      <= ppayload;
							payload <= payload - '1';
						end if;

					when config =>
						en <= '0';
						PS <= header;
				end case;

			end if;
		end if;
	end process;

end architecture;